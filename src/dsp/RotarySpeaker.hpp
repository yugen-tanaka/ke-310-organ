// Hornless single-rotor drum rotary speaker DSP (1979 KE-310 single-rotor drum unit).
// All delay memory is allocated strictly in prepare(); processBlock() is lock-free and allocation-free.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

namespace ke310::dsp
{

/** Configuration for the KE-310 built-in hornless rotary drum speaker. */
struct RotaryDrumConfig
{
    bool chorus = false;   ///< Slow rotation lever ("CHORUS")
    bool tremolo = false;  ///< Fast rotation lever ("TREMOLO", overrides chorus when both ON)
    double slowHz = 0.80;  ///< Slow target rotation speed [Hz], clamped to [0.30, 1.80]
    double fastHz = 6.00;  ///< Fast target rotation speed [Hz], clamped to [3.50, 8.50]
};

/**
 * Physical stereo model of a single-rotor hornless rotary drum speaker.
 *
 * Models motor acceleration/coasting/braking inertia, stereo microphone pickup
 * around the drum circumference (4-point cubic Hermite fractional Doppler delay),
 * phase-coherent 180 Hz omnidirectional bass split, directional baffle low-pass
 * shading, directional amplitude modulation, internal cabinet cross-bleed, and
 * bit-exact dry bypass when both levers are OFF and the drum has come to rest.
 */
class RotaryDrumSpeaker
{
public:
    static constexpr double kMinSlowHz = 0.30;
    static constexpr double kMaxSlowHz = 1.80;
    static constexpr double kDefaultSlowHz = 0.80;

    static constexpr double kMinFastHz = 3.50;
    static constexpr double kMaxFastHz = 8.50;
    static constexpr double kDefaultFastHz = 6.00;

    RotaryDrumSpeaker() noexcept
    {
        lnFcRatio_ = std::log(kFcOpenHz / kFcClosedHz);
    }

    /** Allocates the power-of-two delay line for @p sampleRate Hz and resets state. */
    void prepare(double sampleRate)
    {
        fs_ = (sampleRate > 1000.0) ? sampleRate : 48000.0;
        const std::size_t minSamples = static_cast<std::size_t>(std::ceil(0.015 * fs_)) + 8;
        std::size_t pow2Size = 512;
        while (pow2Size < minSamples)
        {
            pow2Size <<= 1;
        }
        delayBuf_.assign(pow2Size, 0.0);
        mask_ = pow2Size - 1;

        const double gBass = std::tan(std::numbers::pi * kBassCrossHz / fs_);
        gBassFactor_ = gBass / (1.0 + gBass);
        lnFcRatio_ = std::log(kFcOpenHz / kFcClosedHz);
        invFs_ = 1.0 / fs_;

        reset();
    }

    /** Clears all delay memory, rotor motion, and filter states. */
    void reset() noexcept
    {
        std::fill(delayBuf_.begin(), delayBuf_.end(), 0.0);
        writeIdx_ = 0;
        speedHz_ = 0.0;
        phaseRad_ = 0.0;
        wetMix_ = 0.0;
        clearChannelStates();
    }

    /** Updates switch states and custom Slow/Fast target speeds safely on the audio thread. */
    void setConfig(const RotaryDrumConfig& cfg) noexcept
    {
        cfg_.chorus = cfg.chorus;
        cfg_.tremolo = cfg.tremolo;
        cfg_.slowHz = std::clamp(cfg.slowHz, kMinSlowHz, kMaxSlowHz);
        cfg_.fastHz = std::clamp(cfg.fastHz, kMinFastHz, kMaxFastHz);
    }

    const RotaryDrumConfig& config() const noexcept { return cfg_; }
    double currentSpeedHz() const noexcept { return speedHz_; }
    double currentWetMix() const noexcept { return wetMix_; }

    /** Returns true once both switches are OFF, drum speed is zero, and wet crossfade finished. */
    bool isBypassed() const noexcept
    {
        return !cfg_.chorus && !cfg_.tremolo && speedHz_ <= 0.0 && wetMix_ <= 0.0;
    }

