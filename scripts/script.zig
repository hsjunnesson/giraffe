const std = @import("std");

const c = @cImport({
    @cDefine("RND_IMPLEMENTATION", "1");
    @cInclude("rnd.h");
    @cInclude("zig_includes.h");
});

const Engine = extern struct {};
const Game = extern struct {};
const Sprites = extern struct {};

const Mob = struct {
    mass: f32 = 100.0,
    position: c.Vector2f = c.Vector2f{ .x = 0, .y = 0 },
    velocity: c.Vector2f = c.Vector2f{ .x = 0, .y = 0 },
    steering_direction: c.Vector2f = c.Vector2f{ .x = 0, .y = 0 },
    steering_target: c.Vector2f = c.Vector2f{ .x = 0, .y = 0 },
    max_force: f32 = 30.0,
    max_speed: f32 = 30.0,
    orientation: f32 = 0.0,
    radius: f32 = 10.0,
};

const Giraffe = struct {
    sprite_id: u64 = 0,
    mob: Mob = Mob{},
    dead: bool = false,
};

const Lion = struct {
    sprite_id: u64 = 0,
    mob: Mob = Mob{},
    locked_giraffe: ?*Giraffe = null,
    energy: f32 = 0,
    max_energy: f32 = 10,
};

const Food = struct {
    sprite_id: u64 = 0,
    position: c.Vector2f = c.Vector2f{ .x = 0, .y = 0 },
};

const Obstacle = struct {
    position: c.Vector2f = c.Vector2f{ .x = 0, .y = 0 },
    radius: f32 = 100.0,
    color: c.Color4f = c.Color4f{ .r = 0, .g = 0, .b = 0, .a = 0 },
};

var gpa = std.heap.GeneralPurposeAllocator(.{}){};
const allocator = gpa.allocator();

var g_giraffes = std.ArrayList(Giraffe).init(allocator);
var g_obstacles = std.ArrayList(Obstacle).init(allocator);
var g_food: Food = Food{};
var g_lion: Lion = Lion{};

var RANDOM_DEVICE: c.rnd_pcg_t = undefined;
const LION_Z_LAYER: i32 = -1;
const GIRAFFE_Z_LAYER: i32 = -2;
const FOOD_Z_LAYER: i32 = -3;

fn update_mob(mob: *Mob, game: *Game, dt: f32) void {
    _ = game;

    var drag: f32 = 1.0;

    for (g_obstacles.items) |*obstacle| {
        const length = c.length_vec2(c.subtract_vec2(obstacle.position, mob.position));
        if (length <= obstacle.radius) {
            drag = 10.0;
            break;
        }
    }

    const drag_force: c.Vector2f = c.multiply_vec2_factor(mob.velocity, -drag);
    const steering_force: c.Vector2f = c.add_vec2(c.truncate_vec2(mob.steering_direction, mob.max_force), drag_force);
    const acceleration: c.Vector2f = c.divide_vec2(steering_force, mob.mass);

    mob.velocity = c.truncate_vec2(c.add_vec2(mob.velocity, acceleration), mob.max_speed);
    mob.position = c.add_vec2(mob.position, c.multiply_vec2_factor(mob.velocity, dt));
}

fn arrival_behavior(mob: *Mob, target_position: c.Vector2f, speed_ramp_distance: f32) c.Vector2f {
    const target_offset = c.subtract_vec2(target_position, mob.position);
    const distance = c.length_vec2(target_offset);
    const ramped_speed = mob.max_speed * (distance / speed_ramp_distance);
    const clipped_speed = @min(ramped_speed, mob.max_speed);
    const desired_velocity = c.multiply_vec2_factor(target_offset, clipped_speed / distance);
    return c.subtract_vec2(desired_velocity, mob.velocity);
}

