local dbg = require("scripts/debugger")
--local profile = require("scripts/profile")

if jit then
    jit.off()
end

local ffi = jit and require("ffi")

if ffi then
    ffi.cdef[[
        typedef struct {
            float x;
            float y;
        } vec2;

        typedef struct {
            float x;
            float y;
            float z;
        } vec3;

        typedef struct {
            float m[4][4];
        } mat4;

        vec2 glm_add_vec2(const vec2 lhs, const vec2 rhs);
        vec2 glm_subtract_vec2(const vec2 lhs, const vec2 rhs);
        vec2 glm_multiply_vec2_vec2(const vec2 lhs, const vec2 rhs);
        vec2 glm_multiply_vec2_scalar(const vec2 lhs, const float rhs);
        vec2 glm_divide_vec2_scalar(const vec2 lhs, const float rhs);

        bool glm_ray_circle_intersection(const vec2 ray_origin, const vec2 ray_direction, const vec2 circle_center, float circle_radius, vec2 *intersection);
        bool glm_ray_line_intersection(const vec2 ray_origin, const vec2 ray_direction, const vec2 p1, const vec2 p2, vec2 *intersection);
        vec2 glm_truncate(const vec2 v, float max_length);
        vec2 glm_normalize(const vec2 v);
        float glm_length(const vec2 v);
        float glm_length2(const vec2 v);

        mat4 glm_identity_mat4();
        mat4 glm_translate(const mat4 m, const vec3 v);
        mat4 glm_scale(const mat4 m, const vec3 v);

        typedef struct {
            float mass;
            vec2 position;
            vec2 velocity;
            vec2 steering_direction;
            vec2 steering_target;
            float max_force;
            float max_speed;
            float orientation;
            float radius;
        } Mob;

        typedef struct {
            uint64_t sprite_id;
            Mob mob;
            bool dead;
        } Giraffe;

        typedef struct {
            uint64_t sprite_id;
            Mob mob;
            Giraffe *locked_giraffe;
            float energy;
            float max_energy;
        } Lion;

        typedef struct {
            uint64_t id;
        } Sprite;

        typedef struct Sprites Sprites;

        typedef struct {
            Sprites *sprites;
        } Game;

        uint64_t add_sprite(void **object, const char *sprite_name);
    ]]

    if rawget(_G, "Glm") == nil then
        Glm = {
            vec2 = ffi.metatype(ffi.typeof("vec2"), {
                __tostring = function(v)
                    return "vec2(" .. v.x .. ", " .. v.y .. ")"
                end,
                __add = function(lhs, rhs)
                    return ffi.C.glm_add_vec2(lhs, rhs)
                end,
                __sub = function(lhs, rhs)
                    return ffi.C.glm_subtract_vec2(lhs, rhs)
                end,
                __mul = function(lhs, rhs)
                    if type(lhs) == "number" and ffi.istype("vec2", rhs) then -- Multiply a scalar with vec2
                        return ffi.C.glm_multiply_vec2_scalar(rhs, lhs)
                    elseif ffi.istype("vec2", lhs) and type(rhs) == "number" then -- Multiply a vec2 with a scalar
                        return ffi.C.glm_multiply_vec2_scalar(lhs, rhs)
                    elseif ffi.istype("vec2", lhs) and ffi.istype("vec2", rhs) then -- Multiply two vec2 objects
                        return ffi.C.glm_multiply_vec2_vec2(lhs, rhs)
                    else
                        error("Invalid types for multiplication")
                    end
                end,
                __div = function(lhs, rhs)
                    return ffi.C.glm_divide_vec2_scalar(lhs, rhs)
                end
            }),
            vec3 = ffi.metatype(ffi.typeof("vec3"), {
                __tostring = function(v)
                    return "vec3(" .. v.x .. ", " .. v.y .. ", " .. v.z .. ")"
                end
            }),
            mat4 = ffi.metatype(ffi.typeof("mat4"), {
                __tostring = function(v)
                    return "mat4(...)"
                end,
                __new = function(ct, ...)
                    -- Check number of arguments to the constructor
                    local nargs = select("#", ...)
                    if nargs == 1 and type(select(1, ...)) == "number" and select(1, ...) == 1.0 then
                        -- If the argument is 1.0, use the identity matrix constructor
                        return ffi.C.glm_identity_mat4()
                    else
                        -- Otherwise, you could call a default constructor or handle other cases
                        return ffi.new(ct, ...)
                    end
                end
            }),
            ray_circle_intersection = function(ray_origin, ray_direction, circle_center, circle_radius)
                local intersection = ffi.new("vec2")
                local hit = ffi.C.glm_ray_circle_intersection(ray_origin, ray_direction, circle_center, circle_radius, intersection)
                return hit, intersection
            end,
            ray_line_intersection = function()
                local intersection = ffi.new("vec2")
                local hit = ffi.C.glm_ray_line_intersection(ray_origin, ray_direction, p1, p2, intersection)
                return hit, intersection
            end,
            truncate = ffi.C.glm_truncate,
            normalize = ffi.C.glm_normalize,
            length = ffi.C.glm_length,
            length2 = ffi.C.glm_length2,
            translate = ffi.C.glm_translate,
            scale = ffi.C.glm_scale,
        }
    end
