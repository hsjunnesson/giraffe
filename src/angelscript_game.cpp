#include "angelscript_game.h"

#if defined(HAS_ANGELSCRIPT)

#include "game.h"

#include "util.h"

#include "array.h"
#include "memory.h"
#include "rnd.h"
#include "string_stream.h"
#include "temp_allocator.h"

#include <engine/atlas.h>
#include <engine/color.inl>
#include <engine/engine.h>
#include <engine/file.h>
#include <engine/input.h>
#include <engine/log.h>
#include <engine/sprites.h>

#include <sstream>
#include <string>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/norm.hpp>
#include <glm/gtx/transform.hpp>

#include "add_on/scriptarray/scriptarray.h"
#include "add_on/scriptbuilder/scriptbuilder.h"
#include "add_on/scripthandle/scripthandle.h"
#include "add_on/scriptmath/scriptmath.h"
#include "add_on/scriptstdstring/scriptstdstring.h"
#include <angelscript.h>

#include <imgui.h>

namespace {
asIScriptEngine *script_engine = nullptr;
asIScriptFunction *on_enter_func = nullptr;
asIScriptFunction *on_leave_func = nullptr;
asIScriptFunction *on_input_func = nullptr;
asIScriptFunction *update_func = nullptr;
asIScriptFunction *render_func = nullptr;
asIScriptFunction *render_imgui_func = nullptr;
asIScriptContext *ctx = nullptr;
rnd_pcg_t random_device;
} // namespace

void message_callback(const asSMessageInfo *msg, void *param) {
    const char *type = "ERR ";

    if (msg->type == asMSGTYPE_WARNING) {
        type = "WARN";
    } else if (msg->type == asMSGTYPE_INFORMATION) {
        type = "INFO";
    }

    log_info("[ANGELSCRIPT] %s (%d, %d) : %s : %s", msg->section, msg->row, msg->col, type, msg->message);
}

void print(std::string &msg) {
    log_info("[ANGELSCRIPT] %s", msg.c_str());
}

const engine::AtlasFrame *atlas_frame_wrapper(const engine::Atlas *atlas, std::string &sprite_name) {
    return engine::atlas_frame(*atlas, sprite_name.c_str());
}

engine::Sprite add_sprite_wrapper(engine::Sprites *sprites, std::string &sprite_name, math::Color4f color) {
    return engine::add_sprite(*sprites, sprite_name.c_str(), color);
}

void update_sprites_wrapper(engine::Sprites *sprites, float t, float dt) {
    engine::update_sprites(*sprites, t, dt);
}

void transform_sprite_wrapper(engine::Sprites *sprites, uint64_t sprite_id, math::Matrix4f transform) {
    engine::transform_sprite(*sprites, sprite_id, transform);
}

void color_sprite_wrapper(engine::Sprites *sprites, uint64_t sprite_id, math::Color4f color) {
    engine::color_sprite(*sprites, sprite_id, color);
}

void commit_sprites_wrapper(engine::Sprites *sprites) {
    engine::commit_sprites(*sprites);
}

void render_sprites_wrapper(engine::Engine *engine, engine::Sprites *sprites) {
    engine::render_sprites(*engine, *sprites);
}

void vec2_default(void *memory) {
    new (memory) glm::vec2();
}

void vec2_construct(float x, float y, void *memory) {
    new (memory) glm::vec2(x, y);
}

std::string glm_vec2_tostring(const glm::vec2 &v) {
    std::ostringstream oss;
    oss << "vec2(" << v.x << ", " << v.y << ")";
    return oss.str();
}

glm::vec2 vec2_normalize(const glm::vec2 &v) {
    return glm::normalize(v);
}

float vec2_length(const glm::vec2 &v) {
    return glm::length(v);
}

float vec2_length2(const glm::vec2 &v) {
    return glm::length2(v);
}

float vec2_dot(const glm::vec2 &x, const glm::vec2 &y) {
    return glm::dot(x, y);
}

void vec3_default(void *memory) {
    new (memory) glm::vec3();
}

