#pragma once

#include "memory_types.h"

#if defined(HAS_LUA)

namespace lua {

void initialize(foundation::Allocator &allocator);
void close();

} // namespace lua

#endif
