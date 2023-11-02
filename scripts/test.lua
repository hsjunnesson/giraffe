local dbg = require("scripts/debugger")
--local profile = require("scripts/profile")

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
    ]]
end

if jit then
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
        translate = ffi.C.glm_translate,
        scale = ffi.C.glm_scale,
    }
end

function on_enter(engine, game)
    local transform = Glm.mat4(1.0)
    transform = Glm.translate(transform, Glm.vec3(10, 20, 30))
    transform = Glm.scale(transform, Glm.vec3(2, 2, 2))
    local matrix = Math.matrix4f_from_transform(transform)

    print(tostring(matrix))
end

function on_leave(engine, game)
end

function on_input(engine, game, input_command)
end

function update(engine, game, t, dt)
end

function render(engine, game)
end

function render_imgui(engine, game)
end