end

--local class = require("scripts/middleclass")

local require_middleclass = function()
    local middleclass = {
        _VERSION     = 'middleclass v4.1.1',
        _DESCRIPTION = 'Object Orientation for Lua',
        _URL         = 'https://github.com/kikito/middleclass',
        _LICENSE     = [[
          MIT LICENSE
      
          Copyright (c) 2011 Enrique García Cota
      
          Permission is hereby granted, free of charge, to any person obtaining a
          copy of this software and associated documentation files (the
          "Software"), to deal in the Software without restriction, including
          without limitation the rights to use, copy, modify, merge, publish,
          distribute, sublicense, and/or sell copies of the Software, and to
          permit persons to whom the Software is furnished to do so, subject to
          the following conditions:
      
          The above copyright notice and this permission notice shall be included
          in all copies or substantial portions of the Software.
      
          THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
          OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
          MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
          IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
          CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
          TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
          SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
        ]]
      }
      
      local function _createIndexWrapper(aClass, f)
        if f == nil then
          return aClass.__instanceDict
        elseif type(f) == "function" then
          return function(self, name)
            local value = aClass.__instanceDict[name]
      
            if value ~= nil then
              return value
            else
              return (f(self, name))
            end
          end
        else -- if  type(f) == "table" then
          return function(self, name)
            local value = aClass.__instanceDict[name]
      
            if value ~= nil then
              return value
            else
              return f[name]
            end
          end
        end
      end
      
      local function _propagateInstanceMethod(aClass, name, f)
        f = name == "__index" and _createIndexWrapper(aClass, f) or f
        aClass.__instanceDict[name] = f
      
        for subclass in pairs(aClass.subclasses) do
          if rawget(subclass.__declaredMethods, name) == nil then
            _propagateInstanceMethod(subclass, name, f)
          end
        end
      end
      
      local function _declareInstanceMethod(aClass, name, f)
        aClass.__declaredMethods[name] = f
      
        if f == nil and aClass.super then
          f = aClass.super.__instanceDict[name]
        end
      
        _propagateInstanceMethod(aClass, name, f)
      end
      
      local function _tostring(self) return "class " .. self.name end
      local function _call(self, ...) return self:new(...) end
      
      local function _createClass(name, super)
        local dict = {}
        dict.__index = dict
      
        local aClass = { name = name, super = super, static = {},
                         __instanceDict = dict, __declaredMethods = {},
                         subclasses = setmetatable({}, {__mode='k'})  }
      
        if super then
          setmetatable(aClass.static, {
            __index = function(_,k)
              local result = rawget(dict,k)
              if result == nil then
                return super.static[k]
              end
              return result
            end
          })
        else
          setmetatable(aClass.static, { __index = function(_,k) return rawget(dict,k) end })
        end
      
        setmetatable(aClass, { __index = aClass.static, __tostring = _tostring,
                               __call = _call, __newindex = _declareInstanceMethod })
      
        return aClass
      end
      
      local function _includeMixin(aClass, mixin)
        assert(type(mixin) == 'table', "mixin must be a table")
      
        for name,method in pairs(mixin) do
          if name ~= "included" and name ~= "static" then aClass[name] = method end
        end
      
        for name,method in pairs(mixin.static or {}) do
          aClass.static[name] = method
        end
      
        if type(mixin.included)=="function" then mixin:included(aClass) end
        return aClass
      end
      
      local DefaultMixin = {
        __tostring   = function(self) return "instance of " .. tostring(self.class) end,
      
        initialize   = function(self, ...) end,
      
        isInstanceOf = function(self, aClass)
          return type(aClass) == 'table'
             and type(self) == 'table'
             and (self.class == aClass
                  or type(self.class) == 'table'
                  and type(self.class.isSubclassOf) == 'function'
                  and self.class:isSubclassOf(aClass))
        end,
      
        static = {
          allocate = function(self)
            assert(type(self) == 'table', "Make sure that you are using 'Class:allocate' instead of 'Class.allocate'")
            return setmetatable({ class = self }, self.__instanceDict)
          end,
      
          new = function(self, ...)
            assert(type(self) == 'table', "Make sure that you are using 'Class:new' instead of 'Class.new'")
            local instance = self:allocate()
            instance:initialize(...)
            return instance
          end,
      
          subclass = function(self, name)
            assert(type(self) == 'table', "Make sure that you are using 'Class:subclass' instead of 'Class.subclass'")
            assert(type(name) == "string", "You must provide a name(string) for your class")
      
            local subclass = _createClass(name, self)
      
            for methodName, f in pairs(self.__instanceDict) do
              if not (methodName == "__index" and type(f) == "table") then
                _propagateInstanceMethod(subclass, methodName, f)
              end
            end
            subclass.initialize = function(instance, ...) return self.initialize(instance, ...) end
      
            self.subclasses[subclass] = true
            self:subclassed(subclass)
      
            return subclass
          end,
      
          subclassed = function(self, other) end,
      
          isSubclassOf = function(self, other)
            return type(other)      == 'table' and
                   type(self.super) == 'table' and
                   ( self.super == other or self.super:isSubclassOf(other) )
          end,
      
          include = function(self, ...)
            assert(type(self) == 'table', "Make sure you that you are using 'Class:include' instead of 'Class.include'")
            for _,mixin in ipairs({...}) do _includeMixin(self, mixin) end
            return self
          end
        }
      }
      
      function middleclass.class(name, super)
        assert(type(name) == 'string', "A name (string) is needed for the new class")
        return super and super:subclass(name) or _includeMixin(_createClass(name), DefaultMixin)
      end
      
      setmetatable(middleclass, { __call = function(_, ...) return middleclass.class(...) end })
      
      return middleclass
