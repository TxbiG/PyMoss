// PyMoss: pybind11 bindings for the Moss Framework.
// This file contains the core PyMoss API and the expanded renderer/GPU/XR/physics bindings.
// Target: C++17, pybind11, Moss public headers.

#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include <Moss/Moss_stdinc.h>
#include <Moss/Moss_Platform.h>
#include <Moss/Moss_Audio.h>
#include <Moss/Moss_GPU.h>
#include <Moss/Moss_Renderer.h>
#include <Moss/Moss_Physics.h>
#include <Moss/Moss_XR.h>

#include <Moss/Variants/Color.h>
#include <Moss/Variants/Quat.h>
#include <Moss/Variants/Rect.h>
#include <Moss/Variants/Vector/Vec2.h>
#include <Moss/Variants/Vector/Vec3.h>
#include <Moss/Variants/Vector/Vec4.h>

namespace py = pybind11;

namespace {

class Window {
public:
    Window(const std::string& title, int width, int height) {
        if (width <= 0 || height <= 0)
            throw py::value_error("width and height must be positive");
        window_ = Moss_CreateWindow(title.c_str(), width, height, nullptr, nullptr);
        if (!window_)
            throw std::runtime_error("Moss_CreateWindow failed");
    }

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    Window(Window&& other) noexcept : window_(std::exchange(other.window_, nullptr)) {}
    Window& operator=(Window&& other) noexcept {
        if (this != &other) {
            close();
            window_ = std::exchange(other.window_, nullptr);
        }
        return *this;
    }
    ~Window() { close(); }

    bool should_close() const { return !window_ || Moss_ShouldWindowClose(window_); }
    bool is_open() const { return window_ != nullptr; }
    void close() {
        if (window_) {
            Moss_TerminateWindow(window_);
            window_ = nullptr;
        }
    }
    void set_title(const std::string& title) {
        require_open();
        Moss_SetWindowTitle(window_, title.c_str());
    }
    std::uintptr_t native_handle() const {
        require_open();
        return reinterpret_cast<std::uintptr_t>(window_);
    }

private:
    void require_open() const {
        if (!window_) throw std::runtime_error("the Moss window is closed");
    }
    Moss_Window* window_ = nullptr;
};

class Monitor {
public:
    explicit Monitor(Moss_Monitor* monitor = nullptr) : monitor_(monitor) {}
    bool valid() const { return monitor_ != nullptr; }
    std::uintptr_t native_handle() const {
        return reinterpret_cast<std::uintptr_t>(monitor_);
    }
    std::string name() const {
        require_valid();
        const char* value = Moss_MonitorGetName(monitor_);
        return value ? value : "";
    }
    py::tuple position() const {
        require_valid();
        int x = 0, y = 0;
        Moss_MonitorGetPosition(monitor_, &x, &y);
        return py::make_tuple(x, y);
    }
    py::tuple physical_size() const {
        require_valid();
        int width = 0, height = 0;
        Moss_MonitorGetPhysicalSize(monitor_, &width, &height);
        return py::make_tuple(width, height);
    }
    py::tuple content_scale() const {
        require_valid();
        float x = 1.0f, y = 1.0f;
        Moss_MonitorGetContentScale(monitor_, &x, &y);
        return py::make_tuple(x, y);
    }
private:
    void require_valid() const {
        if (!monitor_) throw std::runtime_error("invalid Moss monitor");
    }
    Moss_Monitor* monitor_ = nullptr; // borrowed from Moss; do not destroy it here
};

class Gamepad {
public:
    explicit Gamepad(Moss_Gamepad* gamepad) : gamepad_(gamepad) {}
    Gamepad(const Gamepad&) = delete;
    Gamepad& operator=(const Gamepad&) = delete;
    Gamepad(Gamepad&& other) noexcept : gamepad_(std::exchange(other.gamepad_, nullptr)) {}
    Gamepad& operator=(Gamepad&& other) noexcept {
        if (this != &other) {
            close();
            gamepad_ = std::exchange(other.gamepad_, nullptr);
        }
        return *this;
    }
    ~Gamepad() { close(); }
    void close() {
        if (gamepad_) {
            Moss_CloseGamepad(gamepad_);
            gamepad_ = nullptr;
        }
    }
    bool connected() const { require_open(); return Moss_GamepadConnected(gamepad_); }
    bool button_pressed(Moss_GamepadButton button) const {
        require_open(); return Moss_IsGamepadButtonPressed(gamepad_, button);
    }
    bool button_just_pressed(Moss_GamepadButton button) const {
        require_open(); return Moss_IsGamepadButtonJustPressed(gamepad_, button);
    }
    bool button_just_released(Moss_GamepadButton button) const {
        require_open(); return Moss_IsGamepadButtonJustReleased(gamepad_, button);
    }
    float axis(GamepadAxis axis) const { require_open(); return Moss_GetGamepadAxis(gamepad_, axis); }
    std::string name() const {
        require_open();
        const char* value = Moss_GetGamepadName(gamepad_);
        return value ? value : "";
    }
private:
    void require_open() const {
        if (!gamepad_) throw std::runtime_error("the Moss gamepad is closed");
    }
    Moss_Gamepad* gamepad_ = nullptr;
};

} // namespace

namespace pymoss_extra_detail {
namespace py = pybind11;

template <typename T>
inline T* from_handle(std::uintptr_t h) {
    return reinterpret_cast<T*>(h);
}

template <typename T>
inline std::uintptr_t to_handle(T* p) {
    return reinterpret_cast<std::uintptr_t>(p);
}

inline void require_size(const py::sequence& s, py::ssize_t expected, const char* label) {
    if (py::len(s) != expected) {
        throw py::value_error(std::string(label) + " must contain exactly " + std::to_string(expected) + " values");
    }
}

inline Float2 as_float2(const py::sequence& s, const char* label = "vector2") {
    require_size(s, 2, label);
    return Float2(s[0].cast<float>(), s[1].cast<float>());
}

inline Float3 as_float3(const py::sequence& s, const char* label = "vector3") {
    require_size(s, 3, label);
    return Float3(s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>());
}

inline Vec2 as_vec2(const py::sequence& s, const char* label = "vector2") {
    require_size(s, 2, label);
    return Vec2(s[0].cast<float>(), s[1].cast<float>());
}

inline Vec3 as_vec3(const py::sequence& s, const char* label = "vector3") {
    require_size(s, 3, label);
    return Vec3(s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>());
}

inline Color as_color(const py::sequence& s, const char* label = "color") {
    if (py::len(s) != 3 && py::len(s) != 4) {
        throw py::value_error(std::string(label) + " must contain RGB or RGBA values");
    }
    const float a = py::len(s) == 4 ? s[3].cast<float>() : 1.0f;
    return Color(s[0].cast<float>(), s[1].cast<float>(), s[2].cast<float>(), a);
}

inline std::array<float, 16> as_matrix16(const py::sequence& s, const char* label = "matrix") {
    require_size(s, 16, label);
    std::array<float, 16> out{};
    for (py::ssize_t i = 0; i < 16; ++i) out[static_cast<size_t>(i)] = s[i].cast<float>();
    return out;
}

inline py::tuple vec2_tuple(const Vec2& v) {
    return py::make_tuple(v.GetX(), v.GetY());
}
inline py::tuple vec3_tuple(const Vec3& v) {
    return py::make_tuple(v.GetX(), v.GetY(), v.GetZ());
}
inline py::tuple quat_tuple(const Quat& q) {
    return py::make_tuple(q.GetX(), q.GetY(), q.GetZ(), q.GetW());
}

inline py::dict xr_pose_dict(const MossXR_Pose& pose) {
    py::dict out;
    out["position"] = vec3_tuple(pose.position);
    out["orientation_xyzw"] = quat_tuple(pose.orientation);
    return out;
}

} // namespace pymoss_extra_detail

