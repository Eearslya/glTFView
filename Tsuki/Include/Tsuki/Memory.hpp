#pragma once

#include <cstdlib>

namespace tk {
void* AlignedAlloc(size_t size, size_t align, bool zero = false);
void AlignedFree(void* ptr);
}  // namespace tk