fn avoidance_behavior(mob: *Mob) c.Vector2f {
    const look_ahead_distance: f32 = 200.0;
    const origin = mob.position;
    const target_distance = c.length_vec2(c.subtract_vec2(mob.steering_target, origin));
    const forward = c.normalize_vec2(c.subtract_vec2(mob.steering_target, origin));

    const left_vector = c.Vector2f{ .x = -forward.y, .y = forward.x };
    const right_vector = c.Vector2f{ .x = forward.y, .y = -forward.x };

    const left_start = c.add_vec2(origin, c.multiply_vec2_factor(left_vector, mob.radius));
    const right_start = c.add_vec2(origin, c.multiply_vec2_factor(right_vector, mob.radius));

    var left_intersects: bool = false;
    var left_intersection: ?c.Vector2f = null;
    var left_intersection_distance: f32 = 100000.0;

    var right_intersects: bool = false;
    var right_intersection: ?c.Vector2f = null;
    var right_intersection_distance: f32 = 100000.0;

    // check against each obstacle
    for (g_obstacles.items) |obstacle| {
        var li = c.Vector2f{ .x = 0, .y = 0 };
        const did_li = c.ray_circle_intersection_vec2(left_start, forward, obstacle.position, obstacle.radius, &li);
        if (did_li) {
            const distance = c.length_vec2(c.subtract_vec2(li, left_start));
            if (distance <= look_ahead_distance and distance < left_intersection_distance) {
                left_intersects = true;
                left_intersection = li;
                left_intersection_distance = distance;
            }
        }

        var ri = c.Vector2f{ .x = 0, .y = 0 };
        const did_ri = c.ray_circle_intersection_vec2(right_start, forward, obstacle.position, obstacle.radius, &ri);
        if (did_ri) {
            const distance = c.length_vec2(c.subtract_vec2(ri, right_start));
            if (distance <= look_ahead_distance and distance < right_intersection_distance) {
                right_intersects = true;
                right_intersection = ri;
                right_intersection_distance = distance;
            }
        }
    }

    var avoidance_force = c.Vector2f{ .x = 0, .y = 0 };

    if (left_intersects or right_intersects) {
        if (target_distance <= left_intersection_distance and target_distance <= right_intersection_distance) {
            return c.Vector2f{ .x = 0, .y = 0 };
        }

        if (left_intersection_distance < right_intersection_distance) {
            const ratio = left_intersection_distance / look_ahead_distance;
            avoidance_force = c.multiply_vec2_factor(right_vector, (1.0 - ratio) * 50);
        } else {
            const ratio = right_intersection_distance / look_ahead_distance;
            avoidance_force = c.multiply_vec2_factor(left_vector, (1.0 - ratio) * 50);
        }
    }

    return avoidance_force;
}

