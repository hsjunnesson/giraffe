local dbg = require("scripts/debugger")
local class = require("scripts/middleclass")

local Mob = class("Mob")
function Mob:initialize()
    self.mass = 100
    self.position = Glm.vec2.new(0, 0)
    self.velocity = Glm.vec2.new(0, 0)
    self.steering_direction = Glm.vec2.new(0, 0)
    self.steering_target = Glm.vec2.new(0, 0)
    self.max_force = 30
    self.max_speed = 30
    self.orientation = 0
    self.radius = 10
end

local Giraffe = class("Giraffe")
function Giraffe:initialize()
    self.sprite_id = 0
    self.mob = Mob:new()
    self.dead = false
end

local Lion = class("Lion")
function Lion:initialize()
    self.sprite_id = 0
    self.mob = Mob:new()
    self.locked_giraffe = nil
    self.energy = 0
    self.max_energy = 10
end

local Food = class("Food")
function Food:initialize()
    self.sprite_id = 0
    self.position = Glm.vec2.new(0, 0)
end

local Obstacle = class("Obstacle")
function Obstacle:initialize()
    self.position = Glm.vec2.new(0, 0)
    self.radius = 100
    self.color = Math.Color4f.new(1, 1, 1, 1)
end

local RANDOM_DEVICE = rnd_pcg_t.new()
local LION_Z_LAYER = -1
local GIRAFFE_Z_LAYER = -2
local FOOD_Z_LAYER = -3

local game_state = {
    giraffes = {},
    obstacles = {},
    food = Food:new(),
    lion = Lion:new(),
    debug_draw = false,
    debug_avoidance = false,
}

local spawn_giraffes = function(engine, game, num_giraffes)
    for _ = 1, num_giraffes do
        local giraffe = Giraffe:new()
        giraffe.mob.mass = 100
        giraffe.mob.max_force = 1000
        giraffe.mob.max_speed = 300
        giraffe.mob.radius = 20
        giraffe.mob.position = Glm.vec2.new(
            50 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.x - 100),
            50 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.y - 100)
        )

        local sprite = Engine.add_sprite(game.sprites, "giraffe", Engine.Color.Pico8.orange)
        giraffe.sprite_id = sprite:identifier()
        table.insert(game_state.giraffes, giraffe)
    end
end

function on_enter(engine, game)
    local time = os.time()
    rnd_pcg_seed(RANDOM_DEVICE, time)

    -- Spawn lake
    do
        local lake_obstacle = Obstacle:new()
        lake_obstacle.position = Glm.vec2.new(
            (engine.window_rect.size.x / 2) + 200 * rnd_pcg_nextf(RANDOM_DEVICE) - 100,
            (engine.window_rect.size.y / 2) + 200 * rnd_pcg_nextf(RANDOM_DEVICE) - 100
        )
        lake_obstacle.color = Engine.Color.Pico8.blue
        lake_obstacle.radius = 100 + rnd_pcg_nextf(RANDOM_DEVICE) * 100
        table.insert(game_state.obstacles, lake_obstacle)
    end

    -- Spawn trees
    for _ = 1, 10 do
        local obstacle = Obstacle:new()
        obstacle.position = Glm.vec2.new(
            10 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.x - 20),
            10 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.y - 20)
        )
        obstacle.color = Engine.Color.Pico8.light_gray
        obstacle.radius = 20
        table.insert(game_state.obstacles, obstacle)
    end

    -- Spawn giraffes
    spawn_giraffes(engine, game, 3)

    -- Spawn food
    do
        local sprite = Engine.add_sprite(game.sprites, "food", Engine.Color.Pico8.green)
        game_state.food.sprite_id = sprite:identifier()
        game_state.food.position = Glm.vec2.new(0.25 * engine.window_rect.size.x, 0.25 * engine.window_rect.size.y)

        local transform = Glm.mat4.new(1.0)
        transform = Glm.translate(transform, Glm.vec3.new(
            math.floor(game_state.food.position.x - sprite.atlas_frame.rect.size.x * sprite.atlas_frame.pivot.x),
            math.floor(game_state.food.position.y - sprite.atlas_frame.rect.size.y * (1.0 - sprite.atlas_frame.pivot.y)),
            FOOD_Z_LAYER
        ))
        transform = Glm.scale(transform, Glm.vec3.new(sprite.atlas_frame.rect.size.x, sprite.atlas_frame.rect.size.y, 1.0))

        local matrix = Glm.to_Matrix4f(transform)
        Engine.transform_sprite(game.sprites, game_state.food.sprite_id, matrix)
    end

    -- Spawn lion
    do
        local sprite = Engine.add_sprite(game.sprites, "lion", Engine.Color.Pico8.yellow)
        game_state.lion.sprite_id = sprite:identifier()
        game_state.lion.mob.position = Glm.vec2.new(engine.window_rect.size.x * 0.75, engine.window_rect.size.y * 0.75)
        game_state.lion.mob.mass = 25
        game_state.lion.mob.max_force = 1000
        game_state.lion.mob.max_speed = 400
        game_state.lion.mob.radius = 20
    end
end

function on_leave(engine, game)
end

