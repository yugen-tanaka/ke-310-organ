// Sample-rate-independent RBJ biquad cascades (DF2T) and Cytomic TPT/ZDF SVF filter.
#pragma once

#include "FilterTypes.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>

namespace ke310::dsp
{

struct BiquadCoeffs
{
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
};

inline BiquadCoeffs designSection(const FilterSectionSpec& spec, double fs) noexcept
{
    const double fcSafe = std::clamp(spec.freqHz, 5.0, 0.475 * fs);
    const double q = std::max(0.05, spec.q);

    if (spec.type == FilterType::LP1)
    {
        const double k = std::tan(std::numbers::pi * fcSafe / fs);
        const double norm = 1.0 / (1.0 + k);
        return BiquadCoeffs{k * norm, k * norm, 0.0, (k - 1.0) * norm, 0.0};
    }
    if (spec.type == FilterType::HP1)
    {
        const double k = std::tan(std::numbers::pi * fcSafe / fs);
        const double norm = 1.0 / (1.0 + k);
        return BiquadCoeffs{norm, -norm, 0.0, (k - 1.0) * norm, 0.0};
    }

    const double w0 = 2.0 * std::numbers::pi * fcSafe / fs;
    const double cosW0 = std::cos(w0);
    const double sinW0 = std::sin(w0);
    const double alpha = sinW0 / (2.0 * q);

    double b0 = 1.0, b1 = 0.0, b2 = 0.0;
    double a0 = 1.0, a1 = 0.0, a2 = 0.0;

    switch (spec.type)
    {
        case FilterType::LP2:
            b0 = 0.5 * (1.0 - cosW0);
            b1 = 1.0 - cosW0;
            b2 = 0.5 * (1.0 - cosW0);
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::HP2:
            b0 = 0.5 * (1.0 + cosW0);
            b1 = -(1.0 + cosW0);
            b2 = 0.5 * (1.0 + cosW0);
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::BP2:
            b0 = alpha;
            b1 = 0.0;
            b2 = -alpha;
            a0 = 1.0 + alpha;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha;
            break;

        case FilterType::Peaking:
        {
            const double aLin = std::pow(10.0, spec.gainDb / 40.0);
            b0 = 1.0 + alpha * aLin;
            b1 = -2.0 * cosW0;
            b2 = 1.0 - alpha * aLin;
            a0 = 1.0 + alpha / aLin;
            a1 = -2.0 * cosW0;
            a2 = 1.0 - alpha / aLin;
            break;
        }

        case FilterType::LowShelf:
        {
            const double aLin = std::pow(10.0, spec.gainDb / 40.0);
            const double twoSqrtAAlpha = 2.0 * std::sqrt(aLin) * alpha;
            b0 = aLin * ((aLin + 1.0) - (aLin - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = 2.0 * aLin * ((aLin - 1.0) - (aLin + 1.0) * cosW0);
            b2 = aLin * ((aLin + 1.0) - (aLin - 1.0) * cosW0 - twoSqrtAAlpha);
            a0 = (aLin + 1.0) + (aLin - 1.0) * cosW0 + twoSqrtAAlpha;
            a1 = -2.0 * ((aLin - 1.0) + (aLin + 1.0) * cosW0);
            a2 = (aLin + 1.0) + (aLin - 1.0) * cosW0 - twoSqrtAAlpha;
            break;
        }

        case FilterType::HighShelf:
        {
            const double aLin = std::pow(10.0, spec.gainDb / 40.0);
            const double twoSqrtAAlpha = 2.0 * std::sqrt(aLin) * alpha;
            b0 = aLin * ((aLin + 1.0) + (aLin - 1.0) * cosW0 + twoSqrtAAlpha);
            b1 = -2.0 * aLin * ((aLin - 1.0) + (aLin + 1.0) * cosW0);
            b2 = aLin * ((aLin + 1.0) + (aLin - 1.0) * cosW0 - twoSqrtAAlpha);
            a0 = (aLin + 1.0) - (aLin - 1.0) * cosW0 + twoSqrtAAlpha;
            a1 = 2.0 * ((aLin - 1.0) - (aLin + 1.0) * cosW0);
            a2 = (aLin + 1.0) - (aLin - 1.0) * cosW0 - twoSqrtAAlpha;
            break;
        }

        case FilterType::LP1:
        case FilterType::HP1:
            break;
    }

    const double invA0 = 1.0 / a0;
    return BiquadCoeffs{b0 * invA0, b1 * invA0, b2 * invA0, a1 * invA0, a2 * invA0};
}

template <std::size_t MaxSections>
class BiquadCascade
{
public:
    void design(const FilterSectionSpec* specs, std::size_t count, double gainDb, double fs) noexcept
    {
        const double g = std::pow(10.0, gainDb / 20.0);
        if (specs == nullptr || count == 0)
        {
            numSections_ = 1;
            coeffs_[0] = BiquadCoeffs{g, 0.0, 0.0, 0.0, 0.0};
            return;
        }
        numSections_ = std::min(count, MaxSections);
        for (std::size_t s = 0; s < numSections_; ++s)
        {
            coeffs_[s] = designSection(specs[s], fs);
        }
        coeffs_[0].b0 *= g;
        coeffs_[0].b1 *= g;
        coeffs_[0].b2 *= g;
    }

    template <std::size_t N>
    void design(const std::array<FilterSectionSpec, N>& specs, std::size_t count, double gainDb, double fs) noexcept
    {
        design(specs.data(), std::min(count, N), gainDb, fs);
    }

    void reset() noexcept
    {
        z1_.fill(0.0);
        z2_.fill(0.0);
    }

    double processSample(double x) noexcept
    {
        double sig = x;
        for (std::size_t s = 0; s < numSections_; ++s)
        {
            const auto& c = coeffs_[s];
            const double y = c.b0 * sig + z1_[s];
            z1_[s] = c.b1 * sig - c.a1 * y + z2_[s];
            z2_[s] = c.b2 * sig - c.a2 * y;
            sig = y;
        }
        return sig;
    }

    void processBlock(const double* in, double* out, int n) noexcept
    {
        for (int i = 0; i < n; ++i)
        {
            out[i] = processSample(in[i]);
        }
    }

    void processInPlace(double* buf, int n) noexcept
    {
        processBlock(buf, buf, n);
    }

    double maxPoleRadius() const noexcept
    {
        double maxR = 0.0;
        for (std::size_t s = 0; s < numSections_; ++s)
        {
            const double a1 = coeffs_[s].a1;
            const double a2 = coeffs_[s].a2;
            const double disc = a1 * a1 - 4.0 * a2;
            if (disc >= 0.0)
            {
                const double sq = std::sqrt(disc);
                const double r1 = 0.5 * (-a1 + sq);
                const double r2 = 0.5 * (-a1 - sq);
                maxR = std::max({maxR, std::abs(r1), std::abs(r2)});
            }
            else
            {
                maxR = std::max(maxR, std::sqrt(std::max(0.0, a2)));
            }
        }
        return maxR;
    }

    std::size_t numSections() const noexcept { return numSections_; }
    const BiquadCoeffs& section(std::size_t idx) const noexcept { return coeffs_[idx]; }

private:
    std::array<BiquadCoeffs, MaxSections> coeffs_{};
    std::array<double, MaxSections> z1_{};
    std::array<double, MaxSections> z2_{};
    std::size_t numSections_ = 0;
};

class ZdfSvfLP2
{
public:
    void prepare(double fs) noexcept
    {
        fs_ = fs;
        invFs_ = 1.0 / fs_;
        fcMax_ = 0.45 * fs_;
        reset();
    }

    void reset() noexcept
    {
        ic1eq_ = 0.0;
        ic2eq_ = 0.0;
    }

    void processModulated(const double* in, const double* fcHz, const double* q,
                          double* out, int n, int ctrlStep = 16) noexcept
    {
        if (n <= 0)
        {
            return;
        }
        const int step = (n <= 64) ? 1 : std::max(1, ctrlStep);
        double ic1 = ic1eq_;
        double ic2 = ic2eq_;
        double a1 = 0.0;
        double a2 = 0.0;
        double a3 = 0.0;

        for (int i = 0; i < n; ++i)
        {
            if (i % step == 0)
            {
                const double fcS = std::clamp(fcHz[i], 10.0, fcMax_);
                const double qS = std::clamp(q[i], 0.35, 12.0);
                const double g = std::tan(std::numbers::pi * fcS * invFs_);
                const double k = 1.0 / qS;
                a1 = 1.0 / (1.0 + g * (g + k));
                a2 = g * a1;
                a3 = g * a2;
            }
            const double v3 = in[i] - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            out[i] = v2;
        }

        ic1eq_ = ic1;
        ic2eq_ = ic2;
    }

private:
    double fs_ = 48000.0;
    double invFs_ = 1.0 / 48000.0;
    double fcMax_ = 0.45 * 48000.0;
    double ic1eq_ = 0.0;
    double ic2eq_ = 0.0;
};

} // namespace ke310::dsp