fn update_giraffe(giraffe: *Giraffe, engine: *Engine, game: *Game, dt: f32) void {
    var arrival_force = c.Vector2f{ .x = 0, .y = 0 };
    const arrival_weight: f32 = 1.0;

    var flee_force = c.Vector2f{ .x = 0, .y = 0 };
    const flee_weight: f32 = 1.0;

    var separation_force = c.Vector2f{ .x = 0, .y = 0 };
    const separation_weight: f32 = 10.0;

    var avoidance_force = c.Vector2f{ .x = 0, .y = 0 };
    const avoidance_weight: f32 = 10.0;

    if (!giraffe.dead) {
        var is_hunted: bool = false;

        const distance_to_lion = c.length_vec2(c.subtract_vec2(g_lion.mob.position, giraffe.mob.position));
        if (distance_to_lion <= 300) {
            is_hunted = true;
        }

        // flee
        if (is_hunted) {
            const flee_direction = c.subtract_vec2(giraffe.mob.position, g_lion.mob.position);
            const steering_target = c.add_vec2(giraffe.mob.position, flee_direction);
            giraffe.mob.steering_target = steering_target;

            const desired_velocity = c.multiply_vec2_factor(c.normalize_vec2(flee_direction), giraffe.mob.max_speed);
            flee_force = c.subtract_vec2(desired_velocity, giraffe.mob.velocity);

            var boundary_avoidance_force = c.Vector2f{ .x = 0, .y = 0 };
            const buffer_distance: f32 = 100;

            const window_rect = c.glfw_window_rect(engine);
            const window_res_w: f32 = @floatFromInt(window_rect.size.x);
            const window_res_h: f32 = @floatFromInt(window_rect.size.y);

            if (giraffe.mob.position.x <= buffer_distance) {
                boundary_avoidance_force.x = buffer_distance - giraffe.mob.position.x;
            } else if (giraffe.mob.position.x >= window_res_w - buffer_distance) {
                boundary_avoidance_force.x = window_res_w - buffer_distance - giraffe.mob.position.x;
            }

            if (giraffe.mob.position.y <= buffer_distance) {
                boundary_avoidance_force.y = buffer_distance - giraffe.mob.position.y;
            } else if (giraffe.mob.position.y >= window_res_h - buffer_distance) {
                boundary_avoidance_force.y = window_res_h - buffer_distance - giraffe.mob.position.y;
            }

            boundary_avoidance_force = c.multiply_vec2_factor(boundary_avoidance_force, 20);

            flee_force = c.add_vec2(flee_force, boundary_avoidance_force);
            flee_force = c.multiply_vec2_factor(flee_force, flee_weight);
        } else {
            // arrival
            giraffe.mob.steering_target = g_food.position;
            arrival_force = arrival_behavior(&giraffe.mob, g_food.position, 100);
            arrival_force = c.multiply_vec2_factor(arrival_force, arrival_weight);
        }

        // separation
        for (g_giraffes.items) |*other_giraffe| {
            if (giraffe != other_giraffe and !other_giraffe.dead) {
                var offset = c.subtract_vec2(other_giraffe.mob.position, giraffe.mob.position);
                const distance_squared = c.length2_vec2(offset);
                const near_distance_squared = (giraffe.mob.radius + other_giraffe.mob.radius) * giraffe.mob.radius + other_giraffe.mob.radius;
                if (distance_squared <= near_distance_squared) {
                    const distance = std.math.sqrt(distance_squared);
                    offset = c.divide_vec2(offset, distance);
                    offset = c.multiply_vec2_factor(offset, giraffe.mob.radius + other_giraffe.mob.radius);
                    separation_force = c.subtract_vec2(separation_force, offset);
                }
            }
        }
        separation_force = c.multiply_vec2_factor(separation_force, separation_weight);

        // avoidance
        avoidance_force = avoidance_behavior(&giraffe.mob);
        avoidance_force = c.multiply_vec2_factor(avoidance_force, avoidance_weight);
    }

    const steering_direction = c.add_vec2(c.add_vec2(c.add_vec2(arrival_force, flee_force), separation_force), avoidance_force);
    giraffe.mob.steering_direction = c.truncate_vec2(steering_direction, giraffe.mob.max_force);

    update_mob(&giraffe.mob, game, dt);

    const sprites = c.get_sprites(game);
    const giraffe_frame = c.Engine_atlas_frame(c.get_atlas(game), "giraffe");
    const frame_rect_w = @as(f32, @floatFromInt(giraffe_frame.*.rect.size.x));
    const frame_rect_h = @as(f32, @floatFromInt(giraffe_frame.*.rect.size.y));

    var transform: c.Matrix4f = c.Matrix4f_Identity();
    const flip_x: bool = giraffe.mob.velocity.x <= 0.0;
    const flip_y: bool = giraffe.dead == true;
    var x_offset: f32 = frame_rect_w * giraffe_frame.*.pivot.x;
    var y_offset: f32 = frame_rect_h * (1.0 - giraffe_frame.*.pivot.y);

    if (flip_x) {
        x_offset *= -1.0;
    }

    if (flip_y) {
        y_offset *= -1.0;
    }

    transform = c.translate(transform, c.Vector3f{
        .x = giraffe.mob.position.x - x_offset,
        .y = giraffe.mob.position.y - y_offset,
        .z = GIRAFFE_Z_LAYER,
    });

    const scale_x = if (flip_x) -1.0 * frame_rect_w else frame_rect_w;
    const scale_y = if (flip_y) -1.0 * frame_rect_h else frame_rect_h;

    transform = c.scale(transform, c.Vector3f{
        .x = scale_x,
        .y = scale_y,
        .z = 1.0,
    });

    c.Engine_transform_sprite(sprites, giraffe.sprite_id, transform);
}

