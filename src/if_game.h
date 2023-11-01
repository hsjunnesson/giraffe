#pragma once

#include "memory_types.h"

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace lua {

void initialize(foundation::Allocator &allocator);
void close();

} // namespace lua

#endif
