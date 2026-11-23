// Shared RBJ biquad filter specification types for KE-310 DSP and KE Rhythmbox DSP.
#pragma once

#include <cstddef>

namespace ke310::dsp
{

inline constexpr std::size_t kMaxFilterSections = 4;

enum class FilterType : int
{
    LP1 = 0,
    HP1,
    LP2,
    HP2,
    BP2,
    Peaking,
    LowShelf,
    HighShelf
};

struct FilterSectionSpec
{
    FilterType type = FilterType::LP1;
    double freqHz = 1000.0;
    double q = 0.70710678;
    double gainDb = 0.0;
};

} // namespace ke310::dsp