    /**
     * Renders @p numSamples of mono drum input into stereo @p outL / @p outR.
     * Supports float or double buffers; @p outL or @p outR may alias @p inMono safely
     * because the current sample is read before writing @p outL and @p outR.
     */
    template <typename SampleT>
    void processBlock(int numSamples, const SampleT* inMono, SampleT* outL, SampleT* outR) noexcept
    {
        if (numSamples <= 0 || delayBuf_.empty())
        {
            return;
        }

        const bool active = cfg_.chorus || cfg_.tremolo;
        const double targetHz = cfg_.tremolo ? cfg_.fastHz : (cfg_.chorus ? cfg_.slowHz : 0.0);

        const double cAccel = std::exp(-invFs_ / kTauAccelS);
        const double cDecelSlow = std::exp(-invFs_ / kTauDecelSlowS);
        const double cBrake = std::exp(-invFs_ / kTauBrakeS);
        const double cWetIn = std::exp(-invFs_ / kTauWetInS);
        const double cWetOut = std::exp(-invFs_ / kTauWetOutS);
        const double phaseScale = 2.0 * std::numbers::pi * invFs_;
        constexpr double kTwoPi = 2.0 * std::numbers::pi;

        for (int i = 0; i < numSamples; ++i)
        {
            const SampleT rawSample = inMono[i];
            const double x = static_cast<double>(rawSample);

            // Always keep the delay ring buffer fed so engaging a switch mid-note has warm history.
            const std::size_t wIdx = writeIdx_;
            delayBuf_[wIdx] = x;
            writeIdx_ = (wIdx + 1) & mask_;

            if (!active && speedHz_ <= 0.0 && wetMix_ <= 0.0)
            {
                outL[i] = rawSample;
                outR[i] = rawSample;
                continue;
            }

            // Smooth asymmetric motor inertia (acceleration vs coast-down vs braking)
            double cSpeed = cBrake;
            if (targetHz > speedHz_)
            {
                cSpeed = cAccel;
            }
            else if (targetHz > 0.05)
            {
                cSpeed = cDecelSlow;
            }
            speedHz_ = targetHz + cSpeed * (speedHz_ - targetHz);

            if (!active && speedHz_ < kStopSnapHz)
            {
                speedHz_ = 0.0;
            }

            phaseRad_ += phaseScale * speedHz_;
            if (phaseRad_ >= kTwoPi)
            {
                phaseRad_ -= kTwoPi;
            }

            // Hold 100% wet while braking until the drum actually reaches rest
            const double targetWet = (active || speedHz_ > 0.0) ? 1.0 : 0.0;
            const double cWet = (targetWet > wetMix_) ? cWetIn : cWetOut;
            wetMix_ = targetWet + cWet * (wetMix_ - targetWet);

            if (targetWet == 0.0 && wetMix_ < 1.0e-5)
            {
                wetMix_ = 0.0;
                clearChannelStates();
                outL[i] = rawSample;
                outR[i] = rawSample;
                continue;
            }
            if (targetWet == 1.0 && wetMix_ > 1.0 - 1.0e-5)
            {
                wetMix_ = 1.0;
            }

            const double directL = processChannel(chL_, phaseRad_ - kMicAzimuthRad, wIdx);
            const double directR = processChannel(chR_, phaseRad_ + kMicAzimuthRad, wIdx);

            const double wetL = kMakeupGain * ((1.0 - kCrossBleed) * directL + kCrossBleed * directR);
            const double wetR = kMakeupGain * ((1.0 - kCrossBleed) * directR + kCrossBleed * directL);

            outL[i] = static_cast<SampleT>(x + wetMix_ * (wetL - x));
            outR[i] = static_cast<SampleT>(x + wetMix_ * (wetR - x));
        }
    }

private:
    struct ChannelState
    {
        double lpBass = 0.0;
        double lpBaffle = 0.0;
    };