function on_input(engine, game, input_command)
    if input_command.input_type == Engine.InputType.Key then
        local pressed = input_command.key_state.trigger_state == Engine.TriggerState.Pressed
        local repeated = input_command.key_state.trigger_state == Engine.TriggerState.Repeated

        local bind_action_key = Engine.action_key_for_input_command(input_command)
        if not bind_action_key:valid() then
            return
        end

        local action_hash = Hash.get_identifier(game.action_binds.bind_actions, bind_action_key, Game.ActionHash.NONE)

        if action_hash == Game.ActionHash.NONE then
            return
        elseif action_hash == Game.ActionHash.QUIT then
           if pressed then
               Game.transition(engine, game, Game.AppState.Quitting)
           end
        elseif action_hash == Game.ActionHash.DEBUG_DRAW then
            if pressed then
                game_state.debug_draw = not game_state.debug_draw
            end
        elseif action_hash == Game.ActionHash.DEBUG_AVOIDANCE then
            if pressed then
                game_state.debug_avoidance = not game_state.debug_avoidance
            end
        elseif action_hash == Game.ActionHash.ADD_ONE then
            if pressed or repeated then
                spawn_giraffes(engine, game, 1)
            end
        elseif action_hash == Game.ActionHash.ADD_FIVE then
            if pressed or repeated then
                spawn_giraffes(engine, game, 5)
            end
        elseif action_hash == Game.ActionHash.ADD_TEN then
            if pressed or repeated then
                spawn_giraffes(engine, game, 10)
            end
        end
    elseif input_command.input_type == Engine.InputType.Mouse then
        if input_command.mouse_state.mouse_left_state == Engine.TriggerState.Pressed then
            local x = input_command.mouse_state.mouse_position.x
            local y = engine.window_rect.size.y - input_command.mouse_state.mouse_position.y
            game_state.food.position = Glm.vec2.new(x, y)

            local sprite = Engine.get_sprite(game.sprites, game_state.food.sprite_id)
            local transform = Glm.mat4.new(1.0)
            transform = Glm.translate(transform, Glm.vec3.new(
                math.floor(game_state.food.position.x - sprite.atlas_frame.rect.size.x * sprite.atlas_frame.pivot.x),
                math.floor(game_state.food.position.y - sprite.atlas_frame.rect.size.y * (1.0 - sprite.atlas_frame.pivot.y)),
                FOOD_Z_LAYER
            ))
            transform = Glm.scale(transform, Glm.vec3.new(sprite.atlas_frame.rect.size.x, sprite.atlas_frame.rect.size.y, 1.0))

            local matrix = Glm.to_Matrix4f(transform)
            Engine.transform_sprite(game.sprites, game_state.food.sprite_id, matrix)
        end
    end
end


local update_mob = function(mob, game, dt)
end

local arrival_behavior = function(mob, target_position, speed_ramp_distance)
end

local avoidance_behavior = function(mob, engine, game)
end

local update_giraffe = function(giraffe, engine, game, dt)
    -- TODO: Implement


    local giraffe_frame = Engine.atlas_frame(game.sprites.atlas, "giraffe")

    local transform = Glm.mat4.new(1.0)
    local flip_x = giraffe.mob.velocity.x <= 0.0
    local flip_y = giraffe.dead == true

    local x_offset = giraffe_frame.rect.size.x * giraffe_frame.pivot.x
    local y_offset = giraffe_frame.rect.size.y * (1.0 - giraffe_frame.pivot.y)

    if flip_x then
        x_offset = x_offset * -1.0
    end

    if flip_y then
        y_offset = y_offset * -1.0
    end

    transform = Glm.translate(transform, Glm.vec3.new(
        math.floor(giraffe.mob.position.x - x_offset),
        math.floor(giraffe.mob.position.y - y_offset),
        GIRAFFE_Z_LAYER
    ))
    transform = Glm.scale(transform, Glm.vec3.new((flip_x and -1.0 or 1.0) * giraffe_frame.rect.size.x, (flip_y and -1.0 or 1.0) * giraffe_frame.rect.size.y, 1.0))

    local matrix = Glm.to_Matrix4f(transform)
    Engine.transform_sprite(game.sprites, giraffe.sprite_id, matrix)
end

local update_lion = function(lion, engine, game, t, dt)
    -- TODO: Implement behavior

    local lion_frame = Engine.atlas_frame(game.sprites.atlas, "lion")

    local transform = Glm.mat4.new(1.0)
    local flip = lion.locked_giraffe and lion.locked_giraffe.mob_position.x < lion.mob.position.x

    local x_offset = lion_frame.rect.size.x * lion_frame.pivot.x

    if flip then
        x_offset = x_offset * -1.0
    end

    transform = Glm.translate(transform, Glm.vec3.new(
        math.floor(lion.mob.position.x - x_offset),
        math.floor(lion.mob.position.y - lion_frame.rect.size.y * (1.0 - lion_frame.pivot.y)),
        LION_Z_LAYER
    ))
    transform = Glm.scale(transform, Glm.vec3.new((flip and -1.0 or 1.0) * lion_frame.rect.size.x, lion_frame.rect.size.y, 1.0))

    local matrix = Glm.to_Matrix4f(transform)
    Engine.transform_sprite(game.sprites, lion.sprite_id, matrix)
end

function update(engine, game, t, dt)
    for _, giraffe in ipairs(game_state.giraffes) do
        update_giraffe(giraffe, engine, game, dt)
    end

    update_lion(game_state.lion, engine, game, t, dt)

    Engine.update_sprites(game.sprites, t, dt)
    Engine.commit_sprites(game.sprites)
end

function render(engine, game)
    Engine.render_sprites(engine, game.sprites)
end

function render_imgui()
end
