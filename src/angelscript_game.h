#pragma once

#include "memory_types.h"

#if defined(HAS_ANGELSCRIPT)

namespace angelscript {

void initialize(foundation::Allocator &allocator);
void close();

} // namespace angelscript

#endif
