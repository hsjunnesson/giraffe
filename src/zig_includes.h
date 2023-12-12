#pragma once

#include <inttypes.h>
#include <stdbool.h>

#if defined(__cplusplus)
#define zig_extern extern "C"
#else
#define zig_extern extern
#endif

zig_extern struct Vector2f {
    float x;
    float y;
};

zig_extern struct Vector2 {
    int32_t x;
    int32_t y;
};

zig_extern struct Vector3f {
    float x;
    float y;
    float z;
};

zig_extern struct Rect {
    struct Vector2 origin;
    struct Vector2 size;
};

zig_extern struct Color4f {
    float r;
    float g;
    float b;
    float a;
};

zig_extern struct AtlasFrame {
    struct Vector2f pivot;
    struct Rect rect;
};

zig_extern struct Matrix4f {
    float m[16];
};

zig_extern struct Sprite {
    uint64_t id;
    const struct AtlasFrame *atlas_frame;
    struct Matrix4f transform;
    struct Color4f color;
    bool dirty;
};

zig_extern void fatal(const char *text);

zig_extern struct Rect glfw_window_rect(const void *engine);

zig_extern void *get_sprites(const void *game);
zig_extern void *get_atlas(const void *game);

zig_extern struct Sprite Engine_add_sprite(void *sprites, const char *sprite_name, const struct Color4f color);
zig_extern void Engine_update_sprites(void *sprites, float t, float dt);
zig_extern void Engine_commit_sprites(void *sprites);
zig_extern void Engine_render_sprites(const void *engine, const void *sprites);
zig_extern void Engine_transform_sprite(const void *sprites, uint64_t sprite_id, const struct Matrix4f transform);

zig_extern const struct AtlasFrame *Engine_atlas_frame(void *atlas, const char *sprite_name);

zig_extern struct Matrix4f Matrix4f_Identity();
zig_extern struct Matrix4f translate(const struct Matrix4f m, const struct Vector3f v);
zig_extern struct Matrix4f scale(const struct Matrix4f m, const struct Vector3f v);

zig_extern struct Vector2f add_vec2(const struct Vector2f v1, const struct Vector2f v2);
zig_extern struct Vector2f subtract_vec2(const struct Vector2f v1, const struct Vector2f v2);
zig_extern struct Vector2f multiply_vec2_factor(const struct Vector2f v, float f);
zig_extern struct Vector2f divide_vec2(const struct Vector2f v, float f);
zig_extern struct Vector2f truncate_vec2(const struct Vector2f v, float max_length);
zig_extern struct Vector2f normalize_vec2(const struct Vector2f v);
zig_extern float length_vec2(const struct Vector2f v);
zig_extern float length2_vec2(const struct Vector2f v);

zig_extern bool ray_circle_intersection_vec2(const struct Vector2f ray_origin, const struct Vector2f ray_direction, const struct Vector2f circle_center, float circle_radius, struct Vector2f *intersection);

zig_extern struct Color4f orange;
zig_extern struct Color4f blue;
zig_extern struct Color4f light_gray;
zig_extern struct Color4f green;
zig_extern struct Color4f yellow;