fn update_lion(lion: *Lion, engine: *Engine, game: *Game, dt: f32) void {
    _ = engine;

    if (lion.locked_giraffe == null) {
        if (lion.energy >= lion.max_energy) {
            var found_giraffe: ?*Giraffe = null;
            var distance: f32 = 1000000.0;
            for (g_giraffes.items) |*giraffe| {
                if (!giraffe.dead) {
                    const d = c.length_vec2(c.subtract_vec2(giraffe.mob.position, lion.mob.position));
                    if (d < distance) {
                        distance = d;
                        found_giraffe = giraffe;
                    }
                }
            }

            if (found_giraffe) |giraffe| {
                lion.locked_giraffe = giraffe;
                lion.energy = lion.max_energy;
            }
        } else {
            lion.energy += dt * 2;
        }
    }

    if (lion.locked_giraffe) |locked_giraffe| {
        lion.mob.steering_target = locked_giraffe.mob.position;

        var pursue_force = c.Vector2f{ .x = 0, .y = 0 };
        const pursue_weight: f32 = 1.0;

        var avoidance_force = c.Vector2f{ .x = 0, .y = 0 };
        const avoidance_weight: f32 = 1.0;

        // pursue
        {
            var desired_velocity = c.normalize_vec2(c.subtract_vec2(locked_giraffe.mob.position, lion.mob.position));
            desired_velocity = c.multiply_vec2_factor(desired_velocity, lion.mob.max_speed);
            pursue_force = c.subtract_vec2(desired_velocity, lion.mob.velocity);
            pursue_force = c.multiply_vec2_factor(pursue_force, pursue_weight);
        }

        // avoidance
        {
            avoidance_force = avoidance_behavior(&lion.mob);
            avoidance_force = c.multiply_vec2_factor(avoidance_force, avoidance_weight);
        }

        lion.mob.steering_direction = c.truncate_vec2(c.add_vec2(pursue_force, avoidance_force), lion.mob.max_force);

        if (c.length_vec2(c.subtract_vec2(locked_giraffe.mob.position, lion.mob.position)) <= lion.mob.radius) {
            locked_giraffe.dead = true;
            // color sprite
            lion.locked_giraffe = null;
            lion.energy = 0;
            lion.mob.steering_direction = c.Vector2f{ .x = 0, .y = 0 };
        } else {
            lion.energy = lion.energy - dt;
            if (lion.energy <= 0.0) {
                lion.locked_giraffe = null;
                lion.energy = 0.0;
                lion.mob.steering_direction = c.Vector2f{ .x = 0, .y = 0 };
            }
        }
    }

    update_mob(&lion.mob, game, dt);

    const sprites = c.get_sprites(game);
    const lion_frame = c.Engine_atlas_frame(c.get_atlas(game), "lion");
    const frame_rect_w = @as(f32, @floatFromInt(lion_frame.*.rect.size.x));
    const frame_rect_h = @as(f32, @floatFromInt(lion_frame.*.rect.size.y));

    var transform: c.Matrix4f = c.Matrix4f_Identity();
    const flip_x: bool = lion.mob.velocity.x <= 0.0;
    var x_offset: f32 = frame_rect_w * lion_frame.*.pivot.x;
    const y_offset: f32 = frame_rect_h * (1.0 - lion_frame.*.pivot.y);

    if (flip_x) {
        x_offset *= -1.0;
    }

    transform = c.translate(transform, c.Vector3f{
        .x = lion.mob.position.x - x_offset,
        .y = lion.mob.position.y - y_offset,
        .z = LION_Z_LAYER,
    });

    const scale_x = if (flip_x) -1.0 * frame_rect_w else frame_rect_w;
    const scale_y = frame_rect_h;

    transform = c.scale(transform, c.Vector3f{
        .x = scale_x,
        .y = scale_y,
        .z = 1.0,
    });

    c.Engine_transform_sprite(sprites, lion.sprite_id, transform);
}

