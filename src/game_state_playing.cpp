#include "game.h"

#include "rnd.h"

#include <engine/action_binds.h>
#include <engine/input.h>
#include <engine/sprites.h>
#include <engine/color.inl>
#include <engine/math.inl>
#include <engine/atlas.h>

#include <hash.h>
#include <murmur_hash.h>

#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include <time.h>

namespace {
rnd_pcg_t random_device;
const int32_t z_layer = -1;

} // namespace

namespace game {
using namespace math;
using namespace foundation;

/// Utility to add a sprite to the game.
uint64_t add_sprite(engine::Sprites &sprites, const char *sprite_name, const int32_t x, const int32_t y, Color4f color = engine::color::white) {
    const engine::Sprite sprite = engine::add_sprite(sprites, sprite_name);

    glm::mat4 transform = glm::mat4(1.0f);
    transform = glm::translate(transform, {x, y, z_layer});
    transform = glm::scale(transform, glm::vec3(sprite.atlas_rect->size.x, sprite.atlas_rect->size.y, 1));
    engine::transform_sprite(sprites, sprite.id, Matrix4f(glm::value_ptr(transform)));
    engine::color_sprite(sprites, sprite.id, color);

    return sprite.id;
}

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;

	time_t seconds;
	time(&seconds);
	rnd_pcg_seed(&random_device, (RND_U32)seconds); 
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
    bool pressed = input_command.key_state.trigger_state == engine::TriggerState::Pressed;
    bool repeated = input_command.key_state.trigger_state == engine::TriggerState::Repeated;

    uint64_t bind_action_key = action_key_for_input_command(input_command);
    if (bind_action_key == 0) {
        return;
    }

    ActionHash action_hash = ActionHash(hash::get(game.action_binds->bind_actions, bind_action_key, (uint64_t)0));

    switch (action_hash) {
    case ActionHash::NONE: {
        break;
    }
    case ActionHash::QUIT: {
        if (pressed) {
            transition(engine, &game, AppState::Quitting);
        }
        break;
    }
    }
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    (void)engine;

	if (array::size(game.giraffes) < 5000) {
		for (int i = 0; i < 10; ++i) {
			const engine::Sprite sprite = engine::add_sprite(*game.sprites, "giraffe", engine::color::pico8::orange);

			Mob *mob = MAKE_NEW(game.allocator, Mob);
			mob->sprite_id = sprite.id;
			mob->x = 500.0f;
			mob->y = 500.0f;
			mob->x_vel = rnd_pcg_nextf(&random_device) * 64.0f - 32.0f;
			mob->y_vel = rnd_pcg_nextf(&random_device) * 64.0f - 32.0f;
			array::push_back(game.giraffes, mob);
		}
	}

//	if (array::size(game.lions) < 10) {
//		for (int i = 0; i < 10; ++i) {
//            const uint64_t sprite_id = add_sprite(*game.sprites, "lion", 640, 64, -1, engine::color::pico8::yellow);
//			Mob *mob = MAKE_NEW(game.allocator, Mob);
//			mob->sprite_id = sprite_id;
//			array::push_back(game.lions, mob);
//		}
//	}

	const math::Rect *giraffe_rect = engine::atlas_rect(*game.sprites->atlas, "giraffe");
	assert(giraffe_rect);

	for (Mob **it = array::begin(game.giraffes); it != array::end(game.giraffes); ++it) {
		Mob *mob = *it;
		mob->x += mob->x_vel * dt;
		mob->y += mob->y_vel * dt;

		glm::mat4 transform = glm::mat4(1.0f);
		transform = glm::translate(transform, {floorf(mob->x), floorf(mob->y), z_layer});
		transform = glm::scale(transform, glm::vec3(giraffe_rect->size.x, giraffe_rect->size.y, 1));
		engine::transform_sprite(*game.sprites, mob->sprite_id, Matrix4f(glm::value_ptr(transform)));
	}

	engine::update_sprites(*game.sprites, t, dt);
	engine::commit_sprites(*game.sprites);
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
	engine::render_sprites(engine, *game.sprites);
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    (void)engine;
    (void)game;
}

} // namespace game
