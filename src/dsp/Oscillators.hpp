// PolyBLEP staircase waveform synthesis, KE-310 TOS integer dividers,
// stretched Pedal VCO, and triangle-wave Vibrato LFO with Delay ramp-up.
#pragma once

#include "KE310Params.hpp"

#include <algorithm>
#include <array>
#include <cmath>

namespace ke310::dsp
{

inline double wrap01(double x) noexcept
{
    const double w = x - std::floor(x);
    return (w >= 1.0) ? 0.0 : w;
}

inline double polyblepResidual(double p, double dt) noexcept
{
    if (p < dt)
    {
        const double u = p / dt;
        return u + u - u * u - 1.0;
    }
    if (p > 1.0 - dt)
    {
        const double u = (p - 1.0) / dt;
        return u * u + u + u + 1.0;
    }
    return 0.0;
}

inline double polyblepSquare(double phase, double dt, double duty = 0.5) noexcept
{
    const double p = wrap01(phase);
    const double dtSafe = std::clamp(dt, 1e-7, 0.24);
    const double dutySafe = std::clamp(duty, 0.15, 0.85);
    double sq = (p < dutySafe) ? 1.0 : -1.0;
    sq += polyblepResidual(p, dtSafe);
    const double pFall = wrap01(p + (1.0 - dutySafe));
    sq -= polyblepResidual(pFall, dtSafe);
    const double nyqAtt = std::clamp((0.46 - dt) / 0.06, 0.0, 1.0);
    return sq * nyqAtt;
}

inline double staircaseWave(double phase, double dt, const SourceParams& src, double fs,
                            double bleedSubOct = 0.0, double phaseSub = 0.0) noexcept
{
    const double skewSec = src.dutySkewUs * 1e-6;
    const double duty1 = 0.5 + std::clamp(skewSec * (dt * fs), -0.15, 0.15);
    double out = src.w1 * polyblepSquare(phase, dt, duty1);

    if (std::abs(src.w2) > 1e-6)
    {
        const double dt2 = 2.0 * dt;
        double gain2 = src.w2;
        if (src.top2fCeilHz > 0.0)
        {
            const double f2 = dt2 * fs;
            if (f2 > src.top2fCeilHz)
            {
                const double scale = std::clamp(1.0 - (f2 - src.top2fCeilHz) / 800.0, 0.15, 1.0);
                gain2 = src.w2 * scale;
            }
        }
        const double duty2 = 0.5 + std::clamp(skewSec * (dt2 * fs), -0.15, 0.15);
        out += gain2 * polyblepSquare(2.0 * phase, dt2, duty2);
    }

    if (std::abs(src.w4) > 1e-6)
    {
        out += src.w4 * polyblepSquare(4.0 * phase, 4.0 * dt, 0.5);
    }

    if (std::abs(src.w8) > 1e-6)
    {
        out += src.w8 * polyblepSquare(8.0 * phase, 8.0 * dt, 0.5);
    }

    if (std::abs(bleedSubOct) > 1e-6)
    {
        out += bleedSubOct * polyblepSquare(phaseSub, 0.5 * dt, 0.5);
    }

    return out;
}

class TOSTuningTable
{
public:
    void prepare(double a4Hz = kTuningParams.a4HzSession1, bool vintageTos = true) noexcept
    {
        a4Hz_ = a4Hz;
        vintageTos_ = vintageTos;
        const double divA = static_cast<double>(kTuningParams.tosDividers[9]);

        for (int pc = 0; pc < 12; ++pc)
        {
            const double ideal = a4Hz_ * std::pow(2.0, static_cast<double>(pc - 9) / 12.0);
            if (vintageTos_)
            {
                double raw = a4Hz_ * (divA / static_cast<double>(kTuningParams.tosDividers[static_cast<std::size_t>(pc)]));
                while (raw < ideal * 0.75)
                {
                    raw *= 2.0;
                }
                while (raw > ideal * 1.45)
                {
                    raw *= 0.5;
                }
                pcFreqsOct4_[static_cast<std::size_t>(pc)] = raw;
            }
            else
            {
                pcFreqsOct4_[static_cast<std::size_t>(pc)] = ideal;
            }
        }
    }

    double noteFreq(int midiNote) const noexcept
    {
        const int pc = ((midiNote % 12) + 12) % 12;
        const int octDiff = ((midiNote - pc) / 12) - 5;
        return pcFreqsOct4_[static_cast<std::size_t>(pc)] * std::pow(2.0, static_cast<double>(octDiff));
    }

