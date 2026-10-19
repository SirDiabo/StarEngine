#pragma once
#include <cstdint>

namespace CollisionFilter {

inline bool Allows(int queryLayer, uint32_t queryMask, int colliderLayer, uint32_t colliderMask) {
    if (colliderLayer < 0 || colliderLayer > 31) colliderLayer = 0;
    if (((queryMask >> colliderLayer) & 1u) == 0u) return false;
    if (queryLayer >= 0 && queryLayer <= 31 && ((colliderMask >> queryLayer) & 1u) == 0u) return false;
    return true;
}

}
