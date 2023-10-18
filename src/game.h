#pragma once

#pragma warning(push, 0)
#include "collection_types.h"
#include "memory_types.h"
#include <glm/glm.hpp>
#include <stdint.h>
#include <engine/math.inl>
#include "util.h"
#pragma warning(pop)

typedef struct ini_t ini_t;

namespace engine {
struct Engine;
struct InputCommand;
struct ActionBinds;
struct Sprites;
} // namespace engine

namespace game {

/// Murmur hashed actions.
enum class ActionHash : uint64_t {
    NONE = 0x0ULL,
    QUIT = 0x387bbb994ac3551ULL,
    DEBUG_DRAW = 0xe211fdfdbc9f06a0ULL,
    DEBUG_AVOIDANCE = 0xed7741fffed1553eULL,
    ADD_ONE = 0xc6d7077102962545ULL,
    ADD_FIVE = 0x290570f1484c39a6ULL,
    ADD_TEN = 0xebc74835735457f2ULL,
};

/**
 * @brief An enum that describes a specific application state.
 *
 */
enum class AppState {
    // No state.
    None,

    // Application state is creating, or loading from a save.
    Initializing,

    // The user is using the game
    Playing,

    // Shutting down, saving and closing the application.
    Quitting,

    // Final state that signals the engine to terminate the application.
    Terminate,
};

struct Mob {
    float mass = 100.0f;
    glm::vec2 position = {0.0f, 0.0f};
    glm::vec2 velocity = {0.0f, 0.0f};
    glm::vec2 steering_direction = {0.0f, 0.0f};
    glm::vec2 steering_target = {0.0f, 0.0f};
    float max_force = 30.0f;
    float max_speed = 30.0f;
    float orientation = 0.0f;
    float radius = 10.0f;
};

struct Giraffe {
    uint64_t sprite_id = 0;
    Mob mob;
    bool dead = false;
};

struct Lion {
    uint64_t sprite_id = 0;
    Mob mob;
    Giraffe *locked_giraffe = nullptr;
    float energy = 0.0f;
    float max_enery = 10.0f;
};

struct Obstacle {
    glm::vec2 position = {0.0f, 0.0f};
    float radius = 100.0f;
    math::Color4f color = {1.0f, 1.0f, 1.0f, 1.0f};
};

struct Food {
    uint64_t sprite_id = 0;
    glm::vec2 position = {0.0f, 0.0f};
};

// This is the game state for the C++ gameplay implementation.
struct GameState {
    GameState(foundation::Allocator &allocator)
    : giraffes(allocator)
    , obstacles(allocator)
    , food()
    , lion()
    , debug_draw(true)
    , debug_avoidance(false)
    {}
//    ~GameState();
//    DELETE_COPY_AND_MOVE(GameState)

    foundation::Array<Giraffe> giraffes;
    foundation::Array<Obstacle> obstacles;
    Food food;
    Lion lion;

    bool debug_draw;
    bool debug_avoidance;
};

struct Game {
    Game(foundation::Allocator &allocator, const char *config_path);
    ~Game();

    foundation::Allocator &allocator;
    ini_t *config;
    engine::ActionBinds *action_binds;
    engine::Sprites *sprites;
    AppState app_state;
    GameState game_state;
};

/**
 * @brief Updates the application
 *
 * @param engine The engine which calls this function
 * @param application The application to update
 * @param t The current time
 * @param dt The delta time since last update
 */
void update(engine::Engine &engine, void *application, float t, float dt);

/**
 * @brief Callback to the application that an input has ocurred.
 *
 * @param engine The engine which calls this function
 * @param application The application to signal.
 * @param input_command The input command.
 */
void on_input(engine::Engine &engine, void *application, engine::InputCommand &input_command);

/**
 * @brief Renders the application
 *
 * @param engine The engine which calls this function
 * @param application The application to render
 */
void render(engine::Engine &engine, void *application);

/**
 * @brief Renders the imgui
 *
 * @param engine The engine which calls this function
 * @param application The application to render
 */
void render_imgui(engine::Engine &engine, void *application);

/**
 * @brief Asks the application to quit.
 *
 * @param engine The engine which calls this function
 * @param application The application to render
 */
void on_shutdown(engine::Engine &engine, void *application);

/**
 * @brief Transition an application to another app state.
 *
 * @param engine The engine which calls this function
 * @param application The application to transition
 * @param app_state The AppState to transition to.
 */
void transition(engine::Engine &engine, void *application, AppState app_state);

} // namespace game