void vec3_construct(float x, float y, float z, void *memory) {
    new (memory) glm::vec3(x, y, z);
}

void mat4_default(void *memory) {
    new (memory) glm::mat4();
}

void mat4_construct(float x, void *memory) {
    new (memory) glm::mat4(x);
}

glm::mat4 mat4_translate(const glm::mat4 &mat, const glm::vec3 &vec) {
    return glm::translate(mat, vec);
}

glm::mat4 mat4_scale(const glm::mat4 &mat, const glm::vec3 &vec) {
    return glm::scale(mat, vec);
}

std::string glm_mat4_tostring(const glm::mat4 &mat) {
    std::ostringstream oss;
    oss << "mat4(";
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            oss << mat[col][row];
            if (!(col == 3 && row == 3)) // Prevent adding a comma after the last element
                oss << ", ";
        }
        if (col != 3)
            oss << "\n";
    }
    oss << ")";
    return oss.str();
}

void Color4f_DefaultConstruct(void *memory) {
    new (memory) math::Color4f(); // Call the default constructor
}

void Color4f_Construct(float r, float g, float b, float a, void *memory) {
    new (memory) math::Color4f{r, g, b, a};
}

void Color4f_Destruct(math::Color4f *obj) {
    obj->~Color4f();
}

void Matrix4f_from_mat4(const glm::mat4 mat4, math::Matrix4f *self) {
    new (self) math::Matrix4f(glm::value_ptr(mat4));
}

unsigned int im_col32_wrapper(unsigned int r, unsigned int g, unsigned int b, unsigned int a) {
    return IM_COL32(r, g, b, a);
}