export fn script_enter(engine: *Engine, game: *Game) void {
    c.rnd_pcg_seed(&RANDOM_DEVICE, 100);

    const window_rect = c.glfw_window_rect(engine);
    const window_res_w: f32 = @floatFromInt(window_rect.size.x);
    const window_res_h: f32 = @floatFromInt(window_rect.size.y);
    const sprites = c.get_sprites(game);

    // Spawn lake
    {
        const x: f32 = (window_res_w / 2) + 200 * c.rnd_pcg_nextf(&RANDOM_DEVICE) - 100;
        const y: f32 = (window_res_h / 2) + 200 * c.rnd_pcg_nextf(&RANDOM_DEVICE) - 100;

        var obstacle = Obstacle{};
        obstacle.position = c.Vector2f{ .x = x, .y = y };
        obstacle.color = c.blue;
        obstacle.radius = 100 + c.rnd_pcg_nextf(&RANDOM_DEVICE) * 100;

        g_obstacles.append(obstacle) catch unreachable;
    }

    // Spawn trees
    for (0..10) |_| {
        const x: f32 = 10 + c.rnd_pcg_nextf(&RANDOM_DEVICE) * (window_res_w - 20);
        const y: f32 = 10 + c.rnd_pcg_nextf(&RANDOM_DEVICE) * (window_res_h - 20);

        var obstacle = Obstacle{};
        obstacle.position = c.Vector2f{ .x = x, .y = y };
        obstacle.color = c.light_gray;
        obstacle.radius = 20;

        g_obstacles.append(obstacle) catch unreachable;
    }

    // Spawn giraffes
    for (0..1000) |_| {
        const x: f32 = c.rnd_pcg_nextf(&RANDOM_DEVICE) * (window_res_w - 100);
        const y: f32 = c.rnd_pcg_nextf(&RANDOM_DEVICE) * (window_res_h - 100);

        var giraffe: Giraffe = Giraffe{ .mob = Mob{ .mass = 100, .max_force = 1000, .max_speed = 300, .radius = 20, .position = c.Vector2f{
            .x = 50 + x,
            .y = 50 + y,
        } } };

        const sprite = c.Engine_add_sprite(sprites, "giraffe", c.orange);
        giraffe.sprite_id = sprite.id;

        g_giraffes.append(giraffe) catch unreachable;
    }

    // Spawn food
    {
        const sprite = c.Engine_add_sprite(sprites, "food", c.green);
        g_food.sprite_id = sprite.id;
        g_food.position = c.Vector2f{
            .x = 0.25 * window_res_w,
            .y = 0.25 * window_res_h,
        };

        var transform: c.Matrix4f = c.Matrix4f_Identity();

        transform = c.translate(transform, c.Vector3f{
            .x = g_food.position.x,
            .y = g_food.position.y,
            .z = FOOD_Z_LAYER,
        });

        transform = c.scale(transform, c.Vector3f{
            .x = @as(f32, @floatFromInt(sprite.atlas_frame.*.rect.size.x)),
            .y = @as(f32, @floatFromInt(sprite.atlas_frame.*.rect.size.y)),
            .z = 1.0,
        });

        c.Engine_transform_sprite(sprites, g_food.sprite_id, transform);
    }

    // Spawn lion
    {
        const sprite = c.Engine_add_sprite(sprites, "lion", c.yellow);
        g_lion.sprite_id = sprite.id;
        g_lion.mob = Mob{ .mass = 25, .max_force = 1000, .max_speed = 400, .radius = 20, .position = c.Vector2f{
            .x = 0.75 * window_res_w,
            .y = 0.75 * window_res_h,
        } };
    }
}

export fn script_leave(engine: *Engine, game: *Game) void {
    _ = engine;
    _ = game;
}

export fn script_on_input() void {}

export fn script_update(engine: *Engine, game: *Game, t: f32, dt: f32) void {
    for (g_giraffes.items) |*giraffe| {
        update_giraffe(giraffe, engine, game, dt);
    }

    update_lion(&g_lion, engine, game, dt);

    const sprites = c.get_sprites(game);
    c.Engine_update_sprites(sprites, t, dt);
    c.Engine_commit_sprites(sprites);
}

export fn script_render(engine: *Engine, game: *Game) void {
    const sprites = c.get_sprites(game);
    c.Engine_render_sprites(engine, sprites);
}

export fn script_render_imgui(engine: *Engine, game: *Game) void {
    _ = engine;
    _ = game;
}