end

local class = require_middleclass()

-- local Mob = class("Mob")
-- function Mob:initialize()
--     self.mass = 100
--     self.position = Glm.vec2(0, 0)
--     self.velocity = Glm.vec2(0, 0)
--     self.steering_direction = Glm.vec2(0, 0)
--     self.steering_target = Glm.vec2(0, 0)
--     self.max_force = 30
--     self.max_speed = 30
--     self.orientation = 0
--     self.radius = 10
-- end

-- local Giraffe = class("Giraffe")
-- function Giraffe:initialize()
--     self.sprite_id = 0
--     self.mob = Mob()
--     self.dead = false
-- end

-- local Lion = class("Lion")
-- function Lion:initialize()
--     self.sprite_id = 0
--     self.mob = Mob()
--     self.locked_giraffe = nil
--     self.energy = 0
--     self.max_energy = 10
-- end

local Food = class("Food")
function Food:initialize()
    self.sprite_id = 0
    self.position = Glm.vec2(0, 0)
end

local Obstacle = class("Obstacle")
function Obstacle:initialize()
    self.position = Glm.vec2(0, 0)
    self.radius = 100
    self.color = Math.Color4f(1, 1, 1, 1)
end

local RANDOM_DEVICE = rnd_pcg_t()
local LION_Z_LAYER = -1
local GIRAFFE_Z_LAYER = -2
local FOOD_Z_LAYER = -3

