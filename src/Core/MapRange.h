#pragma once
#include <cmath>

inline double MapRangeFraction(float value, float inA, float inB) {
    double span = (double)inB - (double)inA;
    if (span == 0.0) return 0.0;
    double t = ((double)value - (double)inA) / span;
    return std::isnan(t) ? 0.0 : t;
}

inline float MapRangeUnclamped(float value, float inA, float inB, float outA, float outB) {
    double result = (double)outA + ((double)outB - (double)outA) * MapRangeFraction(value, inA, inB);
    float narrowed = (float)result;
    return std::isfinite(narrowed) ? narrowed : outA;
}

inline float MapRangeClamped(float value, float inA, float inB, float outA, float outB) {
    double t = MapRangeFraction(value, inA, inB);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    return (float)((double)outA + ((double)outB - (double)outA) * t);
}
