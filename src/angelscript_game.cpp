#include "angelscript_game.h"

#if defined(HAS_ANGELSCRIPT)

#include "game.h"

#include "memory.h"
#include "temp_allocator.h"
#include "array.h"
#include "string_stream.h"

#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/file.h>

#include <angelscript.h>
#include "add_on/scriptstdstring/scriptstdstring.h"
#include "add_on/scriptbuilder/scriptbuilder.h"

namespace {
asIScriptEngine *script_engine = nullptr;
asIScriptFunction *update_func = nullptr;
asIScriptContext *ctx = nullptr;
}

void message_callback(const asSMessageInfo *msg, void *param) {
    const char *type = "ERR ";
    
    if (msg->type == asMSGTYPE_WARNING) {
        type = "WARN";
    } else if (msg->type == asMSGTYPE_INFORMATION) {
        type = "INFO";
    }

    log_info("[ANGELSCRIPT] %s (%d, %d) : %s : %s", msg->section, msg->row, msg->col, type, msg->message);
}

void print(std::string &msg) {
    log_info("[ANGELSCRIPT] %s", msg.c_str());
}

namespace angelscript {

void initialize(foundation::Allocator &allocator) {
    using namespace foundation;
    using namespace foundation::string_stream;
	using namespace foundation::array;

    log_info("Initializing angelscript");

    script_engine = asCreateScriptEngine();
    int r = script_engine->SetMessageCallback(asFUNCTION(message_callback), 0, asCALL_CDECL);
    assert(r >= 0);

    RegisterStdString(script_engine);
    
    // Register functions

    r = script_engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print), asCALL_CDECL);
    assert(r >= 0);

    // MyModule

    CScriptBuilder builder;
    r = builder.StartNewModule(script_engine, "MyModule");
    assert(r >= 0);

    r = builder.AddSectionFromFile("scripts/script.as");
    assert(r >= 0);

    asIScriptModule *mod = script_engine->GetModule("MyModule");

    // Context

    ctx = script_engine->CreateContext();
    assert(ctx);

    // Register namespaces and types

    r = script_engine->SetDefaultNamespace("engine");
    assert(r >= 0);

    r = script_engine->RegisterObjectType("Engine", 0, asOBJ_REF | asOBJ_NOCOUNT);
    assert(r >= 0);

    r = script_engine->SetDefaultNamespace("");
    assert(r >= 0);

    r = script_engine->SetDefaultNamespace("game");
    assert(r >= 0);

    r = script_engine->RegisterObjectType("Game", 0, asOBJ_REF | asOBJ_NOCOUNT);
    assert(r >= 0);

    r = script_engine->SetDefaultNamespace("");
    assert(r >= 0);

    // Build module

    r = builder.BuildModule();
    assert(r >= 0);

    // Get functions
    
    update_func = mod->GetFunctionByDecl("void update(engine::Engine @engine, game::Game @game, float t, float dt)");
    assert(update_func);
}

void close() {
    if (ctx) {
        ctx->Release();
        ctx = nullptr;
    }

    if (script_engine) {
        script_engine->ShutDownAndRelease();
        script_engine = nullptr;
        update_func = nullptr;
    }
}

} // namespace angelscript

namespace game {


void game_state_playing_enter(engine::Engine &engine, Game &game) {
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    if (ctx && update_func) {
        ctx->Prepare(update_func);
        ctx->SetArgAddress(0, &engine);
        ctx->SetArgAddress(1, &game);
        ctx->SetArgFloat(2, t);
        ctx->SetArgFloat(3, dt);
        int r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            log_fatal("Could not execute update() in scripts/script.as: %s", ctx->GetExceptionString());
        }
        ctx->Unprepare();
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
}

} // namespace game

#endif