local game_state = {
    giraffes = {},
    obstacles = {},
    food = Food:new(),
    lion = ffi.new("Lion"),
    debug_draw = false,
    debug_avoidance = false,
}

local spawn_giraffes = function(engine, game, num_giraffes)
    for _ = 1, num_giraffes do
        local giraffe = ffi.new("Giraffe")
        giraffe.mob.mass = 100
        giraffe.mob.max_force = 1000
        giraffe.mob.max_speed = 300
        giraffe.mob.radius = 20
        giraffe.mob.position = Glm.vec2(
            50 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.x - 100),
            50 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.y - 100)
        )
        giraffe.dead = false

--        local sprite = Engine.add_sprite(game.sprites, "giraffe", Engine.Color.Pico8.orange)
--        giraffe.sprite_id = sprite:identifier()
        local game_ptr = ffi.cast("void**", ffi.cast("void*", game))
        local sprite_id = ffi.C.add_sprite(game_ptr, "giraffe")
        giraffe.sprite_id = sprite_id
        table.insert(game_state.giraffes, giraffe)
    end
end

function on_enter(engine, game)
    if profile then
        profile.start()
    end

    local time = os.time()
    rnd_pcg_seed(RANDOM_DEVICE, time)

    -- Spawn lake
    do
        local lake_obstacle = Obstacle:new()
        lake_obstacle.position = Glm.vec2(
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
        obstacle.position = Glm.vec2(
            10 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.x - 20),
            10 + rnd_pcg_nextf(RANDOM_DEVICE) * (engine.window_rect.size.y - 20)
        )
        obstacle.color = Engine.Color.Pico8.light_gray
        obstacle.radius = 20
        table.insert(game_state.obstacles, obstacle)
    end

    -- Spawn giraffes
    spawn_giraffes(engine, game, 1000)

    -- Spawn food
    do
        local sprite = Engine.add_sprite(game.sprites, "food", Engine.Color.Pico8.green)
        game_state.food.sprite_id = sprite:identifier()
        game_state.food.position = Glm.vec2(0.25 * engine.window_rect.size.x, 0.25 * engine.window_rect.size.y)

        local transform = Glm.mat4(1.0)
        transform = Glm.translate(transform, Glm.vec3(
            math.floor(game_state.food.position.x - sprite.atlas_frame.rect.size.x * sprite.atlas_frame.pivot.x),
            math.floor(game_state.food.position.y - sprite.atlas_frame.rect.size.y * (1.0 - sprite.atlas_frame.pivot.y)),
            FOOD_Z_LAYER
        ))
        transform = Glm.scale(transform, Glm.vec3(sprite.atlas_frame.rect.size.x, sprite.atlas_frame.rect.size.y, 1.0))

        local matrix = Math.matrix4f_from_transform(transform)
        Engine.transform_sprite(game.sprites, game_state.food.sprite_id, matrix)
    end

    -- Spawn lion
    do
        local game_ptr = ffi.cast("void**", ffi.cast("void*", game))
        local sprite_id = ffi.C.add_sprite(game_ptr, "lion")
        game_state.lion.sprite_id = sprite_id
        -- local sprite = Engine.add_sprite(game.sprites, "lion", Engine.Color.Pico8.yellow)
        -- game_state.lion.sprite_id = sprite:identifier()
        game_state.lion.mob.position = Glm.vec2(engine.window_rect.size.x * 0.75, engine.window_rect.size.y * 0.75)
        game_state.lion.mob.mass = 25
        game_state.lion.mob.max_force = 1000
        game_state.lion.mob.max_speed = 400
        game_state.lion.mob.radius = 20
    end
end

