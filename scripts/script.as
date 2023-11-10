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

bool ray_circle_intersection(const glm::vec2 &in ray_origin, const glm::vec2 &in ray_direction, const glm::vec2 &in circle_center, float circle_radius, glm::vec2 &out intersection) {
    glm::vec2 ray_dir = glm::normalize(ray_direction);

    // Check if origin is inside the circle
    if (glm::length(ray_origin - circle_center) <= circle_radius) {
        intersection = ray_origin;
        return true;
    }

    // Compute the nearest point on the ray to the circle's center
    float t = glm::dot(circle_center - ray_origin, ray_dir);

    if (t < 0.0f) {
        return false;
    }

    glm::vec2 P = ray_origin + t * ray_dir;

    // If the nearest point is inside the circle, calculate intersection
    if (glm::length(P - circle_center) <= circle_radius) {
        // Distance from P to circle boundary along the ray
        float h = sqrt(circle_radius * circle_radius - glm::length(P - circle_center) * glm::length(P - circle_center));
        intersection = P - h * ray_dir;
        return true;
    }

    return false;
}

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
    spawn_giraffes(engine, game, 1000);

    // Spawn food
    {
        const engine::Sprite sprite = engine::add_sprite(game.sprites, "food", engine::color::pico8::green);
        game_state.food.sprite_id = sprite.id;
        game_state.food.position = glm::vec2(0.25f * engine.window_rect.size.x, 0.25f * engine.window_rect.size.y);

        glm::mat4 transform = glm::mat4(1.0f);
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

void update_mob(Mob@ mob, game::Game@ game, float dt) {
    float drag = 1.0f;

    // lake drags you down
    for (uint i = 0; i < game_state.obstacles.length(); ++i) {
        Obstacle@ obstacle = game_state.obstacles[i];
        const float length = glm::length(obstacle.position - mob.position);
        if (length <= obstacle.radius) {
            drag = 10.0f;
            break;
        }
    }

    const glm::vec2 drag_force = -drag * mob.velocity;
    const glm::vec2 steering_force = glm::truncate(mob.steering_direction, mob.max_force) + drag_force;
    const glm::vec2 acceleration = steering_force / mob.mass;

    mob.velocity = glm::truncate(mob.velocity + acceleration, mob.max_speed);
    mob.position = mob.position + mob.velocity * dt;

    if (glm::length(mob.velocity) > 0.001f) {
        mob.orientation = atan2(-mob.velocity.x, mob.velocity.y);
    }
}

glm::vec2 arrival_behavior(const Mob@ mob, const glm::vec2 target_position, float speed_ramp_distance) {
    const glm::vec2 target_offset = target_position - mob.position;
    const float distance = glm::length(target_offset);
    const float ramped_speed = mob.max_speed * (distance / speed_ramp_distance);
    const float clipped_speed = ramped_speed < mob.max_speed ? ramped_speed : mob.max_speed;
    const glm::vec2 desired_velocity = (clipped_speed / distance) * target_offset;
    return desired_velocity - mob.velocity;
}

glm::vec2 avoidance_behavior(const Mob@ mob, engine::Engine@ engine) {
    const float look_ahead_distance = 200.0f;

    const glm::vec2 origin = mob.position;
    const float target_distance = glm::length(mob.steering_target - origin);
    const glm::vec2 forward = glm::normalize(mob.steering_target - origin);

    const glm::vec2 right_vector = glm::vec2(forward.y, -forward.x);
    const glm::vec2 left_vector = glm::vec2(-forward.y, forward.x);

    const glm::vec2 left_start = origin + left_vector * mob.radius;
    const glm::vec2 right_start = origin + right_vector * mob.radius;

    bool left_intersects = false;
    glm::vec2 left_intersection;
    float left_intersection_distance = 1000000.0f; // large enough

    bool right_intersects = false;
    glm::vec2 right_intersection;
    float right_intersection_distance = 1000000.0f; // large enough

    // check against each obstacle
    for (uint i = 0; i < game_state.obstacles.length(); ++i) {
        Obstacle@ obstacle = @game_state.obstacles[i];
        glm::vec2 li;
        bool did_li = ray_circle_intersection(left_start, forward, obstacle.position, obstacle.radius, li);
        if (did_li) {
            float distance = glm::length(li - left_start);
            if (distance <= look_ahead_distance && distance < left_intersection_distance) {
                left_intersects = true;
                left_intersection = li;
                left_intersection_distance = distance;
            }
        }

        glm::vec2 ri;
        bool did_ri = ray_circle_intersection(right_start, forward, obstacle.position, obstacle.radius, ri);
        if (did_ri) {
            float distance = glm::length(ri - right_start);
            if (distance <= look_ahead_distance && distance < right_intersection_distance) {
                right_intersects = true;
                right_intersection = li;
                right_intersection_distance = distance;
            }
        }
    }

    glm::vec2 avoidance_force = glm::vec2(0.0f, 0.0f);

    if (right_intersects || left_intersects) {
        if (target_distance <= left_intersection_distance && target_distance <= right_intersection_distance) {
            return glm::vec2(0.0f, 0.0f);
        }

        if (left_intersection_distance < right_intersection_distance) {
            float ratio = left_intersection_distance / look_ahead_distance;
            avoidance_force = right_vector * (1.0f - ratio) * 50.0f;
        } else {
            float ratio = right_intersection_distance / look_ahead_distance;
            avoidance_force = left_vector * (1.0f - ratio) * 50.0f;
        }
    }

    return avoidance_force;
}

void update_giraffe(Giraffe@ giraffe, engine::Engine@ engine, game::Game@ game, float dt) {
    glm::vec2 arrival_force = glm::vec2(0.0f, 0.0f);
    const float arrival_weight = 1.0f;

    glm::vec2 flee_force = glm::vec2(0.0f, 0.0f);
    float flee_weight = 1.0f;

    glm::vec2 separation_force = glm::vec2(0.0f, 0.0f);
    const float separation_weight = 10.0f;

    glm::vec2 avoidance_force = glm::vec2(0.0f, 0.0f);
    const float avoidance_weight = 10.0f;

    if (!giraffe.dead) {
        bool is_hunted = false;

        float distance = glm::length(game_state.lion.mob.position - giraffe.mob.position);
        if (distance <= 300.0f) {
            is_hunted = true;
        }

        // flee
        if (is_hunted) {
            const glm::vec2 flee_direction = giraffe.mob.position - game_state.lion.mob.position;
            const glm::vec2 steering_target = giraffe.mob.position + flee_direction;
            giraffe.mob.steering_target = steering_target;

            glm::vec2 desired_velocity = glm::normalize(flee_direction) * giraffe.mob.max_speed;
            flee_force = desired_velocity - giraffe.mob.velocity;

            glm::vec2 boundary_avoidance_force = glm::vec2(0.0f, 0.0f);
            const float buffer_distance = 100.0f;

            if (giraffe.mob.position.x <= buffer_distance) {
                boundary_avoidance_force.x = buffer_distance - giraffe.mob.position.x;
            } else if (giraffe.mob.position.x >= engine.window_rect.size.x - buffer_distance) {
                boundary_avoidance_force.x = engine.window_rect.size.x - buffer_distance - giraffe.mob.position.x;
            }

            if (giraffe.mob.position.y <= buffer_distance) {
                boundary_avoidance_force.y = buffer_distance - giraffe.mob.position.y;
            } else if (giraffe.mob.position.y >= engine.window_rect.size.y - buffer_distance) {
                boundary_avoidance_force.y = engine.window_rect.size.y - buffer_distance - giraffe.mob.position.y;
            }

            boundary_avoidance_force *= 20.0f;

            flee_force += boundary_avoidance_force;
            flee_force *= flee_weight;
        } else {
            // arrival
            giraffe.mob.steering_target = game_state.food.position;
            arrival_force = arrival_behavior(giraffe.mob, game_state.food.position, 100.0f);
            arrival_force *= arrival_weight;
        }

        // separation
        {
            for (uint i = 0; i < game_state.giraffes.length(); ++i) {
                Giraffe@ other_giraffe = @game_state.giraffes[i];
                if (other_giraffe !is giraffe && !other_giraffe.dead) {
                    glm::vec2 offset = other_giraffe.mob.position - giraffe.mob.position;
                    const float distance_squared = glm::length2(offset);
                    const float near_distance_squared = (giraffe.mob.radius + other_giraffe.mob.radius) * (giraffe.mob.radius + other_giraffe.mob.radius);
                    if (distance_squared <= near_distance_squared) {
                        float distance = sqrt(distance_squared);
                        offset /= distance;
                        offset *= (giraffe.mob.radius + other_giraffe.mob.radius);
                        separation_force -= offset;
                    }
                }
            }

            separation_force *= separation_weight;
        }

        // avoidance
        {
            avoidance_force = avoidance_behavior(giraffe.mob, engine);
            avoidance_force *= avoidance_weight;
        }
    }

    giraffe.mob.steering_direction = glm::truncate(arrival_force + flee_force + separation_force + avoidance_force, giraffe.mob.max_force);

    update_mob(giraffe.mob, game, dt);

    const engine::AtlasFrame@ giraffe_frame = engine::atlas_frame(game.sprites.atlas, "giraffe");

    glm::mat4 transform = glm::mat4(1.0f);
    bool flip_x = giraffe.mob.velocity.x <= 0.0f;
    bool flip_y = giraffe.dead;

    float x_offset = giraffe_frame.rect.size.x * giraffe_frame.pivot.x;
    float y_offset = giraffe_frame.rect.size.y * (1.0f - giraffe_frame.pivot.y);

    if (flip_x) {
        x_offset *= -1.0f;
    }

    if (flip_y) {
        y_offset *= -1.0f;
    }

    transform = glm::translate(transform, glm::vec3(
        floor(giraffe.mob.position.x - x_offset),
        floor(giraffe.mob.position.y - y_offset),
        GIRAFFE_Z_LAYER));
    transform = glm::scale(transform, glm::vec3((flip_x ? -1.0f : 1.0f) * giraffe_frame.rect.size.x, (flip_y ? -1.0f : 1.0f) * giraffe_frame.rect.size.y, 1.0f));
    engine::transform_sprite(game.sprites, giraffe.sprite_id, math::Matrix4f(transform));
}

void update_lion(Lion@ lion, engine::Engine@ engine, game::Game@ game, float t, float dt) {
    if (lion.locked_giraffe is null) {
        if (lion.energy >= lion.max_energy) {
            Giraffe@ found_giraffe = null;
            float distance = 1000000.0f; // large enough
            for (uint i = 0; i < game_state.giraffes.length(); ++i) {
                Giraffe@ giraffe = @game_state.giraffes[i];
                if (!giraffe.dead) {
                    float d = glm::length(giraffe.mob.position - lion.mob.position);
                    if (d < distance) {
                        distance = d;
                        @found_giraffe = @giraffe;
                    }
                }
            }

            if (found_giraffe !is null) {
                @lion.locked_giraffe = found_giraffe;
                lion.energy = lion.max_energy;
            }
        } else {
            lion.energy += dt * 2.0f;
        }
    }

    if (lion.locked_giraffe !is null) {
        lion.mob.steering_target = lion.locked_giraffe.mob.position;

        glm::vec2 pursue_force = glm::vec2(0.0f, 0.0f);
        float pursue_weight = 1.0f;

        glm::vec2 avoidance_force = glm::vec2(0.0f, 0.0f);
        float avoidance_weight = 10.0f;

        // Pursue
        {
            glm::vec2 desired_velocity = glm::normalize(lion.locked_giraffe.mob.position - lion.mob.position) * lion.mob.max_speed;
            pursue_force = desired_velocity - lion.mob.velocity;
            pursue_force *= pursue_weight;
        }

        // Avoidance
        {
            avoidance_force = avoidance_behavior(lion.mob, engine);
            avoidance_force *= avoidance_weight;
        }

        lion.mob.steering_direction = glm::truncate(pursue_force + avoidance_force, lion.mob.max_force);

        if (glm::length(lion.locked_giraffe.mob.position - lion.mob.position) <= lion.mob.radius) {
            lion.locked_giraffe.dead = true;
            engine::color_sprite(game.sprites, lion.locked_giraffe.sprite_id, engine::color::pico8::light_gray);

            @lion.locked_giraffe = null;
            lion.energy = 0.0f;
            lion.mob.steering_direction = glm::vec2(0.0f, 0.0f);
        } else {
            lion.energy -= dt;
            if (lion.energy <= 0.0f) {
                @lion.locked_giraffe = null;
                lion.energy = 0.0f;
                lion.mob.steering_direction = glm::vec2(0.0f, 0.0f);
            }
        }
    }

    update_mob(lion.mob, game, dt);

    const engine::AtlasFrame@ lion_frame = engine::atlas_frame(game.sprites.atlas, "lion");

    glm::mat4 transform = glm::mat4(1.0f);
    bool flip = false;
    if (lion.locked_giraffe !is null) {
        if (lion.locked_giraffe.mob.position.x < lion.mob.position.x) {
            flip = true;
        }
    }

    float x_offset = lion_frame.rect.size.x * lion_frame.pivot.x;
    if (flip) {
        x_offset *= -1.0f;
    }

    transform = glm::translate(transform, glm::vec3(
        floor(lion.mob.position.x - x_offset),
        floor(lion.mob.position.y - lion_frame.rect.size.y * (1.0f - lion_frame.pivot.y)),
        LION_Z_LAYER));
    transform = glm::scale(transform, glm::vec3((flip ? -1.0f : 1.0f) * lion_frame.rect.size.x, lion_frame.rect.size.y, 1.0f));
    engine::transform_sprite(game.sprites, lion.sprite_id, math::Matrix4f(transform));
}

void update(engine::Engine@ engine, game::Game@ game, float t, float dt) {
    for (uint i = 0; i < game_state.giraffes.length(); ++i) {
        Giraffe@ other_giraffe = @game_state.giraffes[i];
        update_giraffe(other_giraffe, engine, game, dt);
    }

    update_lion(game_state.lion, engine, game, t, dt);

    engine::update_sprites(game.sprites, t, dt);
    engine::commit_sprites(game.sprites);
}

void render(engine::Engine@ engine, game::Game@ game) {
    engine::render_sprites(engine, game.sprites);
}

void render_imgui(engine::Engine@ engine, game::Game@ game) {
    ImDrawList@ draw_list = ImGui::GetForegroundDrawList();
    
    for (uint i = 0; i < game_state.obstacles.length(); ++i) {
        Obstacle@ obstacle = game_state.obstacles[i];
        uint obstacle_color = ImGui::IM_COL32(obstacle.color.r * 255, obstacle.color.g * 255, obstacle.color.b * 255, 255);
        ImVec2 position;
        position.x = obstacle.position.x;
        position.y = engine.window_rect.size.y - obstacle.position.y;
        draw_list.AddCircle(position, obstacle.radius, obstacle_color, 0, 2.0f);
    }
}