namespace angelscript {

void initialize(foundation::Allocator &allocator) {
    using namespace foundation;
    using namespace foundation::string_stream;
    using namespace foundation::array;

    log_info("Initializing angelscript");

    script_engine = asCreateScriptEngine();
    int r = script_engine->SetMessageCallback(asFUNCTION(message_callback), 0, asCALL_CDECL);
    assert(r >= 0);

    RegisterScriptArray(script_engine, true);
    RegisterStdString(script_engine);
    RegisterScriptHandle(script_engine);
    RegisterScriptMath(script_engine);

    // Register functions

    r = script_engine->RegisterGlobalFunction("void print(const string &in)", asFUNCTION(print), asCALL_CDECL);
    assert(r >= 0);

    // MyModule

    CScriptBuilder builder;
    r = builder.StartNewModule(script_engine, "MyModule");
    assert(r >= 0);

    r = builder.AddSectionFromFile("scripts/script.as");
    assert(r >= 0);

    asIScriptModule *mod = script_engine->GetModule("MyModule");

    // Context

    ctx = script_engine->CreateContext();
    assert(ctx);

    // Register namespaces and types

    {
        // glm

        r = script_engine->SetDefaultNamespace("glm");
        assert(r >= 0);

        r = script_engine->RegisterObjectType("vec2", sizeof(glm::vec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS);
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("vec2", "float x", asOFFSET(glm::vec2, x));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("vec2", "float y", asOFFSET(glm::vec2, y));
        assert(r >= 0);

        r = script_engine->RegisterObjectBehaviour("vec2", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(vec2_default), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("vec2", asBEHAVE_CONSTRUCT, "void f(float, float)", asFUNCTION(vec2_construct), asCALL_CDECL_OBJLAST);
        assert(r >= 0);

        r = script_engine->RegisterObjectMethod("vec2", "vec2 opAdd(const vec2 &in) const", asFUNCTIONPR(glm::operator+, (const glm::vec2 &, const glm::vec2 &), glm::vec2), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 opSub(const vec2 &in) const", asFUNCTIONPR(glm::operator-, (const glm::vec2 &, const glm::vec2 &), glm::vec2), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 opMul(const vec2 &in) const", asFUNCTIONPR(glm::operator*, (const glm::vec2 &, const glm::vec2 &), glm::vec2), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 opMul(const float) const", asFUNCTIONPR(glm::operator*, (const glm::vec2 &, const float), glm::vec2), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 opMul_r(const float) const", asFUNCTIONPR(glm::operator*, (const float, const glm::vec2 &), glm::vec2), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 opDiv(const vec2 &in) const", asFUNCTIONPR(glm::operator/, (const glm::vec2 &, const glm::vec2 &), glm::vec2), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 opDiv(const float) const", asFUNCTIONPR(glm::operator/, (const glm::vec2 &, const float), glm::vec2), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);

        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opAddAssign(const vec2 &in)", asMETHODPR(glm::vec2, operator+=, (const glm::vec2 &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opSubAssign(const vec2 &in)", asMETHODPR(glm::vec2, operator-=, (const glm::vec2 &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opMulAssign(const vec2 &in)", asMETHODPR(glm::vec2, operator*=, (const glm::vec2 &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opDivAssign(const vec2 &in)", asMETHODPR(glm::vec2, operator/=, (const glm::vec2 &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);

        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opAddAssign(const float &in)", asMETHODPR(glm::vec2, operator+=, (const float &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opSubAssign(const float &in)", asMETHODPR(glm::vec2, operator-=, (const float &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opMulAssign(const float &in)", asMETHODPR(glm::vec2, operator*=, (const float &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opDivAssign(const float &in)", asMETHODPR(glm::vec2, operator/=, (const float &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);

        r = script_engine->RegisterObjectMethod("vec2", "vec2 &opAssign(const vec2 &in)", asMETHODPR(glm::vec2, operator=, (const glm::vec2 &), glm::vec2 &), asCALL_THISCALL);
        assert(r >= 0);

        r = script_engine->RegisterObjectMethod("vec2", "string tostring() const", asFUNCTION(glm_vec2_tostring), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("vec2 normalize(const vec2 &in)", asFUNCTION(vec2_normalize), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("float length(const vec2 &in)", asFUNCTION(vec2_length), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("float length2(const vec2 &in)", asFUNCTION(vec2_length2), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("float dot(const vec2 &in, const vec2 &in)", asFUNCTION(vec2_dot), asCALL_CDECL);
        assert(r >= 0);

        r = script_engine->RegisterObjectType("vec3", sizeof(glm::vec3), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK | asOBJ_APP_CLASS_ALLFLOATS);
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("vec3", "float x", asOFFSET(glm::vec3, x));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("vec3", "float y", asOFFSET(glm::vec3, y));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("vec3", "float z", asOFFSET(glm::vec3, z));
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("vec3", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(vec3_default), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("vec3", asBEHAVE_CONSTRUCT, "void f(float, float, float)", asFUNCTION(vec3_construct), asCALL_CDECL_OBJLAST);
        assert(r >= 0);

        r = script_engine->RegisterObjectType("mat4", sizeof(glm::mat4), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CAK);
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("mat4", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(mat4_default), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("mat4", asBEHAVE_CONSTRUCT, "void f(float)", asFUNCTION(mat4_construct), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("mat4 translate(const mat4 &in, const vec3 &in)", asFUNCTION(mat4_translate), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("mat4 scale(const mat4 &in, const vec3 &in)", asFUNCTION(mat4_scale), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("mat4", "string tostring() const", asFUNCTION(glm_mat4_tostring), asCALL_CDECL_OBJFIRST);
        assert(r >= 0);

        // util.h

        r = script_engine->RegisterGlobalFunction("vec2 truncate(const vec2 &in, float)", asFUNCTION(truncate), asCALL_CDECL);
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);
    }

    {
        // math.inl

        r = script_engine->SetDefaultNamespace("math");
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Color4f", sizeof(math::Color4f), asOBJ_VALUE | asOBJ_APP_CLASS_ALLFLOATS | asGetTypeTraits<math::Color4f>());
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("Color4f", asBEHAVE_CONSTRUCT, "void f()", asFUNCTION(Color4f_DefaultConstruct), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("Color4f", asBEHAVE_CONSTRUCT, "void f(float, float, float, float)", asFUNCTION(Color4f_Construct), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("Color4f", asBEHAVE_DESTRUCT, "void f()", asFUNCTION(Color4f_Destruct), asCALL_CDECL_OBJLAST);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("Color4f", "bool opEquals(const Color4f &in) const", asMETHOD(math::Color4f, operator==), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("Color4f", "bool opNotEquals(const Color4f &in) const", asMETHOD(math::Color4f, operator!=), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("Color4f", "Color4f &opAssign(const Color4f &in)", asMETHODPR(math::Color4f, operator=, (const math::Color4f &), math::Color4f &), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Color4f", "float r", asOFFSET(math::Color4f, r));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Color4f", "float g", asOFFSET(math::Color4f, g));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Color4f", "float b", asOFFSET(math::Color4f, b));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Color4f", "float a", asOFFSET(math::Color4f, a));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Vector2", sizeof(math::Vector2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLINTS | asOBJ_APP_CLASS_CDAK | asGetTypeTraits<math::Vector2>());
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Vector2", "int32 x", asOFFSET(math::Vector2, x));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Vector2", "int32 y", asOFFSET(math::Vector2, y));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Vector2f", sizeof(math::Vector2f), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asOBJ_APP_CLASS_CDAK | asGetTypeTraits<math::Vector2f>());
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Vector2f", "float x", asOFFSET(math::Vector2f, x));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Vector2f", "float y", asOFFSET(math::Vector2f, y));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Rect", sizeof(math::Rect), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_CDAK | asGetTypeTraits<math::Rect>());
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Rect", "math::Vector2 origin", asOFFSET(math::Rect, origin));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Rect", "math::Vector2 size", asOFFSET(math::Rect, size));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Matrix4f", sizeof(math::Matrix4f), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<math::Matrix4f>());
        assert(r >= 0);
        r = script_engine->RegisterObjectBehaviour("Matrix4f", asBEHAVE_CONSTRUCT, "void f(const glm::mat4 &in)", asFUNCTION(Matrix4f_from_mat4), asCALL_CDECL_OBJLAST);
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);
    }

    {
        // color.inl

        r = script_engine->SetDefaultNamespace("engine::color");
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f black", const_cast<math::Color4f *>(&engine::color::black));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f white", const_cast<math::Color4f *>(&engine::color::white));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f red", const_cast<math::Color4f *>(&engine::color::red));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f green", const_cast<math::Color4f *>(&engine::color::green));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f blue", const_cast<math::Color4f *>(&engine::color::blue));
        assert(r >= 0);
        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("engine::color::pico8");
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f black", const_cast<math::Color4f *>(&engine::color::pico8::black));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f dark_blue", const_cast<math::Color4f *>(&engine::color::pico8::dark_blue));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f dark_purple", const_cast<math::Color4f *>(&engine::color::pico8::dark_purple));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f dark_green", const_cast<math::Color4f *>(&engine::color::pico8::dark_green));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f brown", const_cast<math::Color4f *>(&engine::color::pico8::brown));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f dark_gray", const_cast<math::Color4f *>(&engine::color::pico8::dark_gray));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f light_gray", const_cast<math::Color4f *>(&engine::color::pico8::light_gray));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f red", const_cast<math::Color4f *>(&engine::color::pico8::red));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f orange", const_cast<math::Color4f *>(&engine::color::pico8::orange));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f yellow", const_cast<math::Color4f *>(&engine::color::pico8::yellow));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f green", const_cast<math::Color4f *>(&engine::color::pico8::green));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f blue", const_cast<math::Color4f *>(&engine::color::pico8::blue));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f indigo", const_cast<math::Color4f *>(&engine::color::pico8::indigo));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f pink", const_cast<math::Color4f *>(&engine::color::pico8::pink));
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("const math::Color4f peach", const_cast<math::Color4f *>(&engine::color::pico8::peach));
        assert(r >= 0);
        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);
    }

    {
        // engine.h

        r = script_engine->SetDefaultNamespace("engine");
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Engine", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Engine", "math::Rect window_rect", asOFFSET(engine::Engine, window_rect));
        assert(r >= 0);

        // atlas.h

        r = script_engine->RegisterObjectType("AtlasFrame", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("AtlasFrame", "math::Vector2f pivot", asOFFSET(engine::AtlasFrame, pivot));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("AtlasFrame", "math::Rect rect", asOFFSET(engine::AtlasFrame, rect));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Atlas", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);

        r = script_engine->RegisterGlobalFunction("engine::AtlasFrame@ atlas_frame(const engine::Atlas@, string sprite_name)", asFUNCTION(atlas_frame_wrapper), asCALL_CDECL);
        assert(r >= 0);

        // sprites.h

        r = script_engine->RegisterObjectType("Sprite", sizeof(engine::Sprite), asOBJ_VALUE | asOBJ_POD | asGetTypeTraits<engine::Sprite>());
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Sprite", "uint64 id", asOFFSET(engine::Sprite, id));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Sprite", "engine::AtlasFrame@ atlas_frame", asOFFSET(engine::Sprite, atlas_frame));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("Sprites", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("Sprites", "engine::Atlas@ atlas", asOFFSET(engine::Sprites, atlas));
        assert(r >= 0);

        r = script_engine->RegisterGlobalFunction("engine::Sprite add_sprite(engine::Sprites@ sprites, string sprite_name, math::Color4f color)", asFUNCTION(add_sprite_wrapper), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("void transform_sprite(engine::Sprites@ sprites, uint64 sprite_id, math::Matrix4f)", asFUNCTION(transform_sprite_wrapper), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("void color_sprite(engine::Sprites@ sprites, uint64 sprite_id, math::Color4f color)", asFUNCTION(color_sprite_wrapper), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("void update_sprites(engine::Sprites@ sprites, float t, float dt)", asFUNCTION(update_sprites_wrapper), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("void commit_sprites(engine::Sprites@ sprites)", asFUNCTION(commit_sprites_wrapper), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("void render_sprites(engine::Engine@ engine, engine::Sprites@ sprites)", asFUNCTION(render_sprites_wrapper), asCALL_CDECL);
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);
    }

    {
        r = script_engine->SetDefaultNamespace("game");
        assert(r >= 0);

        // game.h

        r = script_engine->RegisterObjectType("Game", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);

        r = script_engine->RegisterObjectProperty("Game", "engine::Sprites@ sprites", asOFFSET(game::Game, sprites));
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);
    }

    {
        // rnd.h

        r = script_engine->RegisterObjectType("rnd_pcg_t", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("void rnd_pcg_seed(rnd_pcg_t@, uint32)", asFUNCTION(rnd_pcg_seed), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("float rnd_pcg_nextf(rnd_pcg_t@)", asFUNCTION(rnd_pcg_nextf), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalProperty("rnd_pcg_t RANDOM_DEVICE", &random_device);
        assert(r >= 0);
    }

    {
        // imgui.h

        r = script_engine->RegisterObjectType("ImVec2", sizeof(ImVec2), asOBJ_VALUE | asOBJ_POD | asOBJ_APP_CLASS_ALLFLOATS | asOBJ_APP_CLASS_CDAK | asGetTypeTraits<ImVec2>());
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("ImVec2", "float x", asOFFSET(ImVec2, x));
        assert(r >= 0);
        r = script_engine->RegisterObjectProperty("ImVec2", "float y", asOFFSET(ImVec2, y));
        assert(r >= 0);

        r = script_engine->RegisterObjectType("ImDrawList", 0, asOBJ_REF | asOBJ_NOCOUNT);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("ImDrawList", "void AddLine(const ImVec2 &in, const ImVec2 &in, uint, float)", asMETHOD(ImDrawList, AddLine), asCALL_THISCALL);
        assert(r >= 0);
        r = script_engine->RegisterObjectMethod("ImDrawList", "void AddCircle(const ImVec2 &in, float, uint, int, float)", asMETHOD(ImDrawList, AddCircle), asCALL_THISCALL);
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("ImGui");
        assert(r >= 0);

        r = script_engine->RegisterGlobalFunction("ImDrawList@ GetForegroundDrawList()", asFUNCTION(ImGui::GetForegroundDrawList), asCALL_CDECL);
        assert(r >= 0);
        r = script_engine->RegisterGlobalFunction("uint IM_COL32(int r, int g, int b, int a)", asFUNCTION(im_col32_wrapper), asCALL_CDECL);
        assert(r >= 0);

        r = script_engine->SetDefaultNamespace("");
        assert(r >= 0);
    }

    // Build module

    r = builder.BuildModule();
    assert(r >= 0);

    // Get functions
    on_enter_func = mod->GetFunctionByDecl("void on_enter(engine::Engine@ engine, game::Game@ game)");
    on_leave_func = mod->GetFunctionByDecl("void on_leave(engine::Engine@ engine, game::Game@ game)");
    update_func = mod->GetFunctionByDecl("void update(engine::Engine@ engine, game::Game@ game, float t, float dt)");
    render_func = mod->GetFunctionByDecl("void render(engine::Engine@ engine, game::Game@ game)");
    render_imgui_func = mod->GetFunctionByDecl("void render_imgui(engine::Engine@ engine, game::Game@ game)");
    assert(on_enter_func);
    assert(on_leave_func);
    assert(update_func);
    assert(render_func);
    assert(render_imgui_func);
}

void close() {
    if (ctx) {
        ctx->Release();
        ctx = nullptr;
    }

    if (script_engine) {
        script_engine->ShutDownAndRelease();
        script_engine = nullptr;
        update_func = nullptr;
    }
}

} // namespace angelscript

namespace game {

void game_state_playing_enter(engine::Engine &engine, Game &game) {
    if (ctx && update_func) {
        ctx->Prepare(on_enter_func);
        ctx->SetArgAddress(0, &engine);
        ctx->SetArgAddress(1, &game);
        int r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            log_fatal("Could not execute on_enter() in scripts/script.as: %s", ctx->GetExceptionString());
        }
        ctx->Unprepare();
    }
}

void game_state_playing_leave(engine::Engine &engine, Game &game) {
    if (ctx && update_func) {
        ctx->Prepare(on_leave_func);
        ctx->SetArgAddress(0, &engine);
        ctx->SetArgAddress(1, &game);
        int r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            log_fatal("Could not execute on_leave() in scripts/script.as: %s", ctx->GetExceptionString());
        }
        ctx->Unprepare();
    }
}

void game_state_playing_on_input(engine::Engine &engine, Game &game, engine::InputCommand &input_command) {
}

void game_state_playing_update(engine::Engine &engine, Game &game, float t, float dt) {
    if (ctx && update_func) {
        ctx->Prepare(update_func);
        ctx->SetArgAddress(0, &engine);
        ctx->SetArgAddress(1, &game);
        ctx->SetArgFloat(2, t);
        ctx->SetArgFloat(3, dt);
        int r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            log_fatal("Could not execute update() in scripts/script.as: %s", ctx->GetExceptionString());
        }
        ctx->Unprepare();
    }
}

void game_state_playing_render(engine::Engine &engine, Game &game) {
    if (ctx && update_func) {
        ctx->Prepare(render_func);
        ctx->SetArgAddress(0, &engine);
        ctx->SetArgAddress(1, &game);
        int r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            log_fatal("Could not execute render() in scripts/script.as: %s", ctx->GetExceptionString());
        }
        ctx->Unprepare();
    }
}

void game_state_playing_render_imgui(engine::Engine &engine, Game &game) {
    if (ctx && update_func) {
        ctx->Prepare(render_imgui_func);
        ctx->SetArgAddress(0, &engine);
        ctx->SetArgAddress(1, &game);
        int r = ctx->Execute();
        if (r != asEXECUTION_FINISHED) {
            log_fatal("Could not execute render_imgui() in scripts/script.as: %s", ctx->GetExceptionString());
        }
        ctx->Unprepare();
    }
}

} // namespace game

#endif
