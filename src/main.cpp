#define RND_IMPLEMENTATION
#include "rnd.h"

#pragma warning(push, 0)
#include <engine/engine.h>
#include <engine/input.h>
#include <engine/log.h>

#include <backward.hpp>
#include <memory.h>

#if defined(LIVE_PP)
#include <Windows.h>

#include "LPP_API_x64_CPP.h"
#endif
#pragma warning(pop)

#include "game.h"

int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

#if defined(SUPERLUMINAL)
    PerformanceAPI_SetCurrentThreadName("main");
#endif

    // Validate platform
    {
        unsigned int x = 1;
        char *c = (char *)&x;
        if ((int)*c == 0) {
            log_fatal("Unsupported platform: big endian");
        }

        if constexpr (sizeof(char) != sizeof(uint8_t)) {
            log_fatal("Unsupported platform: invalid char size");
        }

        if constexpr (sizeof(float) != 4) {
            log_fatal("Unsupported platform: invalid float size");
        }
    }

#if defined(LIVE_PP)
    lpp::LppDefaultAgent lpp_agent = lpp::LppCreateDefaultAgent(nullptr, L"LivePP");
    lpp_agent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_ALL_IMPORT_MODULES, nullptr, nullptr);
#endif

    int status = 0;
    backward::SignalHandling sh;
    foundation::memory_globals::init();
    foundation::Allocator &allocator = foundation::memory_globals::default_allocator();

    {
        const char *config_path = "assets/config.ini";
        engine::Engine engine(allocator, config_path);

        game::Game game(allocator, config_path);
        engine::EngineCallbacks engine_callbacks;
        engine_callbacks.on_input = game::on_input;
        engine_callbacks.update = game::update;
        engine_callbacks.render = game::render;
        engine_callbacks.render_imgui = game::render_imgui;
        engine_callbacks.on_shutdown = game::on_shutdown;

        engine.engine_callbacks = &engine_callbacks;
        engine.game_object = &game;

        status = engine::run(engine);
    }

    foundation::memory_globals::shutdown();

#if defined(LIVE_PP)
    lpp::LppDestroyDefaultAgent(&lpp_agent);
#endif

    return status;
}
