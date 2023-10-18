#pragma once

// Script interfaces

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace game {

void initialize_lua();

} // namespace game

#endif