function on_leave(engine, game)
    if profile then
        profile.stop()
        print(profile.report(50))
    end
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
            game_state.food.position = Glm.vec2(x, y)

            local sprite = Engine.get_sprite(game.sprites, game_state.food.sprite_id)
            local transform = Glm.mat4(1.0)
            transform = Glm.translate(transform, Glm.vec3(
                math.floor(game_state.food.position.x - sprite.atlas_frame.rect.size.x * sprite.atlas_frame.pivot.x),
                math.floor(game_state.food.position.y - sprite.atlas_frame.rect.size.y * (1.0 - sprite.atlas_frame.pivot.y)),
                FOOD_Z_LAYER
            ))
            transform = Glm.scale(transform, Glm.vec3(sprite.atlas_frame.rect.size.x, sprite.atlas_frame.rect.size.y, 1.0))

            local matrix = Math.matrix4f_from_transform(transform)
            Engine.transform_sprite(game.sprites, game_state.food.sprite_id, matrix)
        end
    end
end

local update_mob = function(mob, game, dt)
    local drag = 1

    -- lake drags you down
    for _, obstacle in ipairs(game_state.obstacles) do
        local length = Glm.length(obstacle.position - mob.position)
        if length <= obstacle.radius then
            drag = 10
            break
        end
    end

    local drag_force = -drag * mob.velocity
    local steering_force = Glm.truncate(mob.steering_direction, mob.max_force) + drag_force
    local acceleration = steering_force / mob.mass

    mob.velocity = Glm.truncate(mob.velocity + acceleration, mob.max_speed)
    mob.position = mob.position + mob.velocity * dt

    if Glm.length(mob.velocity) > 0.001 then
        mob.orientation = math.atan2(-mob.velocity.x, mob.velocity.y)
    end
end

local arrival_behavior = function(mob, target_position, speed_ramp_distance)
    local target_offset = target_position - mob.position
    local distance = Glm.length(target_offset)
    local ramped_speed = mob.max_speed * (distance / speed_ramp_distance)
    local clipped_speed = math.min(ramped_speed, mob.max_speed)
    local desired_velocity = (clipped_speed / distance) * target_offset

    return desired_velocity - mob.velocity
end

local avoidance_behavior = function(mob, engine, game)
    local look_ahead_distance = 200

    local origin = mob.position
    local target_distance = Glm.length(mob.steering_target - origin)
    local forward = Glm.normalize(mob.steering_target - origin)

    local right_vector = Glm.vec2(forward.y, -forward.x)
    local left_vector = Glm.vec2(-forward.y, forward.x)

    local left_start = origin + left_vector * mob.radius
    local right_start = origin + right_vector * mob.radius

    local left_intersects = false
    local left_intersection = nil
    local left_intersection_distance = 1000000 -- sufficiently large enough

    local right_intersects = false
    local right_intersection = nil
    local right_intersection_distance = 1000000 -- sufficiently large enough

    -- check against each obstacle
    for _, obstacle in ipairs(game_state.obstacles) do
        local did_li, li = Glm.ray_circle_intersection(left_start, forward, obstacle.position, obstacle.radius);
        if did_li then
            local distance = Glm.length(li - left_start)
            if distance <= look_ahead_distance and distance < left_intersection_distance then
                left_intersects = true
                left_intersection = li
                left_intersection_distance = distance
            end
        end

        local did_ri, ri = Glm.ray_circle_intersection(right_start, forward, obstacle.position, obstacle.radius)
        if did_ri then
            local distance = Glm.length(ri - right_start)
            if distance <= look_ahead_distance and distance < right_intersection_distance then
                right_intersects = true
                right_intersection = li
                right_intersection_distance = distance
            end
        end
    end

    local avoidance_force = Glm.vec2(0, 0)

    if right_intersects or left_intersects then
        if target_distance <= left_intersection_distance and target_distance <= right_intersection_distance then
            return Glm.vec2(0, 0)
        end

        if left_intersection_distance < right_intersection_distance then
            local ratio = left_intersection_distance / look_ahead_distance
            avoidance_force = right_vector * (1.0 - ratio) * 50
        else
            local ratio = right_intersection_distance / look_ahead_distance
            avoidance_force = left_vector * (1.0 - ratio) * 50
        end
    end

    return avoidance_force
end

