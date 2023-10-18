#pragma once

// Script interfaces

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace lua {

void initialize();

} // namespace game

#endif
