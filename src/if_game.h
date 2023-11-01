#pragma once

#if defined(HAS_LUA) || defined(HAS_LUAJIT)

namespace lua {

void initialize();

} // namespace lua

#endif