inline void bind_pymoss_extra_bindings(pybind11::module_& m) {
    namespace py = pybind11;
    using namespace pymoss_extra_detail;

    // ---------------------------------------------------------------------
    // Moss math value types. Keep the SIMD-backed Moss vectors as real C++
    // values; don't map them to opaque integer handles.
    // ---------------------------------------------------------------------
    py::class_<Vec2>(m, "Vec2")
        .def(py::init<float, float>(), py::arg("x"), py::arg("y"))
        .def_property("x", [](const Vec2& v) { return v.GetX(); }, [](Vec2& v, float x) { v.SetX(x); })
        .def_property("y", [](const Vec2& v) { return v.GetY(); }, [](Vec2& v, float y) { v.SetY(y); })
        .def("length", &Vec2::Length)
        .def("length_squared", &Vec2::LengthSq)
        .def("normalized", &Vec2::Normalized)
        .def("dot", &Vec2::Dot)
        .def("cross", [](const Vec2& a, const Vec2& b) { return a.Cross(b); })
        .def("to_tuple", &vec2_tuple);

    py::class_<Vec3>(m, "Vec3")
        .def(py::init<float, float, float>(), py::arg("x"), py::arg("y"), py::arg("z"))
        .def_property("x", [](const Vec3& v) { return v.GetX(); }, [](Vec3& v, float x) { v.SetX(x); })
        .def_property("y", [](const Vec3& v) { return v.GetY(); }, [](Vec3& v, float y) { v.SetY(y); })
        .def_property("z", [](const Vec3& v) { return v.GetZ(); }, [](Vec3& v, float z) { v.SetZ(z); })
        .def("length", &Vec3::Length)
        .def("length_squared", &Vec3::LengthSq)
        .def("normalized", &Vec3::Normalized)
        .def("dot", &Vec3::Dot)
        .def("cross", [](const Vec3& a, const Vec3& b) { return a.Cross(b); })
        .def("to_tuple", &vec3_tuple);

    py::class_<Vec4>(m, "Vec4")
        .def(py::init<float, float, float, float>(), py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"))
        .def_property_readonly("x", [](const Vec4& v) { return v.GetX(); })
        .def_property_readonly("y", [](const Vec4& v) { return v.GetY(); })
        .def_property_readonly("z", [](const Vec4& v) { return v.GetZ(); })
        .def_property_readonly("w", [](const Vec4& v) { return v.GetW(); })
        .def("length", &Vec4::Length)
        .def("length_squared", &Vec4::LengthSq);

    py::class_<Color>(m, "Color")
        .def(py::init<>())
        .def(py::init<float, float, float>(), py::arg("r"), py::arg("g"), py::arg("b"))
        .def(py::init<float, float, float, float>(), py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a"))
        .def_readwrite("r", &Color::r)
        .def_readwrite("g", &Color::g)
        .def_readwrite("b", &Color::b)
        .def_readwrite("a", &Color::a)
        .def("intensity", &Color::GetIntensity)
        .def("to_vec4", &Color::ToVec4);

    // Convenience constructors returning tuples / Moss vectors.
    m.def("vec2", [](float x, float y) { return Vec2(x, y); }, py::arg("x"), py::arg("y"));
    m.def("vec3", [](float x, float y, float z) { return Vec3(x, y, z); }, py::arg("x"), py::arg("y"), py::arg("z"));
    m.def("vec4", [](float x, float y, float z, float w) { return Vec4(x, y, z, w); }, py::arg("x"), py::arg("y"), py::arg("z"), py::arg("w"));

    // ---------------------------------------------------------------------
    // Renderer lifecycle, model/mesh loading, cameras and draw calls.
    // Public declarations verified in Moss_Renderer.h.
    // ---------------------------------------------------------------------
    m.def("renderer_create", [](std::uintptr_t window) {
        return to_handle(Moss_CreateRenderer(from_handle<Moss_Window>(window)));
    }, py::arg("window"));
    m.def("renderer_destroy", [](std::uintptr_t renderer) {
        if (renderer) Moss_RendererDestroy(from_handle<Moss_Renderer>(renderer));
    });
    m.def("renderer_terminate", [](std::uintptr_t renderer) {
        if (renderer) Moss_TerminateRenderer(from_handle<Moss_Renderer>(renderer));
    });
    m.def("renderer_begin_frame", [](std::uintptr_t renderer) {
        Moss_RendererBeginFrame(from_handle<Moss_Renderer>(renderer));
    });
    m.def("renderer_begin_frame_ex", [](std::uintptr_t renderer, float delta_time,
                                         const py::sequence& clear, float world_scale,
                                         std::uintptr_t camera) {
        Moss_RenderFrameDesc desc{};
        desc.delta_time = delta_time;
        desc.clear_color = as_color(clear);
        desc.world_scale = world_scale;
        desc.camera = reinterpret_cast<const void*>(camera);
        Moss_RendererBeginFrameEx(from_handle<Moss_Renderer>(renderer), &desc);
    }, py::arg("renderer"), py::arg("delta_time"), py::arg("clear_color") = py::make_tuple(0.0f, 0.0f, 0.0f, 1.0f),
       py::arg("world_scale") = 1.0f, py::arg("camera") = 0);
    m.def("renderer_end_frame", [](std::uintptr_t renderer) {
        Moss_RendererEndFrame(from_handle<Moss_Renderer>(renderer));
    });
    m.def("renderer_gpu_device", [](std::uintptr_t renderer) {
        return to_handle(Moss_RendererGetGPUDevice(from_handle<Moss_Renderer>(renderer)));
    });
    m.def("renderer_backbuffer", [](std::uintptr_t renderer) {
        return to_handle(Moss_RendererGetBackbuffer(from_handle<Moss_Renderer>(renderer)));
    });
    m.def("renderer_set_camera", [](std::uintptr_t renderer, std::uintptr_t camera) {
        Moss_RendererSetCamera(from_handle<Moss_Renderer>(renderer), from_handle<Moss_Camera>(camera));
    });

    m.def("mesh_load", [](const std::string& path) { return to_handle(Moss_MeshLoadOBJ(path.c_str())); });
    m.def("mesh_load_obj", [](const std::string& path) { return to_handle(Moss_MeshLoadOBJ(path.c_str())); });
    m.def("mesh_load_fbx", [](const std::string& path) { return to_handle(Moss_MeshLoadFBX(path.c_str())); });
    m.def("mesh_load_glb", [](const std::string& path) { return to_handle(Moss_MeshLoadGLB(path.c_str())); });
    m.def("mesh_remove", [](std::uintptr_t mesh) { if (mesh) Moss_MeshRemove(from_handle<Moss_Mesh>(mesh)); });
    m.def("mesh_draw_legacy", [](std::uintptr_t mesh) { Moss_MeshDraw(from_handle<Moss_Mesh>(mesh)); });

    m.def("model_load", [](const std::string& path) { return to_handle(Moss_ModelLoad(path.c_str())); });
    m.def("model_load_obj", [](const std::string& path) { return to_handle(Moss_ModelLoadOBJ(path.c_str())); });
    m.def("model_load_fbx", [](const std::string& path) { return to_handle(Moss_ModelLoadFBX(path.c_str())); });
    m.def("model_load_glb", [](const std::string& path) { return to_handle(Moss_ModelLoadGLB(path.c_str())); });
    m.def("model_remove", [](std::uintptr_t model) { if (model) Moss_ModelRemove(from_handle<Moss_Model>(model)); });
    m.def("model_draw_legacy", [](std::uintptr_t model) { Moss_ModelDraw(from_handle<Moss_Model>(model)); });
    m.def("model_last_load_error", [] { return static_cast<int>(Moss_ModelGetLastLoadError()); });
    m.def("model_last_load_error_message", [] {
        const char* text = Moss_ModelGetLastLoadErrorMessage();
        return text ? text : "";
    });
    m.def("model_find_bone", [](std::uintptr_t model, const std::string& name) {
        return Moss_ModelFindBone(from_handle<Moss_Model>(model), name.c_str());
    });
    m.def("model_find_morph_target", [](std::uintptr_t model, const std::string& name) {
        return Moss_ModelFindMorphTarget(from_handle<Moss_Model>(model), name.c_str());
    });
    m.def("model_set_bone_transform", [](std::uintptr_t model, uint32_t bone, const py::sequence& matrix) {
        const auto values = as_matrix16(matrix, "transform");
        return Moss_ModelSetBoneTransform(from_handle<Moss_Model>(model), bone, values.data());
    });
    m.def("model_set_morph_weight", [](std::uintptr_t model, uint32_t morph, float weight) {
        return Moss_ModelSetMorphWeight(from_handle<Moss_Model>(model), morph, weight);
    });
    m.def("renderer_draw_model", [](std::uintptr_t renderer, std::uintptr_t model, const py::sequence& matrix) {
        const auto values = as_matrix16(matrix, "transform");
        Moss_RendererDrawModel(from_handle<Moss_Renderer>(renderer), from_handle<Moss_Model>(model), values.data());
    });
    m.def("renderer_draw_mesh", [](std::uintptr_t renderer, std::uintptr_t mesh, const py::sequence& matrix,
                                    uint32_t visibility) {
        Moss_DrawMeshDesc desc{};
        desc.mesh = from_handle<Moss_Mesh>(mesh);
        const auto values = as_matrix16(matrix, "transform");
        std::memcpy(desc.transform, values.data(), sizeof(desc.transform));
        desc.visibility = visibility;
        Moss_RendererDrawMesh(from_handle<Moss_Renderer>(renderer), &desc);
    }, py::arg("renderer"), py::arg("mesh"), py::arg("transform"), py::arg("visibility") = 0xFFFFFFFFu);
    m.def("renderer_draw_model_desc", [](std::uintptr_t renderer, std::uintptr_t model,
                                          const py::sequence& matrix, uint32_t visibility) {
        Moss_DrawModelDesc desc{};
        desc.model = from_handle<Moss_Model>(model);
        const auto values = as_matrix16(matrix, "transform");
        std::memcpy(desc.transform, values.data(), sizeof(desc.transform));
        desc.visibility = visibility;
        Moss_RendererDrawModelDesc(from_handle<Moss_Renderer>(renderer), &desc);
    }, py::arg("renderer"), py::arg("model"), py::arg("transform"), py::arg("visibility") = 0xFFFFFFFFu);

    m.def("camera2d_create", [](const py::sequence& position, const py::sequence& offset,
                                 float zoom, float rotation, float viewport_width, float viewport_height) {
        Moss_Camera2DDesc desc{};
        desc.position = as_float2(position, "position");
        desc.offset = as_float2(offset, "offset");
        desc.zoom = zoom;
        desc.rotation = rotation;
        desc.viewport_width = viewport_width;
        desc.viewport_height = viewport_height;
        return to_handle(Moss_CameraCreate2D(&desc));
    }, py::arg("position") = py::make_tuple(0.0f, 0.0f), py::arg("offset") = py::make_tuple(0.0f, 0.0f),
       py::arg("zoom") = 1.0f, py::arg("rotation") = 0.0f,
       py::arg("viewport_width") = 1.0f, py::arg("viewport_height") = 1.0f);
    m.def("camera3d_create", [](const py::sequence& position, const py::sequence& target,
                                 const py::sequence& up, float fov, float aspect,
                                 float near_plane, float far_plane) {
        Moss_Camera3DDesc desc{};
        desc.position = as_float3(position, "position");
        desc.target = as_float3(target, "target");
        desc.up = as_float3(up, "up");
        desc.fov_degrees = fov;
        desc.aspect_ratio = aspect;
        desc.near_plane = near_plane;
        desc.far_plane = far_plane;
        return to_handle(Moss_CameraCreate3D(&desc));
    }, py::arg("position") = py::make_tuple(0.0f, 0.0f, 0.0f),
       py::arg("target") = py::make_tuple(0.0f, 0.0f, -1.0f),
       py::arg("up") = py::make_tuple(0.0f, 1.0f, 0.0f), py::arg("fov_degrees") = 45.0f,
       py::arg("aspect_ratio") = 1.0f, py::arg("near_plane") = 0.1f, py::arg("far_plane") = 1000.0f);
    m.def("camera_editor_create", [](const py::dict& d) {
        Moss_EditorCameraDesc desc{};
        if (d.contains("position")) desc.camera.position = as_float3(d["position"].cast<py::sequence>(), "position");
        if (d.contains("target")) desc.camera.target = as_float3(d["target"].cast<py::sequence>(), "target");
        if (d.contains("up")) desc.camera.up = as_float3(d["up"].cast<py::sequence>(), "up");
        if (d.contains("fov_degrees")) desc.camera.fov_degrees = d["fov_degrees"].cast<float>();
        if (d.contains("aspect_ratio")) desc.camera.aspect_ratio = d["aspect_ratio"].cast<float>();
        if (d.contains("near_plane")) desc.camera.near_plane = d["near_plane"].cast<float>();
        if (d.contains("far_plane")) desc.camera.far_plane = d["far_plane"].cast<float>();
        if (d.contains("move_speed")) desc.move_speed = d["move_speed"].cast<float>();
        if (d.contains("look_sensitivity")) desc.look_sensitivity = d["look_sensitivity"].cast<float>();
        if (d.contains("orbit_mode")) desc.orbit_mode = d["orbit_mode"].cast<bool>();
        return to_handle(Moss_CameraCreateEditor(&desc));
    }, py::arg("description") = py::dict());
    m.def("camera_destroy", [](std::uintptr_t camera) { if (camera) Moss_CameraDestroy(from_handle<Moss_Camera>(camera)); });

    m.def("shadow_map_create", [](std::uintptr_t renderer, uint32_t width, uint32_t height,
                                   bool cascades, uint32_t cascade_count) {
        Moss_ShadowMapDesc desc{};
        desc.width = width;
        desc.height = height;
        desc.enable_cascades = cascades;
        desc.cascade_count = cascade_count;
        return to_handle(Moss_RendererCreateShadowMap(from_handle<Moss_Renderer>(renderer), &desc));
    }, py::arg("renderer"), py::arg("width") = 2048, py::arg("height") = 2048,
       py::arg("enable_cascades") = false, py::arg("cascade_count") = 1);
    m.def("shadow_map_destroy", [](std::uintptr_t renderer, std::uintptr_t shadow) {
        Moss_RendererDestroyShadowMap(from_handle<Moss_Renderer>(renderer), from_handle<Moss_ShadowMap>(shadow));
    });
    m.def("shadow_pass_begin", [](std::uintptr_t renderer, std::uintptr_t shadow) {
        Moss_RendererBeginShadowPass(from_handle<Moss_Renderer>(renderer), from_handle<Moss_ShadowMap>(shadow));
    });
    m.def("shadow_pass_end", [](std::uintptr_t renderer, std::uintptr_t shadow) {
        Moss_RendererEndShadowPass(from_handle<Moss_Renderer>(renderer), from_handle<Moss_ShadowMap>(shadow));
    });

    // Font / text, sprites, sprite batches and debug geometry.
    m.def("font_load", [](const std::string& path, float pixel_size) { return to_handle(Moss_FontLoad(path.c_str(), pixel_size)); },
          py::arg("path"), py::arg("pixel_size") = 16.0f);
    m.def("font_destroy", [](std::uintptr_t font) { if (font) Moss_FontDestroy(from_handle<Moss_Font>(font)); });
    m.def("font_measure_text", [](std::uintptr_t font, const std::string& text, float scale) {
        const Moss_TextMeasure result = Moss_FontMeasureText(from_handle<Moss_Font>(font), text.c_str(), scale);
        return py::make_tuple(result.width, result.height);
    }, py::arg("font"), py::arg("text"), py::arg("scale") = 1.0f);
    m.def("renderer_draw_text", [](std::uintptr_t renderer, std::uintptr_t font, const std::string& text,
                                    const py::sequence& position, const py::sequence& color,
                                    float scale, float depth, uint32_t visibility) {
        Moss_DrawTextDesc desc{};
        desc.font = from_handle<Moss_Font>(font);
        desc.text = text.c_str();
        desc.position = as_float2(position, "position");
        desc.color = as_color(color);
        desc.scale = scale;
        desc.depth = depth;
        desc.visibility = visibility;
        Moss_RendererDrawText(from_handle<Moss_Renderer>(renderer), &desc);
    }, py::arg("renderer"), py::arg("font"), py::arg("text"),
       py::arg("position") = py::make_tuple(0.0f, 0.0f),
       py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f, 1.0f),
       py::arg("scale") = 1.0f, py::arg("depth") = 0.0f, py::arg("visibility") = 0xFFFFFFFFu);
    m.def("renderer_draw_sprite", [](std::uintptr_t renderer, std::uintptr_t texture,
                                      const py::sequence& position, const py::sequence& size,
                                      const py::sequence& color, float rotation, float depth,
                                      uint32_t visibility) {
        Moss_SpriteDesc desc{};
        desc.texture = from_handle<Texture>(texture);
        desc.position = as_float2(position, "position");
        desc.size = as_float2(size, "size");
        desc.color = as_color(color);
        desc.rotation = rotation;
        desc.depth = depth;
        desc.visibility = visibility;
        Moss_RendererDrawSprite(from_handle<Moss_Renderer>(renderer), &desc);
    }, py::arg("renderer"), py::arg("texture"), py::arg("position"), py::arg("size"),
       py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f, 1.0f), py::arg("rotation") = 0.0f,
       py::arg("depth") = 0.0f, py::arg("visibility") = 0xFFFFFFFFu);
    m.def("sprite_batch_create", [](std::uintptr_t renderer, uint32_t capacity) {
        return to_handle(Moss_RendererCreateSpriteBatch(from_handle<Moss_Renderer>(renderer), capacity));
    }, py::arg("renderer"), py::arg("capacity") = 1024);
    m.def("sprite_batch_clear", [](std::uintptr_t batch) { Moss_SpriteBatchClear(from_handle<Moss_SpriteBatch>(batch)); });
    m.def("sprite_batch_add", [](std::uintptr_t batch, std::uintptr_t texture,
                                  const py::sequence& position, const py::sequence& size,
                                  const py::sequence& color, float rotation, float depth) {
        Moss_SpriteDesc desc{};
        desc.texture = from_handle<Texture>(texture);
        desc.position = as_float2(position, "position");
        desc.size = as_float2(size, "size");
        desc.color = as_color(color);
        desc.rotation = rotation;
        desc.depth = depth;
        return Moss_SpriteBatchAdd(from_handle<Moss_SpriteBatch>(batch), &desc);
    }, py::arg("batch"), py::arg("texture"), py::arg("position"), py::arg("size"),
       py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f, 1.0f), py::arg("rotation") = 0.0f, py::arg("depth") = 0.0f);
    m.def("sprite_batch_draw", [](std::uintptr_t renderer, std::uintptr_t batch) {
        Moss_RendererDrawSpriteBatch(from_handle<Moss_Renderer>(renderer), from_handle<Moss_SpriteBatch>(batch));
    });
    m.def("sprite_batch_destroy", [](std::uintptr_t renderer, std::uintptr_t batch) {
        Moss_RendererDestroySpriteBatch(from_handle<Moss_Renderer>(renderer), from_handle<Moss_SpriteBatch>(batch));
    });

    m.def("debug_draw_list_create", [](std::uintptr_t renderer, uint32_t capacity) {
        return to_handle(Moss_RendererCreateDebugDrawList(from_handle<Moss_Renderer>(renderer), capacity));
    }, py::arg("renderer"), py::arg("capacity") = 1024);
    m.def("debug_draw_list_clear", [](std::uintptr_t list) { Moss_DebugDrawListClear(from_handle<Moss_DebugDrawList>(list)); });
    m.def("debug_draw_line", [](std::uintptr_t list, const py::sequence& from, const py::sequence& to,
                                 const py::sequence& color, float thickness) {
        Moss_DebugLineDesc desc{};
        desc.from = as_float3(from, "from");
        desc.to = as_float3(to, "to");
        desc.color = as_color(color);
        desc.thickness = thickness;
        return Moss_DebugDrawLine(from_handle<Moss_DebugDrawList>(list), &desc);
    }, py::arg("list"), py::arg("from"), py::arg("to"), py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f, 1.0f), py::arg("thickness") = 1.0f);
    m.def("debug_draw_box", [](std::uintptr_t list, const py::sequence& center, const py::sequence& size,
                                const py::sequence& color) {
        Moss_DebugBoxDesc desc{};
        desc.center = as_float3(center, "center");
        desc.size = as_float3(size, "size");
        desc.color = as_color(color);
        return Moss_DebugDrawBox(from_handle<Moss_DebugDrawList>(list), &desc);
    }, py::arg("list"), py::arg("center"), py::arg("size"), py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f, 1.0f));
    m.def("debug_draw_sphere", [](std::uintptr_t list, const py::sequence& center, float radius,
                                   const py::sequence& color, uint32_t segments) {
        Moss_DebugSphereDesc desc{};
        desc.center = as_float3(center, "center");
        desc.radius = radius;
        desc.color = as_color(color);
        desc.segments = segments;
        return Moss_DebugDrawSphere(from_handle<Moss_DebugDrawList>(list), &desc);
    }, py::arg("list"), py::arg("center"), py::arg("radius"), py::arg("color") = py::make_tuple(1.0f, 1.0f, 1.0f, 1.0f), py::arg("segments") = 24);
    m.def("debug_draw_list_render", [](std::uintptr_t renderer, std::uintptr_t list) {
        Moss_RendererDrawDebugList(from_handle<Moss_Renderer>(renderer), from_handle<Moss_DebugDrawList>(list));
    });
    m.def("debug_draw_list_destroy", [](std::uintptr_t renderer, std::uintptr_t list) {
        Moss_RendererDestroyDebugDrawList(from_handle<Moss_Renderer>(renderer), from_handle<Moss_DebugDrawList>(list));
    });

    // ---------------------------------------------------------------------
    // GPU resources and explicit command recording.
    // Enums are accepted as integer values so PyMoss doesn't need to mirror
    // every backend-specific enum before using resource creation functions.
    // ---------------------------------------------------------------------
    m.def("gpu_device_driver", [](std::uintptr_t device) {
        const char* s = Moss_GetGPUDeviceDriver(from_handle<Moss_GPUDevice>(device));
        return s ? s : "";
    });
    m.def("gpu_device_properties", [](std::uintptr_t device) -> py::object {
        const auto* p = Moss_GetGPUDeviceProperties(from_handle<Moss_GPUDevice>(device));
        if (!p) return py::none();
        py::dict d;
        d["driver_name"] = p->driver_name ? p->driver_name : "";
        d["device_name"] = p->device_name ? p->device_name : "";
        d["vendor_name"] = p->vendor_name ? p->vendor_name : "";
        d["vendor_id"] = p->vendor_id;
        d["device_id"] = p->device_id;
        d["dedicated_video_memory"] = p->dedicated_video_memory;
        d["shared_system_memory"] = p->shared_system_memory;
        d["max_texture_dimension_2d"] = p->max_texture_dimension_2d;
        d["max_texture_array_layers"] = p->max_texture_array_layers;
        d["max_frames_in_flight"] = p->max_frames_in_flight;
        d["supports_compute"] = p->supports_compute;
        d["supports_async_compute"] = p->supports_async_compute;
        d["supports_mesh_shaders"] = p->supports_mesh_shaders;
        d["supports_bindless"] = p->supports_bindless;
        d["supports_debug_markers"] = p->supports_debug_markers;
        return py::object(d);
    });
    m.def("gpu_buffer_create", [](std::uintptr_t device, uint64_t size, int usage, bool cpu_visible) {
        Moss_GPUBufferDesc desc{};
        desc.size = size;
        desc.usage = static_cast<EGPUBufferUsage>(usage);
        desc.cpu_visible = cpu_visible ? 1 : 0;
        return to_handle(Moss_GPUBufferCreate(from_handle<Moss_GPUDevice>(device), &desc));
    }, py::arg("device"), py::arg("size"), py::arg("usage"), py::arg("cpu_visible") = false);
    m.def("gpu_buffer_upload", [](std::uintptr_t device, std::uintptr_t buffer, py::bytes data, uint64_t offset) {
        const std::string bytes = data;
        Moss_GPUBufferUpload(from_handle<Moss_GPUDevice>(device), from_handle<Moss_GPUBuffer>(buffer),
                             bytes.data(), static_cast<uint64_t>(bytes.size()), offset);
    }, py::arg("device"), py::arg("buffer"), py::arg("data"), py::arg("offset") = 0);
    m.def("gpu_buffer_download", [](std::uintptr_t device, std::uintptr_t buffer, uint64_t size, uint64_t offset) {
        std::string bytes(static_cast<size_t>(size), '\0');
        Moss_DownloadFromGPUBuffer(from_handle<Moss_GPUDevice>(device), from_handle<Moss_GPUBuffer>(buffer),
                                   bytes.data(), size, offset);
        return py::bytes(bytes);
    }, py::arg("device"), py::arg("buffer"), py::arg("size"), py::arg("offset") = 0);
    m.def("gpu_buffer_destroy", [](std::uintptr_t device, std::uintptr_t buffer) {
        if (buffer) Moss_GPUBufferDestroy(from_handle<Moss_GPUDevice>(device), from_handle<Moss_GPUBuffer>(buffer));
    });

    m.def("gpu_texture_create", [](std::uintptr_t device, int type, int format, int usage,
                                    uint32_t width, uint32_t height, uint32_t depth,
                                    uint32_t layers, uint32_t mip_levels, bool generate_mips) {
        Moss_TextureDesc desc{};
        desc.type = static_cast<ETextureType>(type);
        desc.format = static_cast<ETextureFormat>(format);
        desc.usage = static_cast<ETextureUsage>(usage);
        desc.width = width;
        desc.height = height;
        desc.depth = depth;
        desc.layers = layers;
        desc.mip_levels = mip_levels;
        desc.generate_mips = generate_mips ? 1 : 0;
        return to_handle(Moss_TextureCreate(from_handle<Moss_GPUDevice>(device), &desc));
    }, py::arg("device"), py::arg("type"), py::arg("format"), py::arg("usage"),
       py::arg("width"), py::arg("height"), py::arg("depth") = 1, py::arg("layers") = 1,
       py::arg("mip_levels") = 1, py::arg("generate_mips") = false);
    m.def("gpu_texture_upload", [](std::uintptr_t device, std::uintptr_t texture, py::bytes data,
                                    uint32_t width, uint32_t height, uint32_t row_pitch,
                                    uint32_t mip_level, uint32_t base_layer) {
        const std::string bytes = data;
        Moss_TextureUploadDesc desc{};
        desc.data = bytes.data();
        desc.width = width;
        desc.height = height;
        desc.depth = 1;
        desc.mip_level = mip_level;
        desc.base_layer = base_layer;
        desc.layer_count = 1;
        desc.row_pitch = row_pitch;
        desc.slice_pitch = static_cast<uint32_t>(bytes.size());
        Moss_TextureUpload(from_handle<Moss_GPUDevice>(device), from_handle<Moss_Texture>(texture), &desc);
    }, py::arg("device"), py::arg("texture"), py::arg("data"), py::arg("width"), py::arg("height"),
       py::arg("row_pitch") = 0, py::arg("mip_level") = 0, py::arg("base_layer") = 0);
    m.def("gpu_texture_generate_mips", [](std::uintptr_t device, std::uintptr_t texture) {
        Moss_TextureGenerateMips(from_handle<Moss_GPUDevice>(device), from_handle<Moss_Texture>(texture));
    });
    m.def("gpu_texture_destroy", [](std::uintptr_t device, std::uintptr_t texture) {
        if (texture) Moss_TextureDestroy(from_handle<Moss_GPUDevice>(device), from_handle<Moss_Texture>(texture));
    });
    m.def("gpu_calculate_texture_format_size", [](int format, uint32_t width, uint32_t height, uint32_t depth) {
        return Moss_CalculateGPUTextureFormatSize(static_cast<ETextureFormat>(format), width, height, depth);
    }, py::arg("format"), py::arg("width"), py::arg("height"), py::arg("depth") = 1);
    m.def("gpu_external_feature_mask", &Moss_GPUGetExternalFeatureMask);
    m.def("gpu_external_feature_available", [](uint32_t feature) {
        return Moss_GPUExternalFeatureAvailable(static_cast<Moss_GPUExternalFeature>(feature));
    });
    m.def("gpu_external_feature_name", [](uint32_t feature) {
        const char* s = Moss_GPUExternalFeatureName(static_cast<Moss_GPUExternalFeature>(feature));
        return s ? s : "";
    });

    // Low-level command buffer calls. Keep lifetime/submit order explicit in Python.
    m.def("gpu_command_buffer_acquire", [](std::uintptr_t device) {
        return to_handle(Moss_AcquireGPUCommandBuffer(from_handle<Moss_GPUDevice>(device)));
    });
    m.def("gpu_command_buffer_cancel", [](std::uintptr_t command) {
        Moss_CancelGPUCommandBuffer(from_handle<Moss_GPUCommandBuffer>(command));
    });
    m.def("gpu_command_buffer_submit", [](std::uintptr_t device, std::uintptr_t command) {
        Moss_SubmitGPUCommandBuffer(from_handle<Moss_GPUDevice>(device), from_handle<Moss_GPUCommandBuffer>(command));
    });
    m.def("gpu_cmd_begin_render_pass", [](std::uintptr_t command, std::uintptr_t framebuffer) {
        Moss_CmdBeginRenderPass(from_handle<Moss_GPUCommandBuffer>(command), from_handle<Moss_Framebuffer>(framebuffer));
    });
    m.def("gpu_cmd_end_render_pass", [](std::uintptr_t command) {
        Moss_CmdEndRenderPass(from_handle<Moss_GPUCommandBuffer>(command));
    });
    m.def("gpu_cmd_bind_pipeline", [](std::uintptr_t command, std::uintptr_t pipeline) {
        Moss_CmdBindPipeline(from_handle<Moss_GPUCommandBuffer>(command), from_handle<Moss_PipelineState>(pipeline));
    });
    m.def("gpu_cmd_bind_resource_set", [](std::uintptr_t command, uint32_t set_index, std::uintptr_t set) {
        Moss_CmdBindResourceSet(from_handle<Moss_GPUCommandBuffer>(command), set_index, from_handle<Moss_ResourceSet>(set));
    });
    m.def("gpu_cmd_bind_vertex_buffer", [](std::uintptr_t command, std::uintptr_t buffer, uint64_t offset) {
        Moss_CmdBindVertexBuffer(from_handle<Moss_GPUCommandBuffer>(command), from_handle<Moss_GPUBuffer>(buffer), offset);
    }, py::arg("command"), py::arg("buffer"), py::arg("offset") = 0);
    m.def("gpu_cmd_bind_index_buffer", [](std::uintptr_t command, std::uintptr_t buffer, uint64_t offset) {
        Moss_CmdBindIndexBuffer(from_handle<Moss_GPUCommandBuffer>(command), from_handle<Moss_GPUBuffer>(buffer), offset);
    }, py::arg("command"), py::arg("buffer"), py::arg("offset") = 0);
    m.def("gpu_cmd_draw", [](std::uintptr_t command, uint32_t vertex_count, uint32_t first_vertex) {
        Moss_CmdDraw(from_handle<Moss_GPUCommandBuffer>(command), vertex_count, first_vertex);
    }, py::arg("command"), py::arg("vertex_count"), py::arg("first_vertex") = 0);
    m.def("gpu_cmd_draw_indexed", [](std::uintptr_t command, uint32_t index_count, uint32_t first_index, int32_t vertex_offset) {
        Moss_CmdDrawIndexed(from_handle<Moss_GPUCommandBuffer>(command), index_count, first_index, vertex_offset);
    }, py::arg("command"), py::arg("index_count"), py::arg("first_index") = 0, py::arg("vertex_offset") = 0);
    m.def("gpu_cmd_dispatch", [](std::uintptr_t command, uint32_t x, uint32_t y, uint32_t z) {
        Moss_CmdDispatch(from_handle<Moss_GPUCommandBuffer>(command), x, y, z);
    }, py::arg("command"), py::arg("x"), py::arg("y"), py::arg("z"));
    m.def("gpu_cmd_clear", [](std::uintptr_t command, float r, float g, float b, float a) {
        Moss_GPUClear(from_handle<Moss_GPUCommandBuffer>(command), r, g, b, a);
    }, py::arg("command"), py::arg("r"), py::arg("g"), py::arg("b"), py::arg("a") = 1.0f);

    // ---------------------------------------------------------------------
    // XR session, views, swapchains, anchors, layers, actions and tracking.
    // ---------------------------------------------------------------------
    m.def("xr_initialize", [](std::uintptr_t renderer) { return Moss_XR_Initialize(from_handle<Moss_Renderer>(renderer)); });
    m.def("xr_shutdown", &Moss_XR_Shutdown);
    m.def("xr_enable_validation_layers", &Moss_XR_EnableValidationLayers);
    m.def("xr_get_capabilities", []() -> py::object {
        const auto* c = Moss_XR_GetCapabilities();
        if (!c) return py::none();
        py::dict d;
        d["view_count"] = c->viewCount;
        d["view_type"] = static_cast<unsigned>(c->viewType);
        d["hand_tracking"] = c->handTracking;
        d["eye_tracking"] = c->eyeTracking;
        d["body_tracking"] = c->bodyTracking;
        d["face_tracking"] = c->faceTracking;
        d["passthrough"] = c->passthrough;
        d["anchors"] = c->anchors;
        d["depth_layers"] = c->depthLayers;
        return py::object(d);
    });
    m.def("xr_session_create", [] { return to_handle(Moss_XR_CreateSession()); });
    m.def("xr_session_destroy", [](std::uintptr_t session) { if (session) Moss_XR_DestroySession(from_handle<MossXR_Session>(session)); });
    m.def("xr_session_state", [](std::uintptr_t session) { return static_cast<unsigned>(Moss_XR_GetSessionState(from_handle<MossXR_Session>(session))); });
    m.def("xr_begin_frame", [](std::uintptr_t session) { return Moss_XR_BeginFrame(from_handle<MossXR_Session>(session)); });
    m.def("xr_end_frame", [](std::uintptr_t session) { Moss_XR_EndFrame(from_handle<MossXR_Session>(session)); });
    m.def("xr_predicted_display_time", [](std::uintptr_t session) { return static_cast<uint64_t>(Moss_XR_GetPredictedDisplayTime(from_handle<MossXR_Session>(session))); });
    m.def("xr_delta_seconds", &Moss_XR_GetDeltaSeconds);
    m.def("xr_handle_events", [](std::uintptr_t session) { Moss_XR_HandleEvents(from_handle<MossXR_Session>(session)); });
    m.def("xr_view_count", [](std::uintptr_t session) { return Moss_XR_GetViewCount(from_handle<MossXR_Session>(session)); });
    m.def("xr_get_view", [](std::uintptr_t session, uint32_t index) -> py::object {
        MossXR_View view{};
        if (!Moss_XR_GetView(from_handle<MossXR_Session>(session), index, &view)) return py::none();
        py::dict d;
        d["pose"] = xr_pose_dict(view.pose);
        d["fov"] = py::make_tuple(view.fov.left, view.fov.right, view.fov.up, view.fov.down);
        // Mat44 byte blobs intentionally preserve the native matrix representation;
        // consumers should only decode them using the same Moss build/ABI.
        d["view_matrix_bytes"] = py::bytes(reinterpret_cast<const char*>(&view.view), sizeof(view.view));
        d["projection_matrix_bytes"] = py::bytes(reinterpret_cast<const char*>(&view.projection), sizeof(view.projection));
        return py::object(d);
    });
    m.def("xr_swapchain_create", [](std::uintptr_t session, uint32_t width, uint32_t height, int format) {
        return to_handle(Moss_XR_CreateSwapchain(from_handle<MossXR_Session>(session), width, height, static_cast<ETextureFormat>(format)));
    }, py::arg("session"), py::arg("width"), py::arg("height"), py::arg("format"));
    m.def("xr_swapchain_destroy", [](std::uintptr_t swapchain) { if (swapchain) Moss_XR_DestroySwapchain(from_handle<MossXR_Swapchain>(swapchain)); });
    m.def("xr_swapchain_acquire_image", [](std::uintptr_t swapchain) {
        uint32_t index = 0;
        void* image = Moss_XR_AcquireSwapchainImage(from_handle<MossXR_Swapchain>(swapchain), &index);
        return py::make_tuple(to_handle(image), index);
    });
    m.def("xr_swapchain_release_image", [](std::uintptr_t swapchain, uint32_t image_index) {
        Moss_XR_ReleaseSwapchainImage(from_handle<MossXR_Swapchain>(swapchain), image_index);
    });
    m.def("xr_anchor_create", [](const py::sequence& position, const py::sequence& orientation) {
        require_size(orientation, 4, "orientation_xyzw");
        MossXR_Pose pose{};
        pose.position = as_vec3(position, "position");
        pose.orientation = Quat(orientation[0].cast<float>(), orientation[1].cast<float>(),
                                orientation[2].cast<float>(), orientation[3].cast<float>());
        return to_handle(Moss_XR_CreateAnchor(&pose));
    }, py::arg("position"), py::arg("orientation_xyzw") = py::make_tuple(0.0f, 0.0f, 0.0f, 1.0f));
    m.def("xr_anchor_destroy", [](std::uintptr_t anchor) { if (anchor) Moss_XR_DestroyAnchor(from_handle<MossXR_Anchor>(anchor)); });
    m.def("xr_anchor_locate", [](std::uintptr_t anchor, uint64_t time) -> py::object {
        MossXR_Pose pose{};
        if (!Moss_XR_LocateAnchor(from_handle<MossXR_Anchor>(anchor), static_cast<MossXR_Time>(time), &pose)) return py::none();
        return py::object(xr_pose_dict(pose));
    });
    m.def("xr_projection_layer_create", [](std::uintptr_t swapchain) {
        return to_handle(Moss_XR_CreateProjectionLayer(from_handle<MossXR_Swapchain>(swapchain)));
    });
    m.def("xr_quad_layer_create", [](std::uintptr_t swapchain, const py::sequence& position,
                                      const py::sequence& orientation, const py::sequence& size) {
        require_size(orientation, 4, "orientation_xyzw");
        MossXR_Pose pose{};
        pose.position = as_vec3(position, "position");
        pose.orientation = Quat(orientation[0].cast<float>(), orientation[1].cast<float>(),
                                orientation[2].cast<float>(), orientation[3].cast<float>());
        return to_handle(Moss_XR_CreateQuadLayer(from_handle<MossXR_Swapchain>(swapchain), &pose, as_vec2(size, "size")));
    }, py::arg("swapchain"), py::arg("position"), py::arg("orientation_xyzw"), py::arg("size"));
    m.def("xr_layer_destroy", [](std::uintptr_t layer) { if (layer) Moss_XR_DestroyLayer(from_handle<MossXR_Layer>(layer)); });
    m.def("xr_submit_layers", [](std::uintptr_t session, const std::vector<std::uintptr_t>& layers) {
        std::vector<MossXR_Layer*> ptrs;
        ptrs.reserve(layers.size());
        for (const auto h : layers) ptrs.push_back(from_handle<MossXR_Layer>(h));
        Moss_XR_SubmitLayers(from_handle<MossXR_Session>(session), static_cast<uint32_t>(ptrs.size()), ptrs.data());
    });

    m.def("xr_action_set_create", [](const std::string& name) { return to_handle(Moss_XR_CreateActionSet(name.c_str())); });
    m.def("xr_action_set_destroy", [](std::uintptr_t set) { if (set) Moss_XR_DestroyActionSet(from_handle<MossXR_ActionSet>(set)); });
    m.def("xr_action_create", [](std::uintptr_t set, const std::string& name, uint8_t type) {
        return to_handle(Moss_XR_CreateAction(from_handle<MossXR_ActionSet>(set), name.c_str(), static_cast<MossXR_ActionType>(type)));
    });
    m.def("xr_action_destroy", [](std::uintptr_t action) { if (action) Moss_XR_DestroyAction(from_handle<MossXR_Action>(action)); });
    m.def("xr_attach_action_set", [](std::uintptr_t session, std::uintptr_t set) {
        Moss_XR_AttachActionSet(from_handle<MossXR_Session>(session), from_handle<MossXR_ActionSet>(set));
    });
    m.def("xr_sync_actions", [](std::uintptr_t session) { Moss_XR_SyncActions(from_handle<MossXR_Session>(session)); });
    m.def("xr_action_boolean", [](std::uintptr_t action) { return Moss_XR_GetActionBoolean(from_handle<MossXR_Action>(action)); });
    m.def("xr_action_float", [](std::uintptr_t action) { return Moss_XR_GetActionFloat(from_handle<MossXR_Action>(action)); });
    m.def("xr_action_pose", [](std::uintptr_t action) -> py::object {
        MossXR_Pose pose{};
        if (!Moss_XR_GetActionPose(from_handle<MossXR_Action>(action), &pose)) return py::none();
        return py::object(xr_pose_dict(pose));
    });
    m.def("xr_action_stop_haptic", [](std::uintptr_t action) { Moss_XR_StopHaptic(from_handle<MossXR_Action>(action)); });
    m.def("xr_hand_create", [] { return to_handle(Moss_XR_CreateHand()); });
    m.def("xr_hand_destroy", [](std::uintptr_t hand) { if (hand) Moss_XR_DestroyHand(from_handle<MossXR_HandModifier>(hand)); });
    m.def("xr_hand_pose", [](std::uintptr_t hand) -> py::object {
        MossXR_Pose pose{};
        if (!Moss_XR_GetHandPose(from_handle<MossXR_HandModifier>(hand), &pose)) return py::none();
        return py::object(xr_pose_dict(pose));
    });
    m.def("xr_hand_joint_pose", [](std::uintptr_t hand, uint32_t joint) -> py::object {
        MossXR_Pose pose{};
        if (!Moss_XR_GetHandJointPose(from_handle<MossXR_HandModifier>(hand), static_cast<XRHandJoint>(joint), &pose)) return py::none();
        return py::object(xr_pose_dict(pose));
    });
    m.def("xr_body_create", [] { return to_handle(Moss_XR_CreateBody()); });
    m.def("xr_body_destroy", [](std::uintptr_t body) { if (body) Moss_XR_DestroyBody(from_handle<MossXR_BodyModifier>(body)); });
    m.def("xr_body_joint_pose", [](std::uintptr_t body, uint32_t joint) -> py::object {
        MossXR_Pose pose{};
        if (!Moss_XR_GetBodyJointPose(from_handle<MossXR_BodyModifier>(body), static_cast<XRBodyJoint>(joint), &pose)) return py::none();
        return py::object(xr_pose_dict(pose));
    });

    // ---------------------------------------------------------------------
    // Small, safe physics helpers. Moss_Physics exposes a large Jolt-style C++
    // object model rather than a single create-world C API; these helpers map
    // public value/filter APIs without fabricating a Moss world constructor.
    // ---------------------------------------------------------------------
    py::class_<PhysicsMaterial>(m, "PhysicsMaterial")
        .def(py::init<>())
        .def_readwrite("static_friction", &PhysicsMaterial::staticFriction)
        .def_readwrite("dynamic_friction", &PhysicsMaterial::dynamicFriction)
        .def_readwrite("restitution", &PhysicsMaterial::restitution)
        .def_readwrite("density", &PhysicsMaterial::density);
    m.def("physics_object_layer_from_mask", &ObjectLayerPairFilterMask_GetObjectLayer,
          py::arg("group"), py::arg("mask"));
    m.def("physics_object_layer_group", &ObjectLayerPairFilterMask_GetGroup, py::arg("layer"));
    m.def("physics_object_layer_mask", &ObjectLayerPairFilterMask_GetMask, py::arg("layer"));
    m.def("physics_broadphase_mask_create", [](uint32_t layer_count) {
        return to_handle(BroadPhaseLayerInterfaceMask_Create(layer_count));
    }, py::arg("layer_count"));
    m.def("physics_broadphase_mask_configure_layer", [](std::uintptr_t iface, uint8_t layer,
                                                          uint32_t include_groups, uint32_t exclude_groups) {
        BroadPhaseLayerInterfaceMask_ConfigureLayer(from_handle<BroadPhaseLayerInterface>(iface), layer,
                                                    include_groups, exclude_groups);
    });
    m.def("physics_broadphase_table_create", [](uint32_t object_layers, uint32_t broadphase_layers) {
        return to_handle(BroadPhaseLayerInterfaceTable_Create(object_layers, broadphase_layers));
    }, py::arg("object_layer_count"), py::arg("broadphase_layer_count"));
    m.def("physics_broadphase_table_map_layer", [](std::uintptr_t iface, uint32_t object_layer, uint8_t broadphase_layer) {
        BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer(from_handle<BroadPhaseLayerInterface>(iface),
                                                                  object_layer, broadphase_layer);
    });
}

