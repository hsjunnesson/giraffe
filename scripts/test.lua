local dbg = require("scripts/debugger")
--local profile = require("scripts/profile")

function on_enter(engine, game)
    print(Engine.Color.Pico8.dark_blue.r)
    
    print(Engine.InputType)
    print(Engine.InputType.Key)
end

function on_leave(engine, game)
end

function on_input(engine, game, input_command)
end

function update(engine, game, t, dt)
    -- local origin = Glm.vec2(0, 0)
    -- local direction = Glm.vec2(1, 0)
    -- local circle_center = Glm.vec2(5.0, 2.0)
    -- local circle_radius = 3
    -- local hit, intersection = Glm.ray_circle_intersection(origin, direction, circle_center, circle_radius)
    -- print(hit, tostring(intersection))
end

function render(engine, game)
end

function render_imgui(engine, game)
end
