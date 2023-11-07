
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

struct Obstacle {
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
    float radius = 100.0f;
    math::Color4f color = math::Color4f(1.0f, 1.0f, 1.0f, 1.0f);
};

struct Food {
    uint64 sprite_id = 0;
    glm::vec2 position = glm::vec2(0.0f, 0.0f);
};

void on_enter(engine::Engine@ engine, game::Game@ game) {
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
}

void render(engine::Engine@ engine, game::Game@ game) {
}

void render_imgui(engine::Engine@ engine, game::Game@ game) {
}
