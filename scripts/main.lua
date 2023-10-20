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

local game_state = {
    giraffes = {},
    obstacles = {},
    food = Food:new(),
    lion = {},
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

    -- Spawn trees

    -- Spawn giraffes
    spawn_giraffes(engine, game, 3)

    -- Spawn food

    -- Spawn lion
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
        end
    end
end


local update_mod = function(mob, game, dt)
end

local arrival_behavior = function(mob, target_position, speed_ramp_distance)
end

local avoidance_behavior = function(mob, engine, game)
end

local update_giraffe = function(giraffe, engine, game, dt)
end

local update_lion = function(lion, engine, game, t, dt)
end

function update(engine, game, t, dt)
    -- update giraffes
    -- update lion

    Engine.update_sprites(game.sprites, t, dt)
    Engine.commit_sprites(game.sprites)
end

function render(engine, game)
    Engine.render_sprites(engine, game.sprites)
end

function render_imgui()
end
