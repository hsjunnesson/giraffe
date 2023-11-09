//rnd_pcg_t RANDOM_DEVICE;
const int32 LION_Z_LAYER = -1;
const int32 GIRAFFE_Z_LAYER = -2;
const int32 FOOD_Z_LAYER = -3;

class Mob {
    float mass = 100.0f;
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    glm::vec2 velocity = glm::vec2(0.0f, 0.0f);
    glm::vec2 steering_direction = glm::vec2(0.0f, 0.0f);
    glm::vec2 steering_target = glm::vec2(0.0f, 0.0f);
    float max_force = 30.0f;
    float max_speed = 30.0f;
    float orientation = 0.0f;
    float radius = 10.0f;
}

class Giraffe {
    uint64 sprite_id = 0;
    Mob mob;
    bool dead = false;
}

class Lion {
    uint64 sprite_id = 0;
    Mob mob;
    Giraffe@ locked_giraffe = null;
    float energy = 0.0f;
    float max_energy = 10.0f;
}

class Obstacle {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    float radius = 100.0f;
    math::Color4f color = math::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
}

class Food {
    uint64 sprite_id = 0;
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
}

class GameState {
    array<Giraffe> giraffes;
    array<Obstacle> obstacles;
    Food food;
    Lion lion;
}

GameState game_state;

void spawn_giraffes(engine::Engine@ engine, game::Game@ game, int num_giraffes) {
    for (int i = 0; i < num_giraffes; ++i) {
        Giraffe giraffe;
        giraffe.mob.mass = 100.0f;
        giraffe.mob.max_force = 1000.0f;
        giraffe.mob.max_speed = 300.0f;
        giraffe.mob.radius = 20.0;
        giraffe.mob.position = glm::vec2(
            50.0f + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.x - 100.0f),
            50.0f + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.y - 100.0f));
        const engine::Sprite sprite = engine::add_sprite(game.sprites, "giraffe", engine::color::pico8::orange);
        giraffe.sprite_id = sprite.id;
        game_state.giraffes.insertLast(giraffe);
    }
}

void on_enter(engine::Engine@ engine, game::Game@ game) {
    rnd_pcg_seed(RANDOM_DEVICE, 123456);

    // Spawn lake
    Obstacle obstacle;
    obstacle.position = glm::vec2(
        (engine.window_rect.size.x / 2) + 200.0f * rnd_pcg_nextf(RANDOM_DEVICE) - 100.0f,
        (engine.window_rect.size.y / 2) + 200.0f * rnd_pcg_nextf(RANDOM_DEVICE) - 100.0f);
    obstacle.color = engine::color::pico8::blue;
    obstacle.radius = 100.0f + rnd_pcg_nextf(RANDOM_DEVICE) * 100.0f;

    game_state.obstacles.insertLast(obstacle);

    // Spawn trees
    for (int i = 0; i < 10; ++i) {
        Obstacle obstacle;
        obstacle.position = glm::vec2(
            10.0f + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.x - 20.0f),
            10.0f + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.y - 20.0f));
        obstacle.color = engine::color::pico8::light_gray;
        obstacle.radius = 20.0f;

        game_state.obstacles.insertLast(obstacle);
    }

    // Spawn giraffes
    spawn_giraffes(engine, game, 10);

    // Spawn food
    {
        const engine::Sprite sprite = engine::add_sprite(game.sprites, "food", engine::color::pico8::green);
        game_state.food.sprite_id = sprite.id;
        game_state.food.position = glm::vec2(0.25f * engine.window_rect.size.x, 0.25f * engine.window_rect.size.y);

        print("rect size x " + sprite.atlas_frame.rect.size.x);
        print("rect size y " + sprite.atlas_frame.rect.size.y);

        transform = glm::translate(transform, glm::vec3(
                                                  floor(game_state.food.position.x - sprite.atlas_frame.rect.size.x * sprite.atlas_frame.pivot.x),
                                                  floor(game_state.food.position.y - sprite.atlas_frame.rect.size.y * (1.0f - sprite.atlas_frame.pivot.y)),
                                                  FOOD_Z_LAYER));
        transform = glm::scale(transform, glm::vec3(sprite.atlas_frame.rect.size.x, sprite.atlas_frame.rect.size.y, 1.0f));
        engine::transform_sprite(game.sprites, game_state.food.sprite_id, math::Matrix4f(transform));
    }

    // Spawn lion
    {
        const engine::Sprite sprite = engine::add_sprite(game.sprites, "lion", engine::color::pico8::yellow);
        game_state.lion.sprite_id = sprite.id;
        game_state.lion.mob.position = glm::vec2(engine.window_rect.size.x * 0.75f, engine.window_rect.size.y * 0.75f);
        game_state.lion.mob.mass = 25.0f;
        game_state.lion.mob.max_force = 1000.0f;
        game_state.lion.mob.max_speed = 400.0f;
        game_state.lion.mob.radius = 20.0;
    }
}

void on_leave(engine::Engine@ engine, game::Game@ game) {
}

// void on_input(engine::Engine@ engine, game::Game@ game, engine::InputCommand input_command) {
// }

void update(engine::Engine@ engine, game::Game@ game, float t, float dt) {
//    engine::Sprites@ sprites = game.sprites;
//	engine::update_sprites(sprites, t, dt);

	// glm::vec2 a;
    // a.x = 5.5;
	// glm::vec2 b(1.0f, 2.0f);
	// glm::vec2 c = a + b;
    // print(c.tostring());

    // glm::vec2 d = c / 2.0;
    // print(d.tostring());

    // glm::vec2 n = glm::normalize(d);
    // print(n.tostring());
    // print("" + glm::length(n));

    engine::update_sprites(game.sprites, t, dt);
    engine::commit_sprites(game.sprites);
}

void render(engine::Engine@ engine, game::Game@ game) {
    engine::render_sprites(engine, game.sprites);
}

void render_imgui(engine::Engine@ engine, game::Game@ game) {
}