local update_giraffe = function(giraffe, engine, game, dt)
    local arrival_force = Glm.vec2(0, 0)
    local arrival_weight = 1

    local flee_force = Glm.vec2(0, 0)
    local flee_weight = 1

    local separation_force = Glm.vec2(0, 0)
    local separation_weight = 10

    local avoidance_force = Glm.vec2(0, 0)
    local avoidance_weight = 10

    if not giraffe.dead then
        local is_hunted = false

        local distance = Glm.length(game_state.lion.mob.position - giraffe.mob.position)
        if distance <= 300 then
            is_hunted = true
        end

        -- flee
        if is_hunted then
            local flee_direction = giraffe.mob.position - game_state.lion.mob.position
            local steering_target = giraffe.mob.position + flee_direction
            giraffe.mob.steering_target = steering_target

            local desired_velocity = Glm.normalize(flee_direction) * giraffe.mob.max_speed
            flee_force = desired_velocity - giraffe.mob.velocity

            local boundary_avoidance_force = Glm.vec2(0, 0)
            local buffer_distance = 100

            if giraffe.mob.position.x <= buffer_distance then
                boundary_avoidance_force.x = buffer_distance - giraffe.mob.position.x
            elseif giraffe.mob.position.x >= engine.window_rect.size.x - buffer_distance then
                boundary_avoidance_force.x = engine.window_rect.size.x - buffer_distance - giraffe.mob.position.x
            end

            if giraffe.mob.position.y <= buffer_distance then
                boundary_avoidance_force.y = buffer_distance - giraffe.mob.position.y
            elseif giraffe.mob.position.y >= engine.window_rect.size.y - buffer_distance then
                boundary_avoidance_force.y = engine.window_rect.size.y - buffer_distance - giraffe.mob.position.y
            end

            boundary_avoidance_force = boundary_avoidance_force * 20

            flee_force = flee_force + boundary_avoidance_force
            flee_force = flee_force * flee_weight
        else
            -- arrival
            giraffe.mob.steering_target = game_state.food.position
            arrival_force = arrival_behavior(giraffe.mob, game_state.food.position, 100)
            arrival_force = arrival_force * arrival_weight
        end

        -- separation
        do
            for _, other_giraffe in ipairs(game_state.giraffes) do
                if other_giraffe ~= giraffe and not other_giraffe.dead then
                    local offset = other_giraffe.mob.position - giraffe.mob.position
                    local distance_squared = Glm.length2(offset)
                    local near_distance_squared = (giraffe.mob.radius + other_giraffe.mob.radius) * (giraffe.mob.radius + other_giraffe.mob.radius)
                    if distance_squared <= near_distance_squared then
                        local distance = math.sqrt(distance_squared)
                        offset = offset / distance
                        offset = offset * (giraffe.mob.radius + other_giraffe.mob.radius)
                        separation_force = separation_force - offset
                    end
                end
            end

            separation_force = separation_force * separation_weight
        end

        -- avoidance
        do
            avoidance_force = avoidance_behavior(giraffe.mob, engine, game)
            avoidance_force = avoidance_force * avoidance_weight
        end
    end

    giraffe.mob.steering_direction = Glm.truncate(arrival_force + flee_force + separation_force + avoidance_force, giraffe.mob.max_force)

    update_mob(giraffe.mob, game, dt)

    local giraffe_frame = Engine.atlas_frame(game.sprites.atlas, "giraffe")

    local transform = Glm.mat4(1.0)
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

    transform = Glm.translate(transform, Glm.vec3(
        math.floor(giraffe.mob.position.x - x_offset),
        math.floor(giraffe.mob.position.y - y_offset),
        GIRAFFE_Z_LAYER
    ))
    transform = Glm.scale(transform, Glm.vec3((flip_x and -1.0 or 1.0) * giraffe_frame.rect.size.x, (flip_y and -1.0 or 1.0) * giraffe_frame.rect.size.y, 1.0))

    local matrix = Math.matrix4f_from_transform(transform)
    Engine.transform_sprite(game.sprites, giraffe.sprite_id, matrix)
end