    double a4Hz() const noexcept { return a4Hz_; }
    bool vintageTos() const noexcept { return vintageTos_; }

private:
    double a4Hz_ = kTuningParams.a4HzSession1;
    bool vintageTos_ = true;
    std::array<double, 12> pcFreqsOct4_{};
};

class PedalVCO
{
public:
    void reset() noexcept
    {
        heldNotes_.fill(false);
    }

    void setConfig(double a4Hz, bool vintageStretch) noexcept
    {
        a4Hz_ = a4Hz;
        vintageStretch_ = vintageStretch;
    }

    void setVintageStretch(bool vintageStretch) noexcept { vintageStretch_ = vintageStretch; }
    bool vintageStretch() const noexcept { return vintageStretch_; }

    int noteOn(int midiNote) noexcept
    {
        if (midiNote >= 0 && midiNote < 128)
        {
            heldNotes_[static_cast<std::size_t>(midiNote)] = true;
        }
        return activeNote();
    }

    int noteOff(int midiNote) noexcept
    {
        if (midiNote >= 0 && midiNote < 128)
        {
            heldNotes_[static_cast<std::size_t>(midiNote)] = false;
        }
        return activeNote();
    }

    int activeNote() const noexcept
    {
        for (int i = 0; i < 128; ++i)
        {
            if (heldNotes_[static_cast<std::size_t>(i)])
            {
                return i;
            }
        }
        return -1;
    }

    double noteFreq(int midiNote) const noexcept
    {
        const double f12Tet = a4Hz_ * std::pow(2.0, static_cast<double>(midiNote - 69) / 12.0);
        if (!vintageStretch_)
        {
            return f12Tet;
        }
        const double cents = kTuningParams.pedalVcoC2Cents
                           + (static_cast<double>(midiNote - 36) / 12.0) * kTuningParams.pedalVcoStretchCentsPerOct;
        return f12Tet * std::pow(2.0, cents / 1200.0);
    }

private:
    double a4Hz_ = 440.0;
    bool vintageStretch_ = true;
    std::array<bool, 128> heldNotes_{};
};

class VibratoLFO
{
public:
    void prepare(double fs) noexcept
    {
        fs_ = fs;
        reset();
    }

    void reset() noexcept
    {
        phase_ = 0.0;
        delayEnv_ = 0.0;
    }

    void triggerDelayRestart() noexcept
    {
        delayEnv_ = 0.0;
    }

    void processBlock(int nSamples, bool vibratoOn, bool vibratoFast, bool delayVibrato,
                      bool anyKeyHeld, double* pitchMultOut, double* ampMultOut) noexcept
    {
        if (!(vibratoOn || delayVibrato))
        {
            delayEnv_ = 0.0;
            for (int i = 0; i < nSamples; ++i)
            {
                pitchMultOut[i] = 1.0;
                ampMultOut[i] = 1.0;
            }
            return;
        }

        double rateHz = 5.348;
        double depthCents = 11.01;
        double amDb = 0.24;
        double rampTauS = 0.005;

        if (vibratoFast)
        {
            rateHz = 6.294;
            depthCents = 8.30;
            amDb = 0.27;
            rampTauS = 0.005;
        }
        else if (delayVibrato)
        {
            rateHz = 5.295;
            depthCents = 16.18;
            amDb = 0.48;
            rampTauS = 0.72;
        }

        const double inc = rateHz / fs_;
        const bool useDelayRamp = (delayVibrato && !vibratoOn);
        const double target = anyKeyHeld ? 1.0 : 0.0;
        const double coeff = useDelayRamp ? std::exp(-1.0 / (rampTauS * fs_)) : 0.0;
        double e = useDelayRamp ? delayEnv_ : 1.0;

        for (int i = 0; i < nSamples; ++i)
        {
            const double ph = wrap01(phase_ + inc * static_cast<double>(i));
            const double tri = 1.0 - 4.0 * std::abs(wrap01(ph + 0.25) - 0.5);

            double effEnv = 1.0;
            if (useDelayRamp)
            {
                e = target + coeff * (e - target);
                effEnv = (rampTauS > 0.1) ? (e * (0.3 + 0.7 * e)) : e;
            }

            const double modCents = tri * depthCents * effEnv;
            pitchMultOut[i] = std::pow(2.0, modCents / 1200.0);
            ampMultOut[i] = std::pow(10.0, (tri * amDb * effEnv) / 20.0);
        }

        phase_ = wrap01(phase_ + inc * static_cast<double>(nSamples));
        delayEnv_ = e;
    }

private:
    double fs_ = 48000.0;
    double phase_ = 0.0;
    double delayEnv_ = 0.0;
};

} // namespace ke310::dsp