    static constexpr double kTauAccelS = 0.90;
    static constexpr double kTauDecelSlowS = 1.85;
    static constexpr double kTauBrakeS = 1.40;
    static constexpr double kStopSnapHz = 0.015;
    static constexpr double kTauWetInS = 0.030;
    static constexpr double kTauWetOutS = 0.080;

    static constexpr double kBaseDelayS = 0.0020;
    static constexpr double kRadiusM = 0.145;
    static constexpr double kSpeedOfSoundMps = 343.0;
    static constexpr double kMicAzimuthRad = 55.0 * (std::numbers::pi / 180.0);
    static constexpr double kBassCrossHz = 180.0;
    static constexpr double kFcClosedHz = 680.0;
    static constexpr double kFcOpenHz = 5200.0;
    static constexpr double kCrossBleed = 0.16;
    static constexpr double kMakeupGain = 1.06;

    void clearChannelStates() noexcept
    {
        chL_ = {};
        chR_ = {};
    }

    double readCubicHermite(std::size_t wIdx, double delaySamples) const noexcept
    {
        const int dInt = static_cast<int>(std::floor(delaySamples));
        const double frac = delaySamples - static_cast<double>(dInt);
        auto at = [&](int lag) noexcept -> double
        {
            return delayBuf_[(wIdx - static_cast<std::size_t>(lag)) & mask_];
        };

        const double ym1 = at(dInt - 1);
        const double y0 = at(dInt);
        const double y1 = at(dInt + 1);
        const double y2 = at(dInt + 2);

        const double c0 = y0;
        const double c1 = 0.5 * (y1 - ym1);
        const double c2 = ym1 - 2.5 * y0 + 2.0 * y1 - 0.5 * y2;
        const double c3 = 0.5 * (y2 - ym1) + 1.5 * (y0 - y1);
        return ((c3 * frac + c2) * frac + c1) * frac + c0;
    }

    double processChannel(ChannelState& ch, double alpha, std::size_t wIdx) const noexcept
    {
        const double c = std::cos(alpha);
        const double u = 0.5 * (1.0 + c);

        // Acoustic Doppler propagation delay around rotor radius
        const double dSamp = std::max(2.0, (kBaseDelayS - (kRadiusM / kSpeedOfSoundMps) * c) * fs_);
        const double del = readCubicHermite(wIdx, dSamp);

        // Phase-coherent 180 Hz complementary TPT 1-pole split after Doppler delay
        const double vB = (del - ch.lpBass) * gBassFactor_;
        const double bass = vB + ch.lpBass;
        ch.lpBass = bass + vB;
        const double midHi = del - bass;

        // Directional rotor aperture baffle shading on mid/high band
        const double fc = std::min(0.42 * fs_, kFcClosedHz * std::exp(u * lnFcRatio_));
        const double gBaf = std::tan(std::numbers::pi * fc * invFs_);
        const double vF = (midHi - ch.lpBaffle) * (gBaf / (1.0 + gBaf));
        const double baffleMidHi = vF + ch.lpBaffle;
        ch.lpBaffle = baffleMidHi + vF;

        // Directional amplitude modulation with omnidirectional low-end reinforcement
        const double gBassAmp = 0.94 + 0.08 * c;
        const double gMidHiAmp = 0.76 + 0.28 * c;
        return gBassAmp * bass + gMidHiAmp * baffleMidHi;
    }

    double fs_ = 48000.0;
    double invFs_ = 1.0 / 48000.0;
    double gBassFactor_ = 0.0116;
    double lnFcRatio_ = 2.034;

    RotaryDrumConfig cfg_{};
    std::vector<double> delayBuf_{};
    std::size_t mask_ = 0;
    std::size_t writeIdx_ = 0;

    double speedHz_ = 0.0;
    double phaseRad_ = 0.0;
    double wetMix_ = 0.0;
    ChannelState chL_{};
    ChannelState chR_{};
};

} // namespace ke310::dsp
