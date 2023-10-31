#include <lua.h>
#include <lauxlib.h>

#include <cstdarg>

namespace lua {

template <typename EnumType>
inline int push_enum(lua_State *L, ...) {
    va_list args;
    va_start(args, L);

    lua_newtable(L);  // Create a new table for the enum.

    while (true) {
        const char *enum_name = va_arg(args, const char *);
        if (enum_name == nullptr) { // Sentinel value to determine end
            break;
        }
        EnumType raw_enum_value = va_arg(args, EnumType);
        
        int enum_value = static_cast<int>(raw_enum_value);

        lua_pushinteger(L, enum_value);
        lua_setfield(L, -2, enum_name);
    }

    va_end(args);
    
    return 1;
}

} // namespace lua

