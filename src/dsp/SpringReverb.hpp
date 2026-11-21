// Dispersive allpass chain + 4 parallel damped comb delays simulating the KE-310 spring reverb tank.
// All ring buffers are allocated strictly inside prepare(); processBlock() executes lock-free with zero allocations.
#pragma once

#include "Biquad.hpp"
#include "KE310Params.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace ke310::dsp
{

class AllpassDelay
{
public:
    void prepare(int delaySamples, double g)
    {
        buf_.assign(static_cast<std::size_t>(std::max(2, delaySamples)), 0.0);
        idx_ = 0;
        g_ = g;
    }

    void reset() noexcept
    {
        std::fill(buf_.begin(), buf_.end(), 0.0);
        idx_ = 0;
    }

    void processInPlace(double* buf, int n) noexcept
    {
        const std::size_t len = buf_.size();
        std::size_t idx = idx_;
        const double g = g_;
        for (int i = 0; i < n; ++i)
        {
            const double d = buf_[idx];
            const double v = buf[i] + g * d;
            buf_[idx] = v;
            buf[i] = d - g * v;
            if (++idx >= len)
            {
                idx = 0;
            }
        }
        idx_ = idx;
    }

private:
    std::vector<double> buf_{};
    std::size_t idx_ = 0;
    double g_ = 0.62;
};

class CombDelay
{
public:
    void prepare(int delaySamples, double feedback, double damp)
    {
        buf_.assign(static_cast<std::size_t>(std::max(4, delaySamples)), 0.0);
        idx_ = 0;
        feedback_ = feedback;
        damp_ = damp;
        lpState_ = 0.0;
    }

    void reset() noexcept
    {
        std::fill(buf_.begin(), buf_.end(), 0.0);
        idx_ = 0;
        lpState_ = 0.0;
    }

    void processBlock(const double* in, double* out, int n) noexcept
    {
        const std::size_t len = buf_.size();
        std::size_t idx = idx_;
        const double fb = feedback_;
        const double d1 = damp_;
        const double d0 = 1.0 - d1;
        double lp = lpState_;

        for (int i = 0; i < n; ++i)
        {
            const double delayed = buf_[idx];
            lp = d0 * delayed + d1 * lp;
            buf_[idx] = in[i] + fb * lp;
            out[i] = delayed;
            if (++idx >= len)
            {
                idx = 0;
            }
        }

        idx_ = idx;
        lpState_ = lp;
    }

private:
    std::vector<double> buf_{};
    std::size_t idx_ = 0;
    double feedback_ = 0.72;
    double damp_ = 0.42;
    double lpState_ = 0.0;
};

class SpringReverb
{
public:
    static constexpr int kMaxScratch = 128;

    void prepare(double fs)
    {
        fs_ = fs;
        const double scale = fs_ / 48000.0;

        constexpr std::array<int, 6> kApDelays48k = {23, 37, 53, 79, 113, 157};
        for (std::size_t i = 0; i < allpasses_.size(); ++i)
        {
            const int d = std::max(4, static_cast<int>(std::llround(static_cast<double>(kApDelays48k[i]) * scale)));
            allpasses_[i].prepare(d, 0.62);
        }

        constexpr std::array<int, 4> kCombDelays48k = {1399, 1723, 2111, 2543};
        for (std::size_t i = 0; i < combs_.size(); ++i)
        {
            const int d = std::max(16, static_cast<int>(std::llround(static_cast<double>(kCombDelays48k[i]) * scale)));
            combs_[i].prepare(d, 0.72, 0.42);
        }

        constexpr std::array<FilterSectionSpec, 3> kBandSpecs = {{
            FilterSectionSpec{FilterType::HP2, 210.0, 0.707, 0.0},
            FilterSectionSpec{FilterType::LP2, 3400.0, 0.707, 0.0},
            FilterSectionSpec{FilterType::Peaking, 1150.0, 1.4, 2.5},
        }};
        bandFilter_.design(kBandSpecs, 3, 0.0, fs_);
        reset();
    }

    void reset() noexcept
    {
        for (auto& ap : allpasses_)
        {
            ap.reset();
        }
        for (auto& c : combs_)
        {
            c.reset();
        }
        bandFilter_.reset();
    }

    void processBlock(double* inOut, int n, double amount) noexcept
    {
        if (amount <= 1e-6 || n <= 0)
        {
            return;
        }
        int offset = 0;
        while (offset < n)
        {
            const int chunk = std::min(kMaxScratch, n - offset);
            processSubBlock(inOut + offset, chunk, amount);
            offset += chunk;
        }
    }

private:
    void processSubBlock(double* inOut, int n, double amount) noexcept
    {
        bandFilter_.processBlock(inOut, sigBuf_.data(), n);
        for (auto& ap : allpasses_)
        {
            ap.processInPlace(sigBuf_.data(), n);
        }

        wetBuf_.fill(0.0);
        for (auto& comb : combs_)
        {
            comb.processBlock(sigBuf_.data(), combOut_.data(), n);
            for (int i = 0; i < n; ++i)
            {
                wetBuf_[static_cast<std::size_t>(i)] += combOut_[static_cast<std::size_t>(i)];
            }
        }

        const double wetScale = amount * 0.45;
        for (int i = 0; i < n; ++i)
        {
            const double w = (wetBuf_[static_cast<std::size_t>(i)] * 0.25) * 0.9;
            inOut[i] += wetScale * w;
        }
    }

    double fs_ = 48000.0;
    std::array<AllpassDelay, 6> allpasses_{};
    std::array<CombDelay, 4> combs_{};
    BiquadCascade<3> bandFilter_{};
    std::array<double, kMaxScratch> sigBuf_{};
    std::array<double, kMaxScratch> wetBuf_{};
    std::array<double, kMaxScratch> combOut_{};
};

} // namespace ke310::dsp