PYBIND11_MODULE(_pymoss, m) {
    m.doc() = "Native Python bindings for the Moss Framework.";

    py::class_<Window>(m, "Window")
        .def(py::init<const std::string&, int, int>(), py::arg("title"), py::arg("width"), py::arg("height"))
        .def("should_close", &Window::should_close)
        .def("is_open", &Window::is_open)
        .def("close", &Window::close)
        .def("set_title", &Window::set_title)
        .def("native_handle", &Window::native_handle)
        .def("__enter__", [](Window& self) -> Window& { return self; }, py::return_value_policy::reference_internal)
        .def("__exit__", [](Window& self, py::object, py::object, py::object) { self.close(); });

    py::class_<Monitor>(m, "Monitor")
        .def("valid", &Monitor::valid)
        .def("name", &Monitor::name)
        .def("position", &Monitor::position)
        .def("physical_size", &Monitor::physical_size)
        .def("content_scale", &Monitor::content_scale)
        .def("native_handle", &Monitor::native_handle);

    py::class_<Gamepad>(m, "Gamepad")
        .def("close", &Gamepad::close)
        .def("connected", &Gamepad::connected)
        .def("button_pressed", &Gamepad::button_pressed)
        .def("button_just_pressed", &Gamepad::button_just_pressed)
        .def("button_just_released", &Gamepad::button_just_released)
        .def("axis", &Gamepad::axis)
        .def("name", &Gamepad::name);

    // Register the actual enum types from Moss_Platform.h, rather than the
    // obsolete Moss_KEY_* identifiers used by the previous public main.cpp.
    auto keyboard = py::enum_<Keyboard>(m, "Keyboard");
#define PYMOSS_KEY(name) keyboard.value(#name, Keyboard::name)
    PYMOSS_KEY(KEY_0); PYMOSS_KEY(KEY_1); PYMOSS_KEY(KEY_2); PYMOSS_KEY(KEY_3); PYMOSS_KEY(KEY_4);
    PYMOSS_KEY(KEY_5); PYMOSS_KEY(KEY_6); PYMOSS_KEY(KEY_7); PYMOSS_KEY(KEY_8); PYMOSS_KEY(KEY_9);
    PYMOSS_KEY(KEY_A); PYMOSS_KEY(KEY_B); PYMOSS_KEY(KEY_C); PYMOSS_KEY(KEY_D); PYMOSS_KEY(KEY_E);
    PYMOSS_KEY(KEY_F); PYMOSS_KEY(KEY_G); PYMOSS_KEY(KEY_H); PYMOSS_KEY(KEY_I); PYMOSS_KEY(KEY_J);
    PYMOSS_KEY(KEY_K); PYMOSS_KEY(KEY_L); PYMOSS_KEY(KEY_M); PYMOSS_KEY(KEY_N); PYMOSS_KEY(KEY_O);
    PYMOSS_KEY(KEY_P); PYMOSS_KEY(KEY_Q); PYMOSS_KEY(KEY_R); PYMOSS_KEY(KEY_S); PYMOSS_KEY(KEY_T);
    PYMOSS_KEY(KEY_U); PYMOSS_KEY(KEY_V); PYMOSS_KEY(KEY_W); PYMOSS_KEY(KEY_X); PYMOSS_KEY(KEY_Y); PYMOSS_KEY(KEY_Z);
    PYMOSS_KEY(KEY_BACKSPACE); PYMOSS_KEY(KEY_DELETE); PYMOSS_KEY(KEY_ENTER); PYMOSS_KEY(KEY_ESCAPE);
    PYMOSS_KEY(KEY_SPACE); PYMOSS_KEY(KEY_TAB); PYMOSS_KEY(KEY_LEFT); PYMOSS_KEY(KEY_RIGHT); PYMOSS_KEY(KEY_UP); PYMOSS_KEY(KEY_DOWN);
    PYMOSS_KEY(KEY_LEFT_ALT); PYMOSS_KEY(KEY_LEFT_CONTROL); PYMOSS_KEY(KEY_LEFT_SHIFT); PYMOSS_KEY(KEY_LEFT_SUPER);
    PYMOSS_KEY(KEY_RIGHT_ALT); PYMOSS_KEY(KEY_RIGHT_CONTROL); PYMOSS_KEY(KEY_RIGHT_SHIFT); PYMOSS_KEY(KEY_RIGHT_SUPER);
    PYMOSS_KEY(KEY_F1); PYMOSS_KEY(KEY_F2); PYMOSS_KEY(KEY_F3); PYMOSS_KEY(KEY_F4); PYMOSS_KEY(KEY_F5); PYMOSS_KEY(KEY_F6);
    PYMOSS_KEY(KEY_F7); PYMOSS_KEY(KEY_F8); PYMOSS_KEY(KEY_F9); PYMOSS_KEY(KEY_F10); PYMOSS_KEY(KEY_F11); PYMOSS_KEY(KEY_F12);
#undef PYMOSS_KEY
    keyboard.export_values();

    auto mouse = py::enum_<Mouse>(m, "Mouse");
    mouse.value("LEFT", Mouse::LEFT).value("RIGHT", Mouse::RIGHT).value("MIDDLE", Mouse::MIDDLE)
         .value("BUTTON_4", Mouse::BUTTON_4).value("BUTTON_5", Mouse::BUTTON_5)
         .value("BUTTON_6", Mouse::BUTTON_6).value("BUTTON_7", Mouse::BUTTON_7).value("BUTTON_8", Mouse::BUTTON_8);
    mouse.export_values();

    py::enum_<GamepadAxis>(m, "GamepadAxis")
        .value("LEFT_X", GamepadAxis::LEFT_X).value("LEFT_Y", GamepadAxis::LEFT_Y)
        .value("RIGHT_X", GamepadAxis::RIGHT_X).value("RIGHT_Y", GamepadAxis::RIGHT_Y)
        .value("LEFT_TRIGGER", GamepadAxis::LEFT_TRIGGER).value("RIGHT_TRIGGER", GamepadAxis::RIGHT_TRIGGER)
        .value("TOUCHPAD_X", GamepadAxis::TOUCHPAD_X).value("TOUCHPAD_Y", GamepadAxis::TOUCHPAD_Y)
        .value("GYRO_X", GamepadAxis::GYRO_X).value("GYRO_Y", GamepadAxis::GYRO_Y).value("GYRO_Z", GamepadAxis::GYRO_Z);
    py::enum_<Moss_GamepadButton>(m, "GamepadButton")
        .value("INVALID", Moss_GamepadButton::INVALID).value("SOUTH", Moss_GamepadButton::SOUTH)
        .value("EAST", Moss_GamepadButton::EAST).value("WEST", Moss_GamepadButton::WEST)
        .value("NORTH", Moss_GamepadButton::NORTH).value("BACK", Moss_GamepadButton::BACK)
        .value("GUIDE", Moss_GamepadButton::GUIDE).value("START", Moss_GamepadButton::START)
        .value("LEFT_STICK", Moss_GamepadButton::LEFT_STICK).value("RIGHT_STICK", Moss_GamepadButton::RIGHT_STICK)
        .value("LEFT_SHOULDER", Moss_GamepadButton::LEFT_SHOULDER).value("RIGHT_SHOULDER", Moss_GamepadButton::RIGHT_SHOULDER)
        .value("DPAD_UP", Moss_GamepadButton::DPAD_UP).value("DPAD_DOWN", Moss_GamepadButton::DPAD_DOWN)
        .value("DPAD_LEFT", Moss_GamepadButton::DPAD_LEFT).value("DPAD_RIGHT", Moss_GamepadButton::DPAD_RIGHT);

    m.def("num_gamepads", &Moss_GetNumGamepads);
    m.def("update_gamepads", &Moss_UpdateGamepads);
    m.def("cpu_count", &Moss_GetAvailableCPUCores);
    m.def("cpu_cache_line_size", &Moss_GetCPUCacheLineSize);
    m.def("system_ram", &Moss_GetSystemRAM);
    m.def("open_url", [](const std::string& url) { return Moss_OpenURL(url.c_str()); }, py::arg("url"));
    m.def("poll_events", &Moss_PollEvents, "Process pending platform events.");
    m.def("window_size", [] { return py::make_tuple(Moss_GetWindowWidth(), Moss_GetWindowHeight()); });
    m.def("mouse_position", [] { int x = 0, y = 0; Moss_GetMousePosition(&x, &y); return py::make_tuple(x, y); });
    m.def("set_mouse_position", &Moss_SetMousePosition, py::arg("x"), py::arg("y"));
    m.def("set_mouse_visible", &Moss_SetMouseVisible, py::arg("visible"));
    m.def("key_pressed", [](Keyboard key) { return Moss_IsKeyPressed(key); }, py::arg("key"));
    m.def("key_released", [](Keyboard key) { return Moss_IsReleased(key); }, py::arg("key"));
    m.def("key_just_pressed", [](Keyboard key) { return Moss_IsKeyJustPressed(key); }, py::arg("key"));
    m.def("key_just_released", [](Keyboard key) { return Moss_IsKeyJustReleased(key); }, py::arg("key"));
    m.def("mouse_pressed", [](Mouse button) { return Moss_IsMousePressed(button); }, py::arg("button"));
    m.def("mouse_released", [](Mouse button) { return Moss_IsMouseReleased(button); }, py::arg("button"));
    m.def("mouse_just_pressed", [](Mouse button) { return Moss_IsMouseJustPressed(button); }, py::arg("button"));
    m.def("mouse_just_released", [](Mouse button) { return Moss_IsMouseJustReleased(button); }, py::arg("button"));
    m.def("primary_monitor", []() -> py::object {
        Moss_Monitor* monitor = Moss_MonitorGetPrimary();
        if (!monitor) return py::none();
        return py::cast(Monitor(monitor));
    });
    m.def("secondary_monitor", []() -> py::object {
        Moss_Monitor* monitor = Moss_MonitorGetSecondary();
        if (!monitor) return py::none();
        return py::cast(Monitor(monitor));
    });
    m.def("primary_monitor_name", []() -> py::object {
        auto* monitor = Moss_MonitorGetPrimary();
        if (!monitor) return py::none();
        const char* value = Moss_MonitorGetName(monitor);
        return value ? py::cast(value) : py::none();
    });
    m.def("primary_monitor_position", []() -> py::object {
        auto* monitor = Moss_MonitorGetPrimary();
        if (!monitor) return py::none();
        int x = 0, y = 0; Moss_MonitorGetPosition(monitor, &x, &y);
        return py::make_tuple(x, y);
    });
    m.def("primary_monitor_physical_size", []() -> py::object {
        auto* monitor = Moss_MonitorGetPrimary();
        if (!monitor) return py::none();
        int w = 0, h = 0; Moss_MonitorGetPhysicalSize(monitor, &w, &h);
        return py::make_tuple(w, h);
    });
    m.def("primary_monitor_content_scale", []() -> py::object {
        auto* monitor = Moss_MonitorGetPrimary();
        if (!monitor) return py::none();
        float x = 1.0f, y = 1.0f; Moss_MonitorGetContentScale(monitor, &x, &y);
        return py::make_tuple(x, y);
    });
    m.def("open_gamepad", [](Moss_GamepadID id) {
        Moss_Gamepad* gamepad = Moss_OpenGamepad(id);
        if (!gamepad) throw std::runtime_error("Moss_OpenGamepad failed");
        return std::make_unique<Gamepad>(gamepad);
    }, py::arg("id"));

    // All extended functions are inlined above; no extra binding header is required.
    bind_pymoss_extra_bindings(m);
}
