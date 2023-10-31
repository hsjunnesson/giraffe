#if defined(HAS_LUA) || defined(HAS_LUAJIT)

extern "C" {
#if defined(HAS_LUAJIT)
#include <luajit.h>
#endif
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include "if_game.h"
#include "if_util.inl"

#include "engine/math.inl"
#include "engine/color.inl"
#include "engine/engine.h"
#include "engine/atlas.h"
#include "engine/sprites.h"
#include "engine/input.h"


namespace lua {

int push_engine(lua_State *L, engine::Engine &engine) {
    engine::Engine **udata = static_cast<engine::Engine **>(lua_newuserdata(L, sizeof(engine::Engine *)));
    *udata = &engine;

    luaL_getmetatable(L, ENGINE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_atlas_frame(lua_State *L, engine::AtlasFrame atlas_frame) {
    lua_newtable(L);
    push_vector2f(L, atlas_frame.pivot);
    lua_setfield(L, -2, "pivot");
    push_rect(L, atlas_frame.rect);
    lua_setfield(L, -2, "rect");
    return 1;
}

int push_sprite(lua_State *L, engine::Sprite sprite) {
    engine::Sprite *s = static_cast<engine::Sprite *>(lua_newuserdata(L, sizeof(engine::Sprite)));
    new (s) engine::Sprite(sprite);
    
    luaL_getmetatable(L, SPRITE_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int push_sprites(lua_State *L, engine::Sprites *sprites) {
    engine::Sprites **udata = static_cast<engine::Sprites **>(lua_newuserdata(L, sizeof(engine::Sprites *)));
    *udata = sprites;

    luaL_getmetatable(L, SPRITES_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

int engine_index(lua_State* L) {
    engine::Engine **udata = static_cast<engine::Engine**>(luaL_checkudata(L, 1, ENGINE_METATABLE));
    engine::Engine *engine = *udata;

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "window_rect") == 0) {
        return push_rect(L, engine->window_rect);
    }

    return 0;
}

int engine_add_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    const char *sprite_name = luaL_checkstring(L, 2);
    math::Color4f color = engine::color::white;
    
    if (lua_istable(L, 3)) {
        color = get_color4f(L, 3);
    }
    
    engine::Sprite sprite = engine::add_sprite(**sprites, sprite_name, color);
    return push_sprite(L, sprite);
}

int engine_remove_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    uint64_t id = get_identifier(L, 2);
    engine::remove_sprite(**sprites, id);
    return 0;
}

int engine_get_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    uint64_t id = get_identifier(L, 2);
    const engine::Sprite *sprite = engine::get_sprite(**sprites, id);
    
    if (sprite) {
        push_sprite(L, *sprite);
    } else {
        lua_pushnil(L);
    }
    
    return 1;
}

int engine_transform_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    uint64_t id = get_identifier(L, 2);
    math::Matrix4f transform = get_matrix4f(L, 3);
    
    engine::transform_sprite(**sprites, id, transform);
    return 0;
}

int engine_color_sprite(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    uint64_t id = get_identifier(L, 2);
    math::Color4f color = get_color4f(L, 3);
    
    engine::color_sprite(**sprites, id, color);
    return 0;
}

int engine_update_sprites(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    float t = static_cast<float>(luaL_checknumber(L, 2));
    float dt = static_cast<float>(luaL_checknumber(L, 3));
    engine::update_sprites(**sprites, t, dt);
    return 0;
}

int engine_commit_sprites(lua_State *L) {
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 1, SPRITES_METATABLE));
    engine::commit_sprites(**sprites);
    return 0;
}

int engine_render_sprites(lua_State *L) {
    engine::Engine **engine = static_cast<engine::Engine **>(luaL_checkudata(L, 1, ENGINE_METATABLE));
    engine::Sprites **sprites = static_cast<engine::Sprites **>(luaL_checkudata(L, 2, SPRITES_METATABLE));
    engine::render_sprites(**engine, **sprites);
    return 0;
}

int sprite_identifier(lua_State *L) {
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
        engine::Atlas **atlas = static_cast<engine::Atlas **>(lua_newuserdata(L, sizeof(engine::Atlas)));
        *atlas = sprites->atlas;
        return 1;
    }
    
    return 0;
}

