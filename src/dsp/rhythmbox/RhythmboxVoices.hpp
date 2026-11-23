// Pure C++20 zero-allocation voice circuits for KE Rhythmbox (exact Phase R2.6 Python parity).
#pragma once

#include "RhythmboxParams.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace ke310::rhythmbox
{

class Xorshift32Noise
{
public:
    explicit Xorshift32Noise(std::uint32_t seed = 0x12345678u) noexcept
        : initSeed_(seed != 0u ? seed : 0x12345678u)
        , state_(initSeed_)
    {
    }

    void reset(std::uint32_t seed) noexcept
    {
        state_ = (seed != 0u) ? seed : initSeed_;
    }

    void reset() noexcept
    {
        state_ = initSeed_;
    }

    double nextScalar() noexcept
    {
        state_ ^= (state_ << 13u);
        state_ ^= (state_ >> 17u);
        state_ ^= (state_ << 5u);
        constexpr double kInvScale = 1.0 / 2147483648.0;
        return static_cast<double>(static_cast<std::int32_t>(state_)) * kInvScale;
    }

    void nextAvalancheBlock(double* out, int n, double alpha) noexcept
    {
        if (n <= 0)
        {
            return;
        }
        if (alpha <= 1e-6)
        {
            const double scale = std::sqrt(3.0);
            for (int i = 0; i < n; ++i)
            {
                out[i] = nextScalar() * scale;
            }
            return;
        }

        // Exact moments of u = u1 + u2 + u3 (each ~Uniform(-1,1)):
        // E[u^2] = 1.0, E[u^4] = 38/15, E[u^6] = 205/21
        const double norm = 1.0 / std::sqrt(1.0 + (76.0 / 15.0) * alpha + (205.0 / 21.0) * alpha * alpha);
        for (int i = 0; i < n; ++i)
        {
            const double u = nextScalar() + nextScalar() + nextScalar();
            out[i] = u * (1.0 + alpha * u * u) * norm;
        }
    }

private:
    std::uint32_t initSeed_ = 0x12345678u;
    std::uint32_t state_ = 0x12345678u;
};

inline double cytomicSvfPoleRadius(double fHz, double tauDecS, double fs) noexcept
{
    const double fcMax = 0.45 * fs;
    const double fClamped = std::min(fcMax, std::max(5.0, fHz));
    const double q = std::max(0.7, std::numbers::pi * fClamped * std::max(4e-4, tauDecS));
    const double g = std::tan(std::numbers::pi * fClamped / fs);
    const double k = 1.0 / q;
    const double det = (1.0 + g * (g - k)) / (1.0 + g * (g + k));
    return std::sqrt(std::max(0.0, det));
}

class BridgedTDampedVoice
{
public:
    void prepare(double fs, const BridgedTSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        reset();
    }

    void reset() noexcept
    {
        ic1_ = 0.0;
        ic2_ = 0.0;
        pF_ = 0.0;
        pR_ = 0.0;
        hpState_ = 0.0;
        ageS_ = 100.0;
        crackAgeS_ = 100.0;
        trigVel_ = 0.0;
        pitchMult_ = 1.0;
        decayMult_ = 1.0;
    }

    void trigger(double velocity = 1.0, double pitchMult = 1.0, double decayMult = 1.0) noexcept
    {
        const double vel = std::max(0.0, velocity);
        ageS_ = 0.0;
        crackAgeS_ = 0.0;
        trigVel_ = vel;
        pitchMult_ = pitchMult;
        decayMult_ = std::max(0.2, decayMult);
        // Accumulate smooth bi-exponential excitation so rapid rolls superimpose continuously
        pF_ += vel;
        pR_ += vel;
    }

    void processBlock(double* out, int n) noexcept
    {
        if (n <= 0)
        {
            return;
        }

        const double tauCkS = std::max(4e-5, spec_.tauCrackMs * 1e-3);
        const double crackMaxS = (spec_.crackAmp != 0.0) ? (tauCkS * 14.0) : 0.0;

        if (crackAgeS_ >= crackMaxS &&
            std::abs(ic1_) + std::abs(ic2_) + std::abs(pF_) + std::abs(pR_) + std::abs(hpState_) < 1e-15)
        {
            std::fill_n(out, n, 0.0);
            return;
        }

        const double dt = 1.0 / fs_;
        const double fBase = spec_.freqHz;
        const double dropRatio = spec_.pitchDropRatio;
        const double tauDropS = std::max(1e-4, spec_.tauDropMs * 1e-3);
        const double tauDecS = std::max(4e-4, spec_.tauDecayMs * 1e-3) * decayMult_;
        const double tauRiseS = std::max(1e-5, spec_.tauRiseMs * 1e-3);
        const double tauFallS = std::max(tauRiseS * 1.15, spec_.tauFallMs * 1e-3);
        const double rTf = tauRiseS / tauFallS;
        const double uPeak = (1.0 - rTf) * std::pow(rTf, rTf / (1.0 - rTf));
        const double uScale = 1.0 / std::max(uPeak, 1e-6);

        const double feedGain = spec_.feedGain;
        const double hpFeedHz = spec_.hpFeedHz;
        const double h2Ratio = spec_.h2Ratio;
        const double crackAmp = spec_.crackAmp;
        const double crackFreqHz = spec_.crackFreqHz;
        const double crackPhase = spec_.crackPhaseRad;
        const double gain = spec_.gain;

        const double cF = std::exp(-dt / tauFallS);
        const double cR = std::exp(-dt / tauRiseS);
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double cHp = (hpFeedHz > 0.0) ? std::exp(-kTwoPi * hpFeedHz * dt) : 0.0;
        const double fcMax = 0.45 * fs_;

        const bool hasDrop = std::abs(dropRatio) > 1e-6;
        double a1Fixed = 0.0;
        double a2Fixed = 0.0;
        double a3Fixed = 0.0;
        double nlScaleFixed = 0.0;

        if (!hasDrop)
        {
            const double fFixed = std::min(fcMax, fBase * pitchMult_);
            const double qFixed = std::max(0.7, std::numbers::pi * fFixed * tauDecS);
            const double gFixed = std::tan(std::numbers::pi * fFixed * dt);
            const double kFixed = 1.0 / qFixed;
            a1Fixed = 1.0 / (1.0 + gFixed * (gFixed + kFixed));
            a2Fixed = gFixed * a1Fixed;
            a3Fixed = gFixed * a2Fixed;
            nlScaleFixed = 2.0 * h2Ratio / std::max(1.0, qFixed * 0.25);
        }

        double ic1 = ic1_;
        double ic2 = ic2_;
        double pF = pF_;
        double pR = pR_;
        double hpSt = hpState_;
        double t = ageS_;
        double tc = crackAgeS_;
        const double vel = trigVel_;

        for (int i = 0; i < n; ++i)
        {
            pF *= cF;
            pR *= cR;
            const double u = (pF - pR) * uScale;
            double hpOut = u;
            if (hpFeedHz > 0.0)
            {
                hpOut = u - hpSt;
                hpSt = u + cHp * (hpSt - u);
            }

            double a1 = a1Fixed;
            double a2 = a2Fixed;
            double a3 = a3Fixed;
            double nlS = nlScaleFixed;

            if (hasDrop)
            {
                const double fInst = std::min(fcMax, fBase * pitchMult_ * (1.0 + dropRatio * std::exp(-t / tauDropS)));
                const double qCur = std::max(0.7, std::numbers::pi * fInst * tauDecS);
                const double gCur = std::tan(std::numbers::pi * fInst * dt);
                const double kCur = 1.0 / qCur;
                a1 = 1.0 / (1.0 + gCur * (gCur + kCur));
                a2 = gCur * a1;
                a3 = gCur * a2;
                nlS = 2.0 * h2Ratio / std::max(1.0, qCur * 0.25);
            }

            const double v3 = u - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;

            double val = v1 + feedGain * hpOut;
            if (h2Ratio != 0.0)
            {
                val += nlS * (v1 * v2);
            }
            if (crackAmp != 0.0 && tc < crackMaxS)
            {
                val += vel * crackAmp * std::exp(-tc / tauCkS) * std::sin(kTwoPi * crackFreqHz * tc + crackPhase);
            }
            out[i] = gain * val;
            t += dt;
            tc += dt;
        }

        ic1_ = ic1;
        ic2_ = ic2;
        pF_ = pF;
        pR_ = pR;
        hpState_ = hpSt;
        ageS_ = t;
        crackAgeS_ = tc;
    }

    double maxPoleRadius() const noexcept
    {
        const double tauDecS = std::max(4e-4, spec_.tauDecayMs * 1e-3);
        const double rBase = cytomicSvfPoleRadius(spec_.freqHz, tauDecS, fs_);
        const double rPeak = cytomicSvfPoleRadius(spec_.freqHz * (1.0 + spec_.pitchDropRatio), tauDecS, fs_);
        return std::max(rBase, rPeak);
    }

private:
    double fs_ = 48000.0;
    BridgedTSpec spec_{};
    double ic1_ = 0.0;
    double ic2_ = 0.0;
    double pF_ = 0.0;
    double pR_ = 0.0;
    double hpState_ = 0.0;
    double ageS_ = 100.0;
    double crackAgeS_ = 100.0;
    double trigVel_ = 0.0;
    double pitchMult_ = 1.0;
    double decayMult_ = 1.0;
};

class DualResonatorVoice
{
public:
    void prepare(double fs, const DualResonatorSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        reset();
    }

    void reset() noexcept
    {
        ic1A_ = 0.0;
        ic2A_ = 0.0;
        ic1B_ = 0.0;
        ic2B_ = 0.0;
        pF_ = 0.0;
        pR_ = 0.0;
        hpState_ = 0.0;
        nlDcState_ = 0.0;
        ageS_ = 100.0;
        trigVel_ = 0.0;
        pitchMult_ = 1.0;
        decayMult_ = 1.0;
    }

    void trigger(double velocity = 1.0, double pitchMult = 1.0, double decayMult = 1.0) noexcept
    {
        const double vel = std::max(0.0, velocity);
        ageS_ = 0.0;
        trigVel_ = vel;
        pitchMult_ = pitchMult;
        decayMult_ = std::max(0.2, decayMult);
        pF_ += vel;
        pR_ += vel;
    }

    void processBlock(double* out, int n) noexcept
    {
        if (n <= 0)
        {
            return;
        }

        if (std::abs(ic1A_) + std::abs(ic2A_) + std::abs(ic1B_) + std::abs(ic2B_) +
                std::abs(pF_) + std::abs(pR_) + std::abs(hpState_) + std::abs(nlDcState_) <
            1e-15)
        {
            std::fill_n(out, n, 0.0);
            return;
        }

        const double dt = 1.0 / fs_;
        const double fcMax = 0.45 * fs_;
        const double f1 = std::min(fcMax, spec_.freq1Hz * pitchMult_);
        const double f2 = std::min(fcMax, spec_.freq2Hz * pitchMult_);
        const double tau1S = std::max(5e-4, spec_.tau1Ms * 1e-3) * decayMult_;
        const double tau2S = std::max(5e-4, spec_.tau2Ms * 1e-3) * decayMult_;
        const double tauRiseS = std::max(2e-5, spec_.tauRiseMs * 1e-3);
        const double tauFallS = std::max(tauRiseS * 1.15, spec_.tauFallMs * 1e-3);
        const double rTf = tauRiseS / tauFallS;
        const double uScale = 1.0 / std::max((1.0 - rTf) * std::pow(rTf, rTf / (1.0 - rTf)), 1e-6);

        const double amp1 = spec_.amp1;
        const double amp2 = spec_.amp2;
        const double mode2Sign = spec_.mode2Sign;
        const double hpInHz = spec_.hpInHz;
        const double asymNl = spec_.asymNl;
        const double dropRatio = spec_.pitchDropRatio;
        const double tauDropS = std::max(1e-4, spec_.tauDropMs * 1e-3);
        const double gain = spec_.gain;

        const double cF = std::exp(-dt / tauFallS);
        const double cR = std::exp(-dt / tauRiseS);
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double cHp = (hpInHz > 0.0) ? std::exp(-kTwoPi * hpInHz * dt) : 0.0;
        const double cDc = std::exp(-kTwoPi * 150.0 * dt);

        const bool hasDrop = std::abs(dropRatio) > 1e-6;
        double a1AFx = 0.0, a2AFx = 0.0, a3AFx = 0.0;
        double a1BFx = 0.0, a2BFx = 0.0, a3BFx = 0.0;

        if (!hasDrop)
        {
            const double qA = std::max(0.7, std::numbers::pi * f1 * tau1S);
            const double gA = std::tan(std::numbers::pi * f1 * dt);
            a1AFx = 1.0 / (1.0 + gA * (gA + 1.0 / qA));
            a2AFx = gA * a1AFx;
            a3AFx = gA * a2AFx;

            const double qB = std::max(0.7, std::numbers::pi * f2 * tau2S);
            const double gB = std::tan(std::numbers::pi * f2 * dt);
            a1BFx = 1.0 / (1.0 + gB * (gB + 1.0 / qB));
            a2BFx = gB * a1BFx;
            a3BFx = gB * a2BFx;
        }

        double ic1A = ic1A_, ic2A = ic2A_;
        double ic1B = ic1B_, ic2B = ic2B_;
        double pF = pF_;
        double pR = pR_;
        double hpSt = hpState_;
        double dcSt = nlDcState_;
        double t = ageS_;

        for (int i = 0; i < n; ++i)
        {
            pF *= cF;
            pR *= cR;
            double u = (pF - pR) * uScale;
            if (hpInHz > 0.0)
            {
                const double uh = u - hpSt;
                hpSt = u + cHp * (hpSt - u);
                u = uh;
            }

            double a1A = a1AFx, a2A = a2AFx, a3A = a3AFx;
            double a1B = a1BFx, a2B = a2BFx, a3B = a3BFx;

            if (hasDrop)
            {
                const double mul = 1.0 + dropRatio * std::exp(-t / tauDropS);
                const double f1Cur = std::min(fcMax, f1 * mul);
                const double qA = std::max(0.7, std::numbers::pi * f1Cur * tau1S);
                const double gA = std::tan(std::numbers::pi * f1Cur * dt);
                a1A = 1.0 / (1.0 + gA * (gA + 1.0 / qA));
                a2A = gA * a1A;
                a3A = gA * a2A;

                const double f2Cur = std::min(fcMax, f2 * mul);
                const double qB = std::max(0.7, std::numbers::pi * f2Cur * tau2S);
                const double gB = std::tan(std::numbers::pi * f2Cur * dt);
                a1B = 1.0 / (1.0 + gB * (gB + 1.0 / qB));
                a2B = gB * a1B;
                a3B = gB * a2B;
            }

            const double v3A = u - ic2A;
            const double v1A = a1A * ic1A + a2A * v3A;
            const double v2A = ic2A + a2A * ic1A + a3A * v3A;
            ic1A = 2.0 * v1A - ic1A;
            ic2A = 2.0 * v2A - ic2A;

            const double v3B = u - ic2B;
            const double v1B = a1B * ic1B + a2B * v3B;
            const double v2B = ic2B + a2B * ic1B + a3B * v3B;
            ic1B = 2.0 * v1B - ic1B;
            ic2B = 2.0 * v2B - ic2B;

            double val = amp1 * v1A + mode2Sign * amp2 * v1B;
            if (asymNl != 0.0)
            {
                const double nlRaw = asymNl * (val * val);
                const double nlAc = nlRaw - dcSt;
                dcSt = nlRaw + cDc * (dcSt - nlRaw);
                val += nlAc;
            }

            out[i] = gain * val;
            t += dt;
        }

        ic1A_ = ic1A;
        ic2A_ = ic2A;
        ic1B_ = ic1B;
        ic2B_ = ic2B;
        pF_ = pF;
        pR_ = pR;
        hpState_ = hpSt;
        nlDcState_ = dcSt;
        ageS_ = t;
    }

    double maxPoleRadius() const noexcept
    {
        const double t1 = std::max(5e-4, spec_.tau1Ms * 1e-3);
        const double t2 = std::max(5e-4, spec_.tau2Ms * 1e-3);
        const double r1 = cytomicSvfPoleRadius(spec_.freq1Hz * (1.0 + spec_.pitchDropRatio), t1, fs_);
        const double r2 = cytomicSvfPoleRadius(spec_.freq2Hz * (1.0 + spec_.pitchDropRatio), t2, fs_);
        return std::max(r1, r2);
    }

private:
    double fs_ = 48000.0;
    DualResonatorSpec spec_{};
    double ic1A_ = 0.0;
    double ic2A_ = 0.0;
    double ic1B_ = 0.0;
    double ic2B_ = 0.0;
    double pF_ = 0.0;
    double pR_ = 0.0;
    double hpState_ = 0.0;
    double nlDcState_ = 0.0;
    double ageS_ = 100.0;
    double trigVel_ = 0.0;
    double pitchMult_ = 1.0;
    double decayMult_ = 1.0;
};

class GatedNoiseVCA
{
public:
    void prepare(double fs, const GatedVcaSpec& spec) noexcept
    {
        fs_ = fs;
        spec_ = spec;
        reset();
    }

    void reset() noexcept
    {
        rc_ = 0.0;
        ageS_ = 100.0;
        pingAgeS_ = 100.0;
        trigVel_ = 0.0;
        decayMult_ = 1.0;
        pingScale_ = 1.0;
        clkF_ = 0.0;
        clkR_ = 0.0;
        clkHp_ = 0.0;
    }

    void trigger(double velocity = 1.0, double decayMult = 1.0) noexcept
    {
        // Keep rc and CV click states continuous for authentic recharge/retrigger physics
        const double prevRc = std::clamp(rc_, 0.0, 1.0);
        const double vel = std::max(0.0, velocity);
        pingScale_ = 1.0 + spec_.hotPingBoost * prevRc;
        const double clkScale = 1.0 + spec_.hotClickBoost * prevRc;
        clkF_ += vel * clkScale;
        clkR_ += vel * clkScale;
        ageS_ = 0.0;
        pingAgeS_ = 0.0;
        trigVel_ = vel;
        decayMult_ = std::max(0.2, decayMult);
    }

    void renderEnvelopePingClick(double* envOut, double* pingOut, double* clickOut, int n) noexcept
    {
        if (n <= 0)
        {
            return;
        }

        const double gateS = std::max(1e-3, spec_.gateMs * 1e-3) * decayMult_;
        const double pingAmp = spec_.pingAmp;
        const double tauPingS = std::max(3e-5, spec_.tauPingMs * 1e-3);
        const double pingFreqHz = spec_.pingFreqHz;
        const double spikeAmp = spec_.spikeAmp;
        const double spikeTauS = std::max(2e-4, spec_.spikeTauMs * 1e-3);
        const double pingMaxS = (pingAmp != 0.0) ? (tauPingS * 16.0) : 0.0;

        const double cvClickAmp = spec_.cvClickAmp;
        const double tauCrS = std::max(1e-5, spec_.tauClickRiseMs * 1e-3);
        const double tauCfS = std::max(tauCrS * 1.15, spec_.tauClickFallMs * 1e-3);
        const double clickHpHz = spec_.clickHpHz;

        if (ageS_ >= gateS && rc_ < 1e-12 && pingAgeS_ >= pingMaxS &&
            (cvClickAmp == 0.0 || std::abs(clkF_) + std::abs(clkR_) + std::abs(clkHp_) < 1e-15))
        {
            std::fill_n(envOut, n, 0.0);
            std::fill_n(pingOut, n, 0.0);
            if (clickOut != nullptr)
            {
                std::fill_n(clickOut, n, 0.0);
            }
            return;
        }

        const double dt = 1.0 / fs_;
        const double droopTauS = std::max(5e-3, spec_.holdDroopMs * 1e-3);
        const double tauAtkS = std::max(5e-5, spec_.tauAttackMs * 1e-3);
        const double tauRelS = std::max(1e-3, spec_.tauReleaseMs * 1e-3) * decayMult_;
        const double satDrive = std::max(0.2, spec_.vcaTanhDrive);
        const double gain = spec_.gain;

        const double cAtk = std::exp(-dt / tauAtkS);
        const double cRel = std::exp(-dt / tauRelS);
        const double td = std::tanh(satDrive);
        constexpr double kTwoPi = 2.0 * std::numbers::pi;
        const double spikeMaxS = (spikeAmp > 0.0) ? (spikeTauS * 14.0) : 0.0;

        const double rCk = tauCrS / tauCfS;
        const double ckScale = 1.0 / std::max((1.0 - rCk) * std::pow(rCk, rCk / (1.0 - rCk)), 1e-6);
        const double cCf = std::exp(-dt / tauCfS);
        const double cCr = std::exp(-dt / tauCrS);
        const double cChp = (clickHpHz > 0.0) ? std::exp(-kTwoPi * clickHpHz * dt) : 0.0;

        double t = ageS_;
        double tp = pingAgeS_;
        double rc = rc_;
        const double vel = trigVel_;
        const double pScale = pingScale_;
        double clkF = clkF_;
        double clkR = clkR_;
        double clkHp = clkHp_;
        const bool hasClick = (cvClickAmp != 0.0);

        for (int i = 0; i < n; ++i)
        {
            if (t < gateS)
            {
                const double target = vel * std::exp(-t / droopTauS);
                rc = target + cAtk * (rc - target);
            }
            else
            {
                rc *= cRel;
            }

            double shaped = std::tanh(satDrive * std::max(0.0, rc)) / td;
            if (spikeAmp > 0.0 && t < spikeMaxS)
            {
                shaped += vel * spikeAmp * std::exp(-t / spikeTauS);
            }
            envOut[i] = gain * shaped;

            if (tp < pingMaxS)
            {
                pingOut[i] = pingAmp * pScale * vel * std::exp(-tp / tauPingS) * std::sin(kTwoPi * pingFreqHz * tp);
            }
            else
            {
                pingOut[i] = 0.0;
            }

            if (hasClick)
            {
                clkF *= cCf;
                clkR *= cCr;
                const double uC = -(clkF - clkR) * ckScale;
                double clkAc = uC;
                if (clickHpHz > 0.0)
                {
                    clkAc = uC - clkHp;
                    clkHp = uC + cChp * (clkHp - uC);
                }
                if (clickOut != nullptr)
                {
                    clickOut[i] = cvClickAmp * clkAc;
                }
            }
            else if (clickOut != nullptr)
            {
                clickOut[i] = 0.0;
            }

            t += dt;
            tp += dt;
        }

        ageS_ = t;
        pingAgeS_ = tp;
        rc_ = rc;
        clkF_ = clkF;
        clkR_ = clkR;
        clkHp_ = clkHp;
    }

private:
    double fs_ = 48000.0;
    GatedVcaSpec spec_{};
    double rc_ = 0.0;
    double ageS_ = 100.0;
    double pingAgeS_ = 100.0;
    double trigVel_ = 0.0;
    double decayMult_ = 1.0;
    double pingScale_ = 1.0;
    double clkF_ = 0.0;
    double clkR_ = 0.0;
    double clkHp_ = 0.0;
};

} // namespace ke310::rhythmbox