local update_lion = function(lion, engine, game, t, dt)
    if not lion.locked_giraffe then
        if lion.energy >= lion.max_energy then
            local found_giraffe = nil
            local distance = 1000000
            for _, giraffe in ipairs(game_state.giraffes) do
                if not giraffe.dead then
                    local d = Glm.length(giraffe.mob.position - lion.mob.position)
                    if d < distance then
                        distance = d
                        found_giraffe = giraffe
                    end
                end
            end

            if found_giraffe then
                lion.locked_giraffe = found_giraffe
                lion.energy = lion.max_energy
            end
        else
            lion.energy = lion.energy + dt * 2
        end
    end

    if lion.locked_giraffe then
        lion.mob.steering_target = lion.locked_giraffe.mob.position

        local pursue_force = Glm.vec2(0, 0)
        local pursue_weight = 1

        local avoidance_force = Glm.vec2(0, 0)
        local avoidance_weight = 10

        -- Pursue
        do
            local desired_velocity = Glm.normalize(lion.locked_giraffe.mob.position - lion.mob.position) * lion.mob.max_speed
            pursue_force = desired_velocity - lion.mob.velocity
            pursue_force = pursue_force * pursue_weight
        end

        -- Avoidance
        do
            avoidance_force = avoidance_behavior(lion.mob, engine, game)
            avoidance_force = avoidance_force * avoidance_weight
        end

        lion.mob.steering_direction = Glm.truncate(pursue_force + avoidance_force, lion.mob.max_force)

        if Glm.length(lion.locked_giraffe.mob.position - lion.mob.position) <= lion.mob.radius then
            lion.locked_giraffe.dead = true
            Engine.color_sprite(game.sprites, lion.locked_giraffe.sprite_id, Engine.Color.Pico8.light_gray)

            lion.locked_giraffe = nil
            lion.energy = 0
            lion.mob.steering_direction = Glm.vec2(0, 0)
        else
            lion.energy = lion.energy - dt
            if lion.energy <= 0.0 then
                lion.locked_giraffe = nil
                lion.energy = 0.0
                lion.mob.steering_direction = Glm.vec2(0, 0)
            end
        end
    end

    update_mob(lion.mob, game, dt);

    local lion_frame = Engine.atlas_frame(game.sprites.atlas, "lion")

    local transform = Glm.mat4(1.0)
    local flip = lion.locked_giraffe and lion.locked_giraffe.mob.position.x < lion.mob.position.x

    local x_offset = lion_frame.rect.size.x * lion_frame.pivot.x

    if flip then
        x_offset = x_offset * -1.0
    end

    transform = Glm.translate(transform, Glm.vec3(
        math.floor(lion.mob.position.x - x_offset),
        math.floor(lion.mob.position.y - lion_frame.rect.size.y * (1.0 - lion_frame.pivot.y)),
        LION_Z_LAYER
    ))
    transform = Glm.scale(transform, Glm.vec3((flip and -1.0 or 1.0) * lion_frame.rect.size.x, lion_frame.rect.size.y, 1.0))

    local matrix = Math.matrix4f_from_transform(transform)
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