int atlasframe_index(lua_State *L) {
    engine::AtlasFrame *atlas_frame = static_cast<engine::AtlasFrame *>(luaL_checkudata(L, 1, ATLASFRAME_METATABLE));

    const char *key = luaL_checkstring(L, 2);
    if (strcmp(key, "pivot") == 0) {
        return push_vector2f(L, atlas_frame->pivot);
    } else if (strcmp(key, "rect") == 0) {
        return push_rect(L, atlas_frame->rect);
    }
    
    return 0;
}

int input_command_index(lua_State *L) {
    engine::InputCommand *input_command = static_cast<engine::InputCommand *>(luaL_checkudata(L, 1, INPUTCOMMAND_METATABLE));

    // TODO
    // const char *key = luaL_checkstring(L, 2);
    // if (strcmp(key, "pivot") == 0) {
    //     return push_vector2f(L, atlas_frame->pivot);
    // } else if (strcmp(key, "rect") == 0) {
    //     return push_rect(L, atlas_frame->rect);
    // }
    
    return 0;
}

int push_input_command(lua_State *L, engine::InputCommand &input_command) {
    engine::InputCommand *udata = static_cast<engine::InputCommand *>(lua_newuserdata(L, sizeof(engine::InputCommand)));
    new (udata) engine::InputCommand(input_command);
    
    luaL_getmetatable(L, INPUTCOMMAND_METATABLE);
    lua_setmetatable(L, -2);

    return 1;
}

void init_engine_module(lua_State *L) {
    // Create a table for 'Engine'
    lua_getglobal(L, "Engine");
    if (lua_isnil(L, -1)) {
        lua_pop(L, 1);
        lua_newtable(L);
    }

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
        
        lua_pushcfunction(L, engine_add_sprite);
        lua_setfield(L, -2, "add_sprite");
    
        lua_pushcfunction(L, engine_remove_sprite);
        lua_setfield(L, -2, "remove_sprite");
    
        lua_pushcfunction(L, engine_get_sprite);
        lua_setfield(L, -2, "get_sprite");
    
        lua_pushcfunction(L, engine_transform_sprite);
        lua_setfield(L, -2, "transform_sprite");
    
        lua_pushcfunction(L, engine_color_sprite);
        lua_setfield(L, -2, "color_sprite");
    
        lua_pushcfunction(L, engine_update_sprites);
        lua_setfield(L, -2, "update_sprites");
    
        lua_pushcfunction(L, engine_commit_sprites);
        lua_setfield(L, -2, "commit_sprites");
    
        lua_pushcfunction(L, engine_render_sprites);
        lua_setfield(L, -2, "render_sprites");
    }

    // atlas.h
    {
        luaL_newmetatable(L, ATLASFRAME_METATABLE);
        
        lua_pushstring(L, "__index");
        lua_pushcfunction(L, atlasframe_index);
        lua_settable(L, -3);

        lua_pop(L, 1);
    }
    
    // input.h
    {
        push_enum<engine::InputType>(L, 
            "None", engine::InputType::None,
            "Mouse", engine::InputType::Mouse,
            "Key", engine::InputType::Key,
            "Scroll", engine::InputType::Scroll,
            nullptr);
        lua_setfield(L, -2, "InputType");
        
        // TODO continue from here
    }
    
