#pragma once

#if defined(HAS_ZIG)

#include "memory_types.h"
#include "game.h"
#include <engine/engine.h>
#include <engine/sprites.h>

namespace zig {

void initialize(foundation::Allocator &allocator);
void close();

} // namespace zig

#endif