function render_imgui(engine, game)
    local draw_list = Imgui.GetForegroundDrawList()

    if game_state.debug_draw then
        local debug_draw_mob = function(mob)
            local origin = Glm.vec2(mob.position.x, engine.window_rect.size.y - mob.position.y)

            -- steer
            do
                local steer = Glm.truncate(mob.steering_direction, mob.max_force)
                steer.y = steer.y * -1
                local steer_ratio = Glm.length(steer) / mob.max_force
                local end_point = origin + Glm.normalize(steer) * (steer_ratio * 100)
                draw_list:AddLine(Imgui.Imvec2(origin.x, origin.y), Imgui.Imvec2(end_point.x, end_point.y), Imgui.IM_COL32(255, 0, 0, 255), 1.0)
                local ss = string.format("steer: %.0f", Glm.length(steer))
                draw_list:AddText(Imgui.Imvec2(origin.x, origin.y + 8), Imgui.IM_COL32(255, 0, 0, 255), ss)
            end

            -- velocity
            do
                local vel = Glm.truncate(mob.velocity, mob.max_speed)
                vel.y = vel.y * -1
                local vel_ratio = Glm.length(vel) / mob.max_speed
                local end_point = origin + Glm.normalize(vel) * (vel_ratio * 100)
                draw_list:AddLine(Imgui.Imvec2(origin.x, origin.y), Imgui.Imvec2(end_point.x, end_point.y), Imgui.IM_COL32(0, 255, 0, 255), 1.0)
                local ss = string.format("veloc: %.0f", Glm.length(vel))
                draw_list:AddText(Imgui.Imvec2(origin.x, origin.y + 20), Imgui.IM_COL32(0, 255, 0, 255), ss)
            end

            -- avoidance
            if game_state.debug_avoidance then
                local forward = Glm.normalize(mob.steering_target - mob.position)

                forward.y = forward.y * -1
                local look_ahead = origin + forward * 200

                local right_vector = Glm.vec2(-forward.y, forward.x)
                local left_vector = Glm.vec2(forward.y, -forward.x)

                local left_start = origin + left_vector * mob.radius
                local left_end = look_ahead + left_vector * mob.radius

                local right_start = origin + right_vector * mob.radius
                local right_end = look_ahead + right_vector * mob.radius

                draw_list:AddLine(Imgui.Imvec2(left_start.x, left_start.y), Imgui.Imvec2(left_end.x, left_end.y), Imgui.IM_COL32(255, 255, 0, 255), 1.0)
                draw_list:AddLine(Imgui.Imvec2(right_start.x, right_start.y), Imgui.Imvec2(right_end.x, right_end.y), Imgui.IM_COL32(255, 255, 0, 255), 1.0)
            end

            -- radius
            do
                draw_list:AddCircle(Imgui.Imvec2(origin.x, origin.y), mob.radius, Imgui.IM_COL32(255, 255, 0, 255), 0, 1.0)
            end
        end

        for _, giraffe in ipairs(game_state.giraffes) do
            if not giraffe.dead then
                debug_draw_mob(giraffe.mob)
            end
        end

        -- lion
        do
            debug_draw_mob(game_state.lion.mob)

            local origin = Glm.vec2(game_state.lion.mob.position.x, engine.window_rect.size.y - game_state.lion.mob.position.y)

            local ss = string.format("energy: %.0f", game_state.lion.energy)
            draw_list:AddText(Imgui.Imvec2(origin.x, origin.y + 32), Imgui.IM_COL32(255, 0, 0, 255), ss)
        end

        -- food
        draw_list:AddText(Imgui.Imvec2(game_state.food.position.x, engine.window_rect.size.y - game_state.food.position.y + 8), Imgui.IM_COL32(255, 255, 0, 255), "food")
    end

    -- obstacles
    for _, obstacle in ipairs(game_state.obstacles) do
        local obstacle_color = Imgui.IM_COL32(obstacle.color.r * 255, obstacle.color.g * 255, obstacle.color.b * 255, 255)
        local position = Imgui.Imvec2(obstacle.position.x, engine.window_rect.size.y - obstacle.position.y)
        draw_list:AddCircle(position, obstacle.radius, obstacle_color, 0, 2.0)
    end

    -- debug text
    do
        local ss = string.format("KEY_F1: Debug Draw %s\n", game_state.debug_draw and "on" or "off")
        ss = ss .. string.format("KEY_F2: Debug Avoidance %s\n", game_state.debug_avoidance and "on" or "off")
        ss = ss .. string.format("KEY_1: Spawn 1 giraffe\n")
        ss = ss .. string.format("KEY_5: Spawn 5 giraffes\n")
        ss = ss .. string.format("KEY_0: Spawn 10 giraffes\n")
        ss = ss .. string.format("MOUSE_LEFT: Move food\n")
        ss = ss .. string.format("Giraffes: %u\n", #game_state.giraffes)

        draw_list:AddText(Imgui.Imvec2(8, 8), Imgui.IM_COL32(255, 255, 255, 255), ss)
    end
end