//        // input.h
//        {
//            engine.new_enum("InputType",
//                            "None", engine::InputType::None,
//                            "Mouse", engine::InputType::Mouse,
//                            "Key", engine::InputType::Key,
//                            "Scroll", engine::InputType::Scroll
//                            );
//        
//            engine.new_enum("MouseAction",
//                            "None", engine::MouseAction::None,
//                            "MouseMoved", engine::MouseAction::MouseMoved,
//                            "MouseTrigger", engine::MouseAction::MouseTrigger
//                            );
//        
//            engine.new_enum("TriggerState",
//                            "None", engine::TriggerState::None,
//                            "Pressed", engine::TriggerState::Pressed,
//                            "Released", engine::TriggerState::Released,
//                            "Repeated", engine::TriggerState::Repeated
//                            );
//        
//            engine.new_enum("CursorMode",
//                            "Normal", engine::CursorMode::Normal,
//                            "Hidden", engine::CursorMode::Hidden
//                            );
//        
//            engine.new_usertype < engine::KeyState > ("KeyState",
//                "keycode", &engine::KeyState::keycode,
//                "trigger_state", &engine::KeyState::trigger_state,
//                "shift_state", &engine::KeyState::shift_state,
//                "alt_state", &engine::KeyState::alt_state,
//                "ctrl_state", &engine::KeyState::ctrl_state
//            );
//        
//            engine.new_usertype < engine::MouseState > ("MouseState",
//                "mouse_action", &engine::MouseState::mouse_action,
//                "mouse_position", &engine::MouseState::mouse_position,
//                "mouse_relative_motion", &engine::MouseState::mouse_relative_motion,
//                "mouse_left_state", &engine::MouseState::mouse_left_state,
//                "mouse_right_state", &engine::MouseState::mouse_right_state
//            );
//        
//            engine.new_usertype < engine::ScrollState > ("ScrollState",
//                "x_offset", &engine::ScrollState::x_offset,
//                "y_offset", &engine::ScrollState::y_offset
//            );
//        
//            engine.new_usertype < engine::InputCommand > ("InputCommand",
//                "input_type", &engine::InputCommand::input_type,
//                "key_state", &engine::InputCommand::key_state,
//                "mouse_state", &engine::InputCommand::mouse_state,
//                "scroll_state", &engine::InputCommand::scroll_state
//            );
//        }
//    }
//
//    // action_binds.h
//    {
//        sol::table action_binds_table = engine["ActionBindsBind"].get_or_create<sol::table>();
//        for (int i = static_cast<int>(engine::ActionBindsBind::FIRST) + 1; i < static_cast<int>(engine::ActionBindsBind::LAST); ++i) {
//            engine::ActionBindsBind bind = static_cast<engine::ActionBindsBind>(i);
//            action_binds_table[engine::bind_descriptor(bind)] = i;
//        }
//
//        engine.new_usertype<engine::ActionBinds>("ActionBinds",
//            "bind_actions", &engine::ActionBinds::bind_actions
//        );
//
//        engine["bind_descriptor"] = engine::bind_descriptor;
//        engine["bind_for_keycode"] = engine::bind_for_keycode;
//        engine["action_key_for_input_command"] = [L](const engine::InputCommand &input_command) {
//            uint64_t result = engine::action_key_for_input_command(input_command);
//            return Identifier(result);
//        };
//    }
    
    // color.inl
    {
        lua_newtable(L);
        
        push_color4f(L, engine::color::black);
        lua_setfield(L, -2, "black");
        push_color4f(L, engine::color::white);
        lua_setfield(L, -2, "white");
        push_color4f(L, engine::color::red);
        lua_setfield(L, -2, "red");
        push_color4f(L, engine::color::green);
        lua_setfield(L, -2, "green");
        push_color4f(L, engine::color::blue);
        lua_setfield(L, -2, "blue");
        
        lua_newtable(L);

        push_color4f(L, engine::color::pico8::black);
        lua_setfield(L, -2, "black");
        push_color4f(L, engine::color::pico8::dark_blue);
        lua_setfield(L, -2, "dark_blue");
        push_color4f(L, engine::color::pico8::dark_purple);
        lua_setfield(L, -2, "dark_purple");
        push_color4f(L, engine::color::pico8::dark_green);
        lua_setfield(L, -2, "dark_green");
        push_color4f(L, engine::color::pico8::brown);
        lua_setfield(L, -2, "brown");
        push_color4f(L, engine::color::pico8::dark_gray);
        lua_setfield(L, -2, "dark_gray");
        push_color4f(L, engine::color::pico8::light_gray);
        lua_setfield(L, -2, "light_gray");
        push_color4f(L, engine::color::pico8::white);
        lua_setfield(L, -2, "white");
        push_color4f(L, engine::color::pico8::red);
        lua_setfield(L, -2, "red");
        push_color4f(L, engine::color::pico8::orange);
        lua_setfield(L, -2, "orange");
        push_color4f(L, engine::color::pico8::yellow);
        lua_setfield(L, -2, "yellow");
        push_color4f(L, engine::color::pico8::green);
        lua_setfield(L, -2, "green");
        push_color4f(L, engine::color::pico8::blue);
        lua_setfield(L, -2, "blue");
        push_color4f(L, engine::color::pico8::green);
        lua_setfield(L, -2, "green");
        push_color4f(L, engine::color::pico8::indigo);
        lua_setfield(L, -2, "indigo");
        push_color4f(L, engine::color::pico8::pink);
        lua_setfield(L, -2, "pink");
        push_color4f(L, engine::color::pico8::peach);
        lua_setfield(L, -2, "peach");

        lua_setfield(L, -2, "Pico8");

        lua_setfield(L, -2, "Color");
    }

    // Pop Engine table
    lua_setglobal(L, "Engine");
}


} // namespace lua

#endif
