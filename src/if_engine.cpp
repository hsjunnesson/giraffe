#if defined(HAS_LUA) || defined(HAS_LUAJIT)

#include "if_game.h"

extern "C" {
#if defined(HAS_LUAJIT)
#include <luajit.h>
#endif
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#define SOL_ALL_SAFETIES_ON 1
#include <sol/sol.hpp>

#include "engine/engine.h"
#include "engine/atlas.h"
#include "engine/sprites.h"

namespace lua {

int push_engine(lua_State *L, engine::Engine &engine) {
    return 0;
}

static int push_atlas_frame(lua_State *L, engine::AtlasFrame atlas_frame) {
    lua_newtable(L);
    push_vector2f(L, atlas_frame.pivot);
    lua_setfield(L, -2, "pivot");
    push_rect(L, atlas_frame.rect);
    lua_setfield(L, -2, "rect");
    return 1;
}

// Index function for Engine
int engine_index(lua_State* L) {
    engine::Engine *engine = static_cast<engine::Engine*>(luaL_checkudata(L, 1, ENGINE_METATABLE));

    const char* key = luaL_checkstring(L, 2);
    if (strcmp(key, "window_rect") == 0) {
        return push_rect(L, engine->window_rect);
    }

    return 0;
}

static int sprite_identifier(lua_State *L) {
    engine::Sprite *sprite = static_cast<engine::Sprite *>(luaL_checkudata(L, 1, SPRITE_METATABLE));
    return push_identifier(L, sprite->id);
}

int sprite_index(lua_State *L) {
    engine::Sprite *sprite= static_cast<engine::Sprite*>(luaL_checkudata(L, 1, SPRITE_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "atlas_frame") == 0) {
        return push_atlas_frame(L, *sprite->atlas_frame);
    }
    
    return 0;
}

int sprites_index(lua_State *L) {
    engine::Sprites *sprites = static_cast<engine::Sprites*>(luaL_checkudata(L, 1, SPRITES_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "atlas") == 0) {
        return push_atlas_frame(L, *(*sprite)->atlas_frame);
    }
    
    return 0;
}

void init_engine_module(lua_State *L) {
//    sol::state_view lua(L);
//    sol::table engine = lua["Engine"].get_or_create<sol::table>();

    // engine.h
    {
        luaL_newmetatable(L, ENGINE_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, engine_index);
        lua_settable(L, -3);

        lua_pop(L, 1);
    }

    // sprites.h
    {
        luaL_newmetatable(L, SPRITE_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, sprite_index);
        lua_settable(L, -3);

        lua_pushstring(L, "identifier");
        lua_pushcfunction(L, sprite_identifier);
        lua_settable(L, -3);

        lua_pop(L, 1);


        luaL_newmetatable(L, SPRITES_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, sprites_index);
        lua_settable(L, -3);

        lua_pop(L, 1);
    }
    
//        engine.new_usertype<engine::Sprite>("Sprite",
//            "identifier", [](const engine::Sprite &sprite) {
//                return Identifier(sprite.id);
//            },
//            "atlas_frame", &engine::Sprite::atlas_frame
//        );
//
//        engine.new_usertype<engine::Sprites>("Sprites",
//            "atlas", &engine::Sprites::atlas
//        );

        engine["add_sprite"] = engine::add_sprite;
        engine["remove_sprite"] = engine::add_sprite;
        engine["get_sprite"] = [](const engine::Sprites &sprites, const Identifier identifier) {
            return engine::get_sprite(sprites, identifier.value);
        };
        engine["transform_sprite"] = [](engine::Sprites &sprites, const Identifier identifier, const math::Matrix4f transform) {
            engine::transform_sprite(sprites, identifier.value, transform);
        };
        engine["color_sprite"] = [](engine::Sprites &sprites, const Identifier identifier, const math::Color4f color) {
            engine::color_sprite(sprites, identifier.value, color);
        };
        engine["update_sprites"] = engine::update_sprites;
        engine["commit_sprites"] = engine::commit_sprites;
        engine["render_sprites"] = engine::render_sprites;
    }

    // color.inl
    {
        sol::table color = engine["Color"].get_or_create<sol::table>();
        color["black"] = engine::color::black;
        color["white"] = engine::color::white;
        color["red"] = engine::color::red;
        color["green"] = engine::color::green;
        color["blue"] = engine::color::blue;

        sol::table pico8 = color["Pico8"].get_or_create<sol::table>();
        pico8["black"] = engine::color::pico8::black;
        pico8["dark_blue"] = engine::color::pico8::dark_blue;
        pico8["dark_purple"] = engine::color::pico8::dark_purple;
        pico8["dark_green"] = engine::color::pico8::dark_green;
        pico8["brown"] = engine::color::pico8::brown;
        pico8["dark_gray"] = engine::color::pico8::dark_gray;
        pico8["light_gray"] = engine::color::pico8::light_gray;
        pico8["white"] = engine::color::pico8::white;
        pico8["red"] = engine::color::pico8::red;
        pico8["orange"] = engine::color::pico8::orange;
        pico8["yellow"] = engine::color::pico8::yellow;
        pico8["green"] = engine::color::pico8::green;
        pico8["blue"] = engine::color::pico8::blue;
        pico8["indigo"] = engine::color::pico8::indigo;
        pico8["pink"] = engine::color::pico8::pink;
        pico8["peach"] = engine::color::pico8::peach;
    }

    // atlas.h
    {
        engine.new_usertype<engine::AtlasFrame>("AtlasFrame",
            "pivot", &engine::AtlasFrame::pivot,
            "rect", &engine::AtlasFrame::rect
        );

        engine["atlas_frame"] = engine::atlas_frame;
    }

    // input.h
    {
        engine.new_enum("InputType",
            "None", engine::InputType::None,
            "Mouse", engine::InputType::Mouse,
            "Key", engine::InputType::Key,
            "Scroll", engine::InputType::Scroll
        );

        engine.new_enum("MouseAction",
            "None", engine::MouseAction::None,
            "MouseMoved", engine::MouseAction::MouseMoved,
            "MouseTrigger", engine::MouseAction::MouseTrigger
        );

        engine.new_enum("TriggerState",
            "None", engine::TriggerState::None,
            "Pressed", engine::TriggerState::Pressed,
            "Released", engine::TriggerState::Released,
            "Repeated", engine::TriggerState::Repeated
        );

        engine.new_enum("CursorMode",
            "Normal", engine::CursorMode::Normal,
            "Hidden", engine::CursorMode::Hidden
        );

        engine.new_usertype<engine::KeyState>("KeyState",
            "keycode", &engine::KeyState::keycode,
            "trigger_state", &engine::KeyState::trigger_state,
            "shift_state", &engine::KeyState::shift_state,
            "alt_state", &engine::KeyState::alt_state,
            "ctrl_state", &engine::KeyState::ctrl_state
        );

        engine.new_usertype<engine::MouseState>("MouseState",
            "mouse_action", &engine::MouseState::mouse_action,
            "mouse_position", &engine::MouseState::mouse_position,
            "mouse_relative_motion", &engine::MouseState::mouse_relative_motion,
            "mouse_left_state", &engine::MouseState::mouse_left_state,
            "mouse_right_state", &engine::MouseState::mouse_right_state
        );

        engine.new_usertype<engine::ScrollState>("ScrollState",
            "x_offset", &engine::ScrollState::x_offset,
            "y_offset", &engine::ScrollState::y_offset
        );

        engine.new_usertype<engine::InputCommand>("InputCommand",
            "input_type", &engine::InputCommand::input_type,
            "key_state", &engine::InputCommand::key_state,
            "mouse_state", &engine::InputCommand::mouse_state,
            "scroll_state", &engine::InputCommand::scroll_state
        );
    }

    // action_binds.h
    {
        sol::table action_binds_table = engine["ActionBindsBind"].get_or_create<sol::table>();
        for (int i = static_cast<int>(engine::ActionBindsBind::FIRST) + 1; i < static_cast<int>(engine::ActionBindsBind::LAST); ++i) {
            engine::ActionBindsBind bind = static_cast<engine::ActionBindsBind>(i);
            action_binds_table[engine::bind_descriptor(bind)] = i;
        }

        engine.new_usertype<engine::ActionBinds>("ActionBinds",
            "bind_actions", &engine::ActionBinds::bind_actions
        );

        engine["bind_descriptor"] = engine::bind_descriptor;
        engine["bind_for_keycode"] = engine::bind_for_keycode;
        engine["action_key_for_input_command"] = [L](const engine::InputCommand &input_command) {
            uint64_t result = engine::action_key_for_input_command(input_command);
            return Identifier(result);
        };
    }
    
    // Create a table for 'Engine'
    lua_getglobal(L, "Engine");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

    lua_setglobal(L, "Engine");
}


} // namespace lua

#endif
