#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/pytypes.h>

#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <memory>

#include <Moss/Moss_stdinc.h>
#include <Moss/Moss_Platform.h>
#include <Moss/Moss_Audio.h>

namespace py = pybind11;

namespace {

template <typename T>
using Handle = std::uintptr_t;

template <typename T>
T* ptr(Handle<T> h) { return reinterpret_cast<T*>(h); }

template <typename T>
Handle<T> handle(T* p) { return reinterpret_cast<Handle<T>>(p); }

template <typename T>
py::dict path_info_dict(const T* info) {
    py::dict d;
    if (!info) return d;
    d["type"] = static_cast<int>(info->type);
    d["size"] = info->size;
    d["create_time"] = info->create_time;
    d["modify_time"] = info->modify_time;
    d["access_time"] = info->access_time;
    d["readable"] = info->readable;
    d["writable"] = info->writable;
    d["executable"] = info->executable;
    return d;
}

static py::dict camera_spec_dict(const Moss_CameraSpec& s) {
    py::dict d;
    d["format"] = static_cast<int>(s.format);
    d["colorspace"] = static_cast<int>(s.colorspace);
    d["width"] = s.width;
    d["height"] = s.height;
    d["framerate_numerator"] = s.framerate_numerator;
    d["framerate_denominator"] = s.framerate_denominator;
    return d;
}

static py::dict microphone_levels_dict(const Moss_MicrophoneLevels& x) {
    py::dict d;
    d["rms"] = x.rms;
    d["peak"] = x.peak;
    d["smoothed_volume"] = x.smoothed_volume;
    d["voice_activity"] = x.voice_activity;
    return d;
}

static py::dict ray_result_dict(const Moss_AudioRayTraceResult& x) {
    py::dict d;
    d["audible"] = x.audible;
    d["occluded"] = x.occluded;
    d["distance"] = x.distance;
    d["attenuation"] = x.attenuation;
    d["occlusion"] = x.occlusion;
    d["transmission_gain"] = x.transmission_gain;
    d["lowpass"] = x.lowpass;
    d["reflection_gain"] = x.reflection_gain;
    d["reflection_delay_seconds"] = x.reflection_delay_seconds;
    d["delay_seconds"] = x.delay_seconds;
    return d;
}

// Python callback bridges. The Moss callback typedefs are C function pointers, so
// Python callables are stored here and invoked under the GIL.
static std::unique_ptr<py::function> g_framebuffer_cb;
static std::unique_ptr<py::function> g_window_size_cb;
static std::unique_ptr<py::function> g_window_resize_cb;
static std::unique_ptr<py::function> g_window_position_cb;
static std::unique_ptr<py::function> g_window_focus_cb;
static std::unique_ptr<py::function> g_window_scale_cb;
static std::unique_ptr<py::function> g_monitor_cb;

static void framebuffer_cb(int w, int h) { if (g_framebuffer_cb) { py::gil_scoped_acquire gil; (*g_framebuffer_cb)(w,h); } }
static void window_size_cb(int w, int h) { if (g_window_size_cb) { py::gil_scoped_acquire gil; (*g_window_size_cb)(w,h); } }
static void window_resize_cb(int w, int h) { if (g_window_resize_cb) { py::gil_scoped_acquire gil; (*g_window_resize_cb)(w,h); } }
static void window_position_cb(int x, int y) { if (g_window_position_cb) { py::gil_scoped_acquire gil; (*g_window_position_cb)(x,y); } }
static void window_focus_cb(bool focused) { if (g_window_focus_cb) { py::gil_scoped_acquire gil; (*g_window_focus_cb)(focused); } }
static void window_scale_cb(float x, float y) { if (g_window_scale_cb) { py::gil_scoped_acquire gil; (*g_window_scale_cb)(x,y); } }
static void monitor_cb(const char* name, bool connected) { if (g_monitor_cb) { py::gil_scoped_acquire gil; (*g_monitor_cb)(name ? name : "", connected); } }

static void set_cb(std::unique_ptr<py::function>& dst, py::function fn) { dst = std::make_unique<py::function>(std::move(fn)); }

} // namespace

PYBIND11_MODULE(_pymoss, m) {
    m.doc() = "Native Python bindings for the Moss Framework.";

    // -------------------------------------------------------------------------
    // Standard library / time API
    // -------------------------------------------------------------------------
    m.def("degrees_to_radians", &DegreesToRadians, py::arg("value"));
    m.def("radians_to_degrees", &RadiansToDegrees, py::arg("value"));
    m.def("center_angle_around_zero", &CenterAngleAroundZero, py::arg("value"));
    m.def("count_trailing_zeros", &CountTrailingZeros, py::arg("value"));
    m.def("count_leading_zeros", &CountLeadingZeros, py::arg("value"));
    m.def("count_bits", &CountBits, py::arg("value"));
    m.def("get_next_power_of_2", &GetNextPowerOf2, py::arg("value"));
    m.def("clamp_float", [](float v,float lo,float hi){return Clamp(v,lo,hi);}, py::arg("value"),py::arg("min"),py::arg("max"));
    m.def("lerp_float", [](float v,float lo,float hi){return Lerp(v,lo,hi);}, py::arg("value"),py::arg("min"),py::arg("max"));
    m.def("square_float", [](float v){return Square(v);});
    m.def("cubed_float", [](float v){return Cubed(v);});
    m.def("sign_float", [](float v){return Sign(v);});
    m.def("align_up", [](std::uint64_t v,std::uint64_t a){return AlignUp(v,a);});
    m.def("is_aligned", [](std::uint64_t v,std::uint64_t a){return IsAligned(v,a);});

    m.def("get_ticks", &Moss_GetTicks);
    m.def("get_seconds", &Moss_GetSeconds, py::arg("ticks"));
    m.def("get_milliseconds", &Moss_GetMilliseconds, py::arg("ticks"));
    m.def("get_milliseconds_and_reset", [](Moss_Time ticks){ auto t=ticks; auto r=Moss_GetMillisecondsAndReset(&t); return py::make_tuple(r,t); });
    m.def("yield_cpu", &Moss_Yield);
    m.def("delay", &Moss_Delay, py::arg("milliseconds"));
    m.def("local_time", []{ const char* s=Moss_LocalTime(); return s?s:""; });
    m.def("timestamp", []{ const char* s=Moss_TimeStamp(); return s?s:""; });
    m.def("time_now", []{ const char* s=Moss_TimeNow(); return s?s:""; });
    m.def("ctime_now", []{ const char* s=Moss_CTimeNow(); return s?s:""; });
    m.def("timestamp_et", []{ const char* s=Moss_TimeStampET(); return s?s:""; });
    m.def("timestamp_ct", []{ const char* s=Moss_TimeStampCT(); return s?s:""; });
    m.def("timestamp_mt", []{ const char* s=Moss_TimeStampMT(); return s?s:""; });
    m.def("timestamp_pt", []{ const char* s=Moss_TimeStampPT(); return s?s:""; });
    m.def("format_time", [](const std::string& fmt){ const char* s=Moss_FormatTime(fmt.c_str()); return s?s:""; });

    // -------------------------------------------------------------------------
    // Enums
    // -------------------------------------------------------------------------
    py::enum_<Keyboard>(m,"Keyboard").value("KEY_0",Keyboard::KEY_0).value("KEY_1",Keyboard::KEY_1).value("KEY_2",Keyboard::KEY_2).value("KEY_3",Keyboard::KEY_3).value("KEY_4",Keyboard::KEY_4).value("KEY_5",Keyboard::KEY_5).value("KEY_6",Keyboard::KEY_6).value("KEY_7",Keyboard::KEY_7).value("KEY_8",Keyboard::KEY_8).value("KEY_9",Keyboard::KEY_9).value("KEY_A",Keyboard::KEY_A).value("KEY_B",Keyboard::KEY_B).value("KEY_C",Keyboard::KEY_C).value("KEY_D",Keyboard::KEY_D).value("KEY_E",Keyboard::KEY_E).value("KEY_F",Keyboard::KEY_F).value("KEY_G",Keyboard::KEY_G).value("KEY_H",Keyboard::KEY_H).value("KEY_I",Keyboard::KEY_I).value("KEY_J",Keyboard::KEY_J).value("KEY_K",Keyboard::KEY_K).value("KEY_L",Keyboard::KEY_L).value("KEY_M",Keyboard::KEY_M).value("KEY_N",Keyboard::KEY_N).value("KEY_O",Keyboard::KEY_O).value("KEY_P",Keyboard::KEY_P).value("KEY_Q",Keyboard::KEY_Q).value("KEY_R",Keyboard::KEY_R).value("KEY_S",Keyboard::KEY_S).value("KEY_T",Keyboard::KEY_T).value("KEY_U",Keyboard::KEY_U).value("KEY_V",Keyboard::KEY_V).value("KEY_W",Keyboard::KEY_W).value("KEY_X",Keyboard::KEY_X).value("KEY_Y",Keyboard::KEY_Y).value("KEY_Z",Keyboard::KEY_Z).value("KEY_SPACE",Keyboard::KEY_SPACE).value("KEY_ENTER",Keyboard::KEY_ENTER).value("KEY_ESCAPE",Keyboard::KEY_ESCAPE).value("KEY_TAB",Keyboard::KEY_TAB).value("KEY_BACKSPACE",Keyboard::KEY_BACKSPACE).value("KEY_DELETE",Keyboard::KEY_DELETE).value("KEY_UP",Keyboard::KEY_UP).value("KEY_DOWN",Keyboard::KEY_DOWN).value("KEY_LEFT",Keyboard::KEY_LEFT).value("KEY_RIGHT",Keyboard::KEY_RIGHT).export_values();
    py::enum_<Mouse>(m,"Mouse").value("LEFT",Mouse::LEFT).value("RIGHT",Mouse::RIGHT).value("MIDDLE",Mouse::MIDDLE).value("BUTTON_4",Mouse::BUTTON_4).value("BUTTON_5",Mouse::BUTTON_5).value("BUTTON_6",Mouse::BUTTON_6).value("BUTTON_7",Mouse::BUTTON_7).value("BUTTON_8",Mouse::BUTTON_8).export_values();
    py::enum_<Moss_GamepadButton>(m,"GamepadButton").value("INVALID",Moss_GamepadButton::INVALID).value("SOUTH",Moss_GamepadButton::SOUTH).value("EAST",Moss_GamepadButton::EAST).value("WEST",Moss_GamepadButton::WEST).value("NORTH",Moss_GamepadButton::NORTH).value("BACK",Moss_GamepadButton::BACK).value("GUIDE",Moss_GamepadButton::GUIDE).value("START",Moss_GamepadButton::START).value("LEFT_STICK",Moss_GamepadButton::LEFT_STICK).value("RIGHT_STICK",Moss_GamepadButton::RIGHT_STICK).value("LEFT_SHOULDER",Moss_GamepadButton::LEFT_SHOULDER).value("RIGHT_SHOULDER",Moss_GamepadButton::RIGHT_SHOULDER).value("DPAD_UP",Moss_GamepadButton::DPAD_UP).value("DPAD_DOWN",Moss_GamepadButton::DPAD_DOWN).value("DPAD_LEFT",Moss_GamepadButton::DPAD_LEFT).value("DPAD_RIGHT",Moss_GamepadButton::DPAD_RIGHT).export_values();
    py::enum_<GamepadAxis>(m,"GamepadAxis").value("LEFT_X",GamepadAxis::LEFT_X).value("LEFT_Y",GamepadAxis::LEFT_Y).value("RIGHT_X",GamepadAxis::RIGHT_X).value("RIGHT_Y",GamepadAxis::RIGHT_Y).value("LEFT_TRIGGER",GamepadAxis::LEFT_TRIGGER).value("RIGHT_TRIGGER",GamepadAxis::RIGHT_TRIGGER).value("TOUCHPAD_X",GamepadAxis::TOUCHPAD_X).value("TOUCHPAD_Y",GamepadAxis::TOUCHPAD_Y).value("GYRO_X",GamepadAxis::GYRO_X).value("GYRO_Y",GamepadAxis::GYRO_Y).value("GYRO_Z",GamepadAxis::GYRO_Z).export_values();
    py::enum_<Moss_CursorMode>(m,"CursorMode").value("VISIBLE",Moss_CursorMode::VISIBLE).value("HIDDEN",Moss_CursorMode::HIDDEN).value("CAPTURED",Moss_CursorMode::CAPTURED).value("CONFINED",Moss_CursorMode::CONFINED).value("CONFINED_HIDDEN",Moss_CursorMode::CONFINED_HIDDEN);
    py::enum_<Moss_CursorShape>(m,"CursorShape").value("ARROW",Moss_CursorShape::ARROW).value("IBEAM",Moss_CursorShape::IBEAM).value("POINTING_HAND",Moss_CursorShape::POINTING_HAND).value("CROSS",Moss_CursorShape::CROSS).value("WAIT",Moss_CursorShape::WAIT).value("BUSY",Moss_CursorShape::BUSY).value("DRAG",Moss_CursorShape::DRAG).value("FORBIDDEN",Moss_CursorShape::FORBIDDEN).value("VSIZE",Moss_CursorShape::VSIZE).value("HSIZE",Moss_CursorShape::HSIZE).value("MOVE",Moss_CursorShape::MOVE).value("HELP",Moss_CursorShape::HELP);
    py::enum_<Moss_PowerState>(m,"PowerState").value("ERROR",Moss_PowerState::ERROR).value("UNKNOWN",Moss_PowerState::UNKNOWN).value("ON_BATTERY",Moss_PowerState::ON_BATTERY).value("NO_BATTERY",Moss_PowerState::NO_BATTERY).value("CHARGING",Moss_PowerState::CHARGING).value("CHARGED",Moss_PowerState::CHARGED);
    py::enum_<Moss_PathType>(m,"PathType").value("UNKNOWN",Moss_PathType::UNKNOWN).value("FILE",Moss_PathType::FILE).value("DIRECTORY",Moss_PathType::DIRECTORY).value("SYMLINK",Moss_PathType::SYMLINK);
    py::enum_<Moss_UserFolder>(m,"UserFolder").value("HOME",Moss_UserFolder::HOME).value("DESKTOP",Moss_UserFolder::DESKTOP).value("DOCUMENTS",Moss_UserFolder::DOCUMENTS).value("DOWNLOADS",Moss_UserFolder::DOWNLOADS).value("PICTURES",Moss_UserFolder::PICTURES).value("MUSIC",Moss_UserFolder::MUSIC).value("VIDEOS",Moss_UserFolder::VIDEOS).value("APPDATA",Moss_UserFolder::APPDATA).value("CACHE",Moss_UserFolder::CACHE);
    py::enum_<Moss_CameraPermissionState>(m,"CameraPermissionState").value("DENIED",Moss_CameraPermissionState::DENIED).value("PENDING",Moss_CameraPermissionState::PENDING).value("APPROVED",Moss_CameraPermissionState::APPROVED);
    py::enum_<Moss_CameraPosition>(m,"CameraPosition").value("UNKNOWN",Moss_CameraPosition::UNKNOWN).value("FRONT_FACING",Moss_CameraPosition::FRONT_FACING).value("BACK_FACING",Moss_CameraPosition::BACK_FACING);
    py::enum_<AudioEffectType>(m,"AudioEffectType").value("LOWPASS",AudioEffectType::LOWPASS).value("HIGHPASS",AudioEffectType::HIGHPASS).value("ECHO",AudioEffectType::ECHO).value("FLANGE",AudioEffectType::FLANGE).value("DISTORTION",AudioEffectType::DISTORTION).value("NORMALIZE",AudioEffectType::NORMALIZE).value("PARAMEQ",AudioEffectType::PARAMEQ).value("PITCHSHIFTER",AudioEffectType::PITCHSHIFTER).value("CHORUS",AudioEffectType::CHORUS).value("COMPRESSOR",AudioEffectType::COMPRESSOR).value("REVERB",AudioEffectType::REVERB).value("DELAY",AudioEffectType::DELAY).value("DOPPLER",AudioEffectType::DOPPLER).value("PANNING",AudioEffectType::PANNING).value("DISTANCE_ATTENUATION",AudioEffectType::DISTANCE_ATTENUATION);
    py::enum_<DistanceModel>(m,"DistanceModel").value("LINEAR",DistanceModel::LINEAR).value("INVERSE",DistanceModel::INVERSE).value("EXPONENTIAL",DistanceModel::EXPONENTIAL);
    py::enum_<AudioLoadType>(m,"AudioLoadType").value("FULLY_LOADED",AudioLoadType::FULLY_LOADED).value("STREAMING",AudioLoadType::STREAMING);

    // -------------------------------------------------------------------------
    // Window / monitor / input / gamepads
    // -------------------------------------------------------------------------
    m.def("create_window", [](const std::string& title,int width,int height,std::uintptr_t monitor=0,std::uintptr_t share=0){return handle(Moss_CreateWindow(title.c_str(),width,height,ptr<Moss_Monitor>(monitor),ptr<Moss_Window>(share)));},py::arg("title"),py::arg("width"),py::arg("height"),py::arg("monitor")=0,py::arg("share")=0);
    m.def("terminate_window", [](std::uintptr_t h){Moss_TerminateWindow(ptr<Moss_Window>(h));});
    m.def("create_message_box", [](const std::string&a,const std::string&b,Moss_MessageBoxFlags f,std::uintptr_t w){return Moss_CreateMessageBox(a.c_str(),b.c_str(),f,ptr<Moss_Window>(w));},py::arg("title"),py::arg("message"),py::arg("flags"),py::arg("window")=0);
    m.def("should_window_close", [](std::uintptr_t h){return Moss_ShouldWindowClose(ptr<Moss_Window>(h));});
    m.def("poll_events", &Moss_PollEvents);
    m.def("get_window_width", &Moss_GetWindowWidth);
    m.def("get_window_height", &Moss_GetWindowHeight);
    m.def("set_window_title", [](std::uintptr_t h,const std::string&s){Moss_SetWindowTitle(ptr<Moss_Window>(h),s.c_str());});
    m.def("close_window", [](std::uintptr_t h){Moss_CloseWindow(ptr<Moss_Window>(h));});
    m.def("get_primary_monitor", []{return handle(Moss_MonitorGetPrimary());});
    m.def("get_secondary_monitor", []{return handle(Moss_MonitorGetSecondary());});
    m.def("monitor_position", [](std::uintptr_t h){int x=0,y=0;Moss_MonitorGetPosition(ptr<Moss_Monitor>(h),&x,&y);return py::make_tuple(x,y);});
    m.def("monitor_physical_size", [](std::uintptr_t h){int x=0,y=0;Moss_MonitorGetPhysicalSize(ptr<Moss_Monitor>(h),&x,&y);return py::make_tuple(x,y);});
    m.def("monitor_content_scale", [](std::uintptr_t h){float x=0,y=0;Moss_MonitorGetContentScale(ptr<Moss_Monitor>(h),&x,&y);return py::make_tuple(x,y);});
    m.def("monitor_name", [](std::uintptr_t h){const char*s=Moss_MonitorGetName(ptr<Moss_Monitor>(h));return s?s:"";});
    m.def("monitor_set_gamma", [](std::uintptr_t h,float g){Moss_MonitorSetGamma(ptr<Moss_Monitor>(h),g);});
    m.def("monitor_gamma_ramp", [](std::uintptr_t h){auto*p=Moss_MonitorGetGammaRamp(ptr<Moss_Monitor>(h));return handle(p);});
    m.def("is_key_pressed", &Moss_IsKeyPressed);
    m.def("is_key_released", &Moss_IsReleased);
    m.def("is_key_just_pressed", &Moss_IsKeyJustPressed);
    m.def("is_key_just_released", &Moss_IsKeyJustReleased);
    m.def("input_get_key", &Moss_InputGetKey);
    m.def("is_mouse_pressed", &Moss_IsMousePressed);
    m.def("is_mouse_released", &Moss_IsMouseReleased);
    m.def("is_mouse_just_pressed", &Moss_IsMouseJustPressed);
    m.def("is_mouse_just_released", &Moss_IsMouseJustReleased);
    m.def("input_get_mouse_button", &Moss_InputGetMouseButton);
    m.def("mouse_position", []{int x=0,y=0;Moss_GetMousePosition(&x,&y);return py::make_tuple(x,y);});
    m.def("set_mouse_position", &Moss_SetMousePosition);
    m.def("set_mouse_visible", &Moss_SetMouseVisible);
    m.def("num_gamepads", &Moss_GetNumGamepads);
    m.def("open_gamepad", [](Moss_GamepadID id){return handle(Moss_OpenGamepad(id));});
    m.def("close_gamepad", [](std::uintptr_t h){Moss_CloseGamepad(ptr<Moss_Gamepad>(h));});
    m.def("gamepad_connected", [](std::uintptr_t h){return Moss_GamepadConnected(ptr<Moss_Gamepad>(h));});
    m.def("update_gamepads", &Moss_UpdateGamepads);
    m.def("gamepad_button_pressed", [](std::uintptr_t h,Moss_GamepadButton b){return Moss_IsGamepadButtonPressed(ptr<Moss_Gamepad>(h),b);});
    m.def("gamepad_button_just_pressed", [](std::uintptr_t h,Moss_GamepadButton b){return Moss_IsGamepadButtonJustPressed(ptr<Moss_Gamepad>(h),b);});
    m.def("gamepad_button_just_released", [](std::uintptr_t h,Moss_GamepadButton b){return Moss_IsGamepadButtonJustReleased(ptr<Moss_Gamepad>(h),b);});
    m.def("gamepad_axis", [](std::uintptr_t h,GamepadAxis a){return Moss_GetGamepadAxis(ptr<Moss_Gamepad>(h),a);});
    m.def("set_gamepad_axis_deadzone", &Moss_SetGamepadAxisDeadzone);
    m.def("set_gamepad_axis_inverted", &Moss_SetGamepadAxisInverted);
    m.def("rumble_gamepad", [](std::uintptr_t h,uint16_t l,uint16_t r,uint32_t d){return Moss_RumbleGamepad(ptr<Moss_Gamepad>(h),l,r,d);});
    m.def("rumble_gamepad_triggers", [](std::uintptr_t h,uint16_t l,uint16_t r,uint32_t d){return Moss_RumbleGamepadTriggers(ptr<Moss_Gamepad>(h),l,r,d);});
    m.def("set_gamepad_led", [](std::uintptr_t h,uint8_t r,uint8_t g,uint8_t b){return Moss_SetGamepadLED(ptr<Moss_Gamepad>(h),r,g,b);});
    m.def("gamepad_name", [](std::uintptr_t h){const char*s=Moss_GetGamepadName(ptr<Moss_Gamepad>(h));return s?s:"";});
    m.def("gamepad_id", [](std::uintptr_t h){return Moss_GetGamepadID(ptr<Moss_Gamepad>(h));});
    m.def("gamepad_player_index", [](std::uintptr_t h){return Moss_GetGamepadPlayerIndex(ptr<Moss_Gamepad>(h));});
    m.def("gamepad_power_info", [](std::uintptr_t h){int p=0;auto s=Moss_GetGamepadPowerInfo(ptr<Moss_Gamepad>(h),&p);return py::make_tuple(s,p);});
    m.def("gamepad_num_touchpads", [](std::uintptr_t h){return Moss_GetNumGamepadTouchpads(ptr<Moss_Gamepad>(h));});
    m.def("gamepad_num_touchpad_fingers", [](std::uintptr_t h){return Moss_GetNumGamepadTouchpadFingers(ptr<Moss_Gamepad>(h));});
    m.def("gamepad_touchpad_finger", [](std::uintptr_t h,int pad,int finger){bool d=false;float x=0,y=0,p=0;bool ok=Moss_GetGamepadTouchpadFinger(ptr<Moss_Gamepad>(h),pad,finger,&d,&x,&y,&p);return py::make_tuple(ok,d,x,y,p);});
    m.def("gamepad_mapping", [](std::uintptr_t h){const char*s=Moss_GetGamepadMapping(ptr<Moss_Gamepad>(h));return s?s:"";});
    m.def("set_gamepad_mapping", [](std::uintptr_t h,const std::string&s){return Moss_SetGamepadMapping(ptr<Moss_Gamepad>(h),s.c_str());});
    m.def("reload_gamepad_mappings", &Moss_ReloadGamepadMappings);
    m.def("input_get_gamepad_button", &Moss_InputGetGamepadButton);
    m.def("input_get_gamepad_axis", &Moss_InputGetGamepadAxis);
    m.def("get_pen_device_type", &Moss_GetPenDeviceType);
    m.def("touch_device_name", [](Moss_TouchID id){const char*s=Moss_GetTouchDeviceName(id);return s?s:"";});
    m.def("touch_devices", []{int n=0;auto*p=Moss_GetTouchDevices(&n);py::list out;for(int i=0;i<n;i++)out.append(p[i]);return out;});
    m.def("touch_device_type", &Moss_GetTouchDeviceType);
    m.def("touch_fingers", [](Moss_TouchID id){int n=0;auto*p=Moss_GetTouchFingers(id,&n);py::list out;for(int i=0;i<n;i++)out.append(handle(p[i]));return out;});

    // Haptics.
    m.def("open_haptic", [](Moss_HapticID id){return handle(Moss_OpenHaptic(id));});
    m.def("close_haptic", [](std::uintptr_t h){Moss_CloseHaptic(ptr<Moss_Haptic>(h));});
    m.def("create_haptic_effect", [](std::uintptr_t h){return Moss_CreateHapticEffect(ptr<Moss_Haptic>(h));});
    m.def("destroy_haptic_effect", [](std::uintptr_t h){Moss_DestroyHapticEffect(ptr<Moss_Haptic>(h));});
    m.def("haptic_effect_status", [](std::uintptr_t h){return Moss_GetHapticEffectStatus(ptr<Moss_Haptic>(h));});
    m.def("haptic_features", [](std::uintptr_t h){return Moss_GetHapticFeatures(ptr<Moss_Haptic>(h));});
    m.def("haptic_from_id", [](std::uintptr_t h){return handle(Moss_GetHapticFromID(ptr<Moss_Haptic>(h)));});
    m.def("haptic_id_pointer", [](std::uintptr_t h){return handle(Moss_GetHapticID(ptr<Moss_Haptic>(h)));});
    m.def("haptic_name", [](std::uintptr_t h){const char*s=Moss_GetHapticName(ptr<Moss_Haptic>(h));return s?s:"";});
    m.def("haptic_name_for_id", [](std::uintptr_t h){const char*s=Moss_GetHapticNameForID(ptr<Moss_Haptic>(h));return s?s:"";});
    m.def("haptics_pointer", [](std::uintptr_t h){return handle(Moss_GetHaptics(ptr<Moss_Haptic>(h)));});
    m.def("max_haptic_effects", [](std::uintptr_t h){return Moss_GetMaxHapticEffects(ptr<Moss_Haptic>(h));});
    m.def("max_haptic_effects_playing", [](std::uintptr_t h){return Moss_GetMaxHapticEffectsPlaying(ptr<Moss_Haptic>(h));});
    m.def("num_haptic_axes", [](std::uintptr_t h){return Moss_GetNumHapticAxes(ptr<Moss_Haptic>(h));});
    m.def("haptic_effect_supported", [](std::uintptr_t h){return Moss_HapticEffectSupported(ptr<Moss_Haptic>(h));});
    m.def("haptic_rumble_supported", [](std::uintptr_t h){return Moss_HapticRumbleSupported(ptr<Moss_Haptic>(h));});
    m.def("init_haptic_rumble", [](std::uintptr_t h){return Moss_InitHapticRumble(ptr<Moss_Haptic>(h));});
    m.def("is_joystick_haptic", [](std::uintptr_t h){return Moss_IsJoystickHaptic(ptr<Moss_GamepadAxis>(h));});
    m.def("is_mouse_haptic", &Moss_IsMouseHaptic);
    m.def("open_haptic_from_joystick", [](std::uintptr_t h){return handle(Moss_OpenHapticFromJoystick(ptr<Moss_GamepadAxis>(h)));});
    m.def("open_haptic_from_mouse", []{return handle(Moss_OpenHapticFromMouse());});
    m.def("pause_haptic", [](std::uintptr_t h){return Moss_PauseHaptic(ptr<Moss_Haptic>(h));});
    m.def("play_haptic_rumble", [](std::uintptr_t h,float s,uint32_t l){return Moss_PlayHapticRumble(ptr<Moss_Haptic>(h),s,l);});
    m.def("resume_haptic", [](std::uintptr_t h){return Moss_ResumeHaptic(ptr<Moss_Haptic>(h));});
    m.def("run_haptic_effect", [](std::uintptr_t h,uint32_t i){return Moss_RunHapticEffect(ptr<Moss_Haptic>(h),i);});
    m.def("set_haptic_autocenter", [](std::uintptr_t h,int c){return Moss_SetHapticAutocenter(ptr<Moss_Haptic>(h),c);});
    m.def("set_haptic_gain", [](std::uintptr_t h,int g){return Moss_SetHapticGain(ptr<Moss_Haptic>(h),g);});
    m.def("stop_haptic_effect", [](std::uintptr_t h,Moss_HapticEffectID id){return Moss_StopHapticEffect(ptr<Moss_Haptic>(h),id);});
    m.def("stop_haptic_effects", [](std::uintptr_t h){return Moss_StopHapticEffects(ptr<Moss_Haptic>(h));});
    m.def("stop_haptic_rumble", [](std::uintptr_t h){return Moss_StopHapticRumble(ptr<Moss_Haptic>(h));});
    m.def("update_haptic_effect", [](std::uintptr_t h,Moss_HapticEffectID id,py::bytes raw){std::string s=raw; if(s.size()!=sizeof(Moss_HapticEffect)) throw py::value_error("effect buffer must be sizeof(Moss_HapticEffect) bytes"); Moss_HapticEffect e{}; std::memcpy(&e,s.data(),sizeof(e)); return Moss_UpdateHapticEffect(ptr<Moss_Haptic>(h),id,&e);});

    // System, process and dynamic libraries.
    m.def("cpu_count", &Moss_GetAvailableCPUCores);
    m.def("cpu_cache_line_size", &Moss_GetCPUCacheLineSize);
    m.def("system_ram", &Moss_GetSystemRAM);
    m.def("open_url", [](const std::string&s){return Moss_OpenURL(s.c_str());});
    m.def("locale", []{auto*p=Moss_GetLocale();if(!p)return py::none();py::dict d;d["country"]=p->country?p->country:"";d["language"]=p->language?p->language:"";return py::object(d);});
    m.def("power_info", []{int sec=0,pct=0;auto s=Moss_GetPowerInfo(&sec,&pct);return py::make_tuple(s,sec,pct);});
    m.def("is_process_running", [](const std::string&s){return Moss_IsProcessRunningByName(s.c_str());});
    m.def("load_dynamic_library", [](const std::string&s){return reinterpret_cast<std::uintptr_t>(Moss_LoadDynamicLibrary(s.c_str()));});
    m.def("get_library_symbol", [](std::uintptr_t h,const std::string&s){return reinterpret_cast<std::uintptr_t>(Moss_GetLibrarySymbol(reinterpret_cast<void*>(h),s.c_str()));});
    m.def("unload_dynamic_library", [](std::uintptr_t h){Moss_UnloadDynamicLibrary(reinterpret_cast<void*>(h));});

    // Callbacks.
    m.def("set_framebuffer_resize_callback", [](py::function f){set_cb(g_framebuffer_cb,std::move(f));Moss_SetFramebufferResizeCallback(framebuffer_cb);});
    m.def("set_window_size_callback", [](py::function f){set_cb(g_window_size_cb,std::move(f));Moss_SetWindowSizeCallback(window_size_cb);});
    m.def("set_window_resize_callback", [](py::function f){set_cb(g_window_resize_cb,std::move(f));Moss_SetWindowResizeCallback(window_resize_cb);});
    m.def("set_window_position_callback", [](py::function f){set_cb(g_window_position_cb,std::move(f));Moss_SetWindowPositionCallback(window_position_cb);});
    m.def("set_window_focus_callback", [](py::function f){set_cb(g_window_focus_cb,std::move(f));Moss_SetWindowFocusCallback(window_focus_cb);});
    m.def("set_window_content_scale_callback", [](py::function f){set_cb(g_window_scale_cb,std::move(f));Moss_SetWindowContentScaleCallback(window_scale_cb);});
    m.def("set_monitor_callback", [](py::function f){set_cb(g_monitor_cb,std::move(f));Moss_SetMonitorCallback(monitor_cb);});

    // Cameras / capture.
    m.def("get_cameras", []{int n=0;auto*p=Moss_GetCameras(&n);py::list out;for(int i=0;i<n;i++)out.append(p[i]);return out;});
    m.def("camera_name", [](Moss_CameraID id){const char*s=Moss_GetCameraName(id);return s?s:"";});
    m.def("camera_position", &Moss_GetCameraPosition);
    m.def("current_camera_driver", []{const char*s=Moss_GetCurrentCameraDriver();return s?s:"";});
    m.def("num_camera_drivers", &Moss_GetNumCameraDrivers);
    m.def("camera_supported_formats", [](Moss_CameraID id){int n=0;auto*p=Moss_GetCameraSupportedFormats(id,&n);py::list out;for(int i=0;i<n;i++)out.append(camera_spec_dict(p[i]));return out;});
    m.def("acquire_camera_frame", [](std::uintptr_t h){uint64_t ts=0;return py::make_tuple(handle(Moss_AcquireCameraFrame(ptr<Moss_Capture>(h),&ts)),ts);});
    m.def("release_camera_frame", [](std::uintptr_t h,std::uintptr_t f){Moss_ReleaseCameraFrame(ptr<Moss_Capture>(h),ptr<Moss_Surface>(f));});
    m.def("camera_format", [](std::uintptr_t h){Moss_CameraSpec s{}; if(!Moss_GetCameraFormat(ptr<Moss_Capture>(h),&s)) return py::object(py::none()); return py::object(camera_spec_dict(s));});
    m.def("camera_permission_state", [](std::uintptr_t h){return Moss_GetCameraPermissionState(ptr<Moss_Capture>(h));});
    m.def("camera_properties", [](std::uintptr_t h){return Moss_GetCameraProperties(ptr<Moss_Capture>(h));});
    m.def("close_camera", [](std::uintptr_t h){Moss_CloseCamera(ptr<Moss_Capture>(h));});
    m.def("camera_id", [](std::uintptr_t h){return Moss_GetCameraID(ptr<Moss_Capture>(h));});
    m.def("open_capture", [](Moss_CameraID id,py::dict d){Moss_CameraSpec s{};s.format=static_cast<PixelFormat>(d["format"].cast<int>());s.colorspace=static_cast<Colorspace>(d["colorspace"].cast<int>());s.width=d["width"].cast<int>();s.height=d["height"].cast<int>();s.framerate_numerator=d["framerate_numerator"].cast<int>();s.framerate_denominator=d["framerate_denominator"].cast<int>();return handle(Moss_OpenCapture(id,&s));});

    // Filesystem and storage.
    m.def("copy_file", [](const std::string&a,const std::string&b,bool o){return Moss_CopyFile(a.c_str(),b.c_str(),o);});
    m.def("create_directory", [](const std::string&p,bool r){return Moss_CreateDirectory(p.c_str(),r);});
    m.def("remove_path", [](const std::string&p,bool r){return Moss_RemovePath(p.c_str(),r);});
    m.def("rename_path", [](const std::string&a,const std::string&b,bool o){return Moss_RenamePath(a.c_str(),b.c_str(),o);});
    m.def("path_info", [](const std::string&p){Moss_PathInfo x{};if(!Moss_GetPathInfo(p.c_str(),&x))return py::object(py::none());return py::object(path_info_dict(&x));});
    m.def("current_directory", []{char b[4096]{};return Moss_GetCurrentDirectory(b,sizeof(b))?std::string(b):std::string();});
    m.def("base_path", []{char b[4096]{};return Moss_GetBasePath(b,sizeof(b))?std::string(b):std::string();});
    m.def("user_folder", [](Moss_UserFolder f){char b[4096]{};return Moss_GetUserFolder(f,b,sizeof(b))?std::string(b):std::string();});
    m.def("pref_path", [](const std::string&o,const std::string&a){char b[4096]{};return Moss_GetPrefPath(o.c_str(),a.c_str(),b,sizeof(b))?std::string(b):std::string();});
    m.def("open_file_storage", [](const std::string&p){return handle(Moss_OpenFileStorage(p.c_str()));});
    m.def("open_title_storage", [](const std::string&p,uint32_t props){return handle(Moss_OpenTitleStorage(p.c_str(),props));});
    m.def("open_user_storage", [](const std::string&o,const std::string&a,uint32_t props){return handle(Moss_OpenUserStorage(o.c_str(),a.c_str(),props));});
    m.def("storage_ready", [](std::uintptr_t h){return Moss_StorageReady(ptr<Moss_Storage>(h));});
    m.def("close_storage", [](std::uintptr_t h){return Moss_CloseStorage(ptr<Moss_Storage>(h));});
    m.def("create_storage_directory", [](std::uintptr_t h,const std::string&p){return Moss_CreateStorageDirectory(ptr<Moss_Storage>(h),p.c_str());});
    m.def("copy_storage_file", [](std::uintptr_t h,const std::string&a,const std::string&b){return Moss_CopyStorageFile(ptr<Moss_Storage>(h),a.c_str(),b.c_str());});
    m.def("remove_storage_path", [](std::uintptr_t h,const std::string&p){return Moss_RemoveStoragePath(ptr<Moss_Storage>(h),p.c_str());});
    m.def("rename_storage_path", [](std::uintptr_t h,const std::string&a,const std::string&b){return Moss_RenameStoragePath(ptr<Moss_Storage>(h),a.c_str(),b.c_str());});
    m.def("storage_file_size", [](std::uintptr_t h,const std::string&p){uint64 n=0;return py::make_tuple(Moss_GetStorageFileSize(ptr<Moss_Storage>(h),p.c_str(),&n),n);});
    m.def("storage_space_remaining", [](std::uintptr_t h){return Moss_GetStorageSpaceRemaining(ptr<Moss_Storage>(h));});
    m.def("storage_path_info", [](std::uintptr_t h,const std::string&p){Moss_PathInfo x{};if(!Moss_GetStoragePathInfo(ptr<Moss_Storage>(h),p.c_str(),&x))return py::object(py::none());return py::object(path_info_dict(&x));});
    m.def("read_storage_file", [](std::uintptr_t h,const std::string&p,uint64_t len){std::string b(len,'\\0'); if(!Moss_ReadStorageFile(ptr<Moss_Storage>(h),p.c_str(),b.data(),len))return py::bytes(); return py::bytes(b);});
    m.def("write_storage_file", [](std::uintptr_t h,const std::string&p,py::bytes src){std::string b=src;return Moss_WriteStorageFile(ptr<Moss_Storage>(h),p.c_str(),b.data(),b.size());});

    // -------------------------------------------------------------------------
    // Audio
    // -------------------------------------------------------------------------
    m.def("init_audio", &Moss_Init_Audio);
    m.def("terminate_audio", &Moss_Terminate_Audio);
    m.def("audio_update", &Moss_AudioUpdate);
    m.def("audio_load_wav", [](const std::string&p){return handle(Moss_AudioLoadWavFile(p.c_str()));});
    m.def("audio_load_wav_legacy", []{return handle(Moss_AudioLoadWav());});
    m.def("audio_load_ogg", [](const std::string&p,AudioLoadType t){return handle(Moss_AudioLoadOgg(p.c_str(),t));});
    m.def("audio_load_mp3", [](const std::string&p){return handle(Moss_AudioLoadMP3(p.c_str()));});
    m.def("audio_source_destroy", [](std::uintptr_t h){Moss_AudioSourceDestroy(ptr<Moss_AudioSource>(h));});
    m.def("audio_capture_microphone", [](std::uintptr_t h){return handle(Moss_AudioCaptureMicrophone(ptr<Moss_Microphone>(h)));});
    // The AudioEffect-by-value API is present in the current Moss header but AudioEffect
    // is only forward-declared there, so it cannot be safely materialized by this TU.
    m.def("audio_remove_effect", [](std::uintptr_t h){Moss_AudioRemoveEffect(ptr<AudioEffect>(h));});
    m.def("audio_create_channel", [](ChannelID c){return Moss_AudioCreateChannel(c);});
    m.def("audio_remove_channel", [](ChannelID c){Audio_RemoveChannel(c);});
    m.def("audio_master_channel", &Moss_AudioGetMasterChannel);
    m.def("audio_set_channel_volume", &Moss_AudioSetChannelVolume);
    m.def("audio_set_channel_mute", &Moss_AudioSetChannelMute);
    m.def("audio_add_channel_effect", [](ChannelID c,std::uintptr_t e){Moss_AudioAddChannelEffect(c,ptr<AudioEffect>(e));});
    m.def("audio_remove_channel_effect", [](ChannelID c,std::uintptr_t e){Moss_AudioRemoveChannelEffect(c,ptr<AudioEffect>(e));});
    m.def("audio_remove_all_channel_effects", &Moss_AudioRemoveAllChannelEffects);
    m.def("audio_stream_create", []{return handle(Moss_AudioStreamCreate());});
    m.def("audio_stream_play", [](std::uintptr_t h){Moss_AudioStreamPlay(ptr<AudioStream>(h));});
    m.def("audio_stream_stop", [](std::uintptr_t h){Moss_AudioStreamStop(ptr<AudioStream>(h));});
    m.def("audio_stream_set_volume", [](std::uintptr_t h,float v){Moss_AudioStreamSetVolume(ptr<AudioStream>(h),v);});
    m.def("audio_stream_set_pitch", [](std::uintptr_t h,float v){Moss_AudioStreamSetPitch(ptr<AudioStream>(h),v);});
    m.def("audio_stream_set_playback_rate", [](std::uintptr_t h,float v){Moss_AudioStreamSetPlaybackRate(ptr<AudioStream>(h),v);});
    m.def("audio_stream_set_pan", [](std::uintptr_t h,float v){Moss_AudioStreamSetPan(ptr<AudioStream>(h),v);});
    m.def("audio_stream_set_loop", [](std::uintptr_t h,bool v){Moss_AudioStreamSetLoop(ptr<AudioStream>(h),v);});
    m.def("audio_stream_remove", [](std::uintptr_t h){Moss_AudioStreamRemove(ptr<AudioStream>(h));});
    m.def("audio_stream2d_create", []{return handle(Moss_AudioStream2DCreate());});
    m.def("audio_stream2d_play", [](std::uintptr_t h){Moss_AudioStream2DPlay(ptr<AudioStream2D>(h));});
    m.def("audio_stream2d_stop", [](std::uintptr_t h){Moss_AudioStream2DStop(ptr<AudioStream2D>(h));});
    m.def("audio_stream2d_set_volume", [](std::uintptr_t h,float v){Moss_AudioStream2DSetVolume(ptr<AudioStream2D>(h),v);});
    m.def("audio_stream2d_set_pitch", [](std::uintptr_t h,float v){Moss_AudioStream2DSetPitch(ptr<AudioStream2D>(h),v);});
    m.def("audio_stream2d_set_playback_rate", [](std::uintptr_t h,float v){Moss_AudioStream2DSetPlaybackRate(ptr<AudioStream2D>(h),v);});
    m.def("audio_stream2d_set_pan", [](std::uintptr_t h,float v){Moss_AudioStream2DSetPan(ptr<AudioStream2D>(h),v);});
    m.def("audio_stream2d_set_loop", [](std::uintptr_t h,bool v){Moss_AudioStream2DSetLoop(ptr<AudioStream2D>(h),v);});
    m.def("audio_stream3d_position_legacy", [](std::uintptr_t h){Moss_AudioStream3DSetPosition(ptr<AudioStream2D>(h));});
    m.def("audio_stream3d_velocity_legacy", [](std::uintptr_t h){Moss_AudioStream3DSetVelocity(ptr<AudioStream2D>(h));});
    m.def("audio_stream3d_max_distance_legacy", [](std::uintptr_t h){Moss_AudioStream3DSetMaxDistance(ptr<AudioStream2D>(h));});
    m.def("audio_stream2d_remove", [](std::uintptr_t h){Moss_AudioStream2DRemove(ptr<AudioStream2D>(h));});
    m.def("audio_stream3d_create", []{return handle(Moss_AudioStream3DCreate());});
    m.def("audio_stream3d_play", [](std::uintptr_t h){Moss_AudioStream3DPlay(ptr<AudioStream3D>(h));});
    m.def("audio_stream3d_stop", [](std::uintptr_t h){Moss_AudioStream3DStop(ptr<AudioStream3D>(h));});
    m.def("audio_stream3d_set_volume", [](std::uintptr_t h,float v){Moss_AudioStream3DSetVolume(ptr<AudioStream3D>(h),v);});
    m.def("audio_stream3d_set_pitch", [](std::uintptr_t h,float v){Moss_AudioStream3DSetPitch(ptr<AudioStream3D>(h),v);});
    m.def("audio_stream3d_set_playback_rate", [](std::uintptr_t h,float v){Moss_AudioStream3DSetPlaybackRate(ptr<AudioStream3D>(h),v);});
    m.def("audio_stream3d_set_pan", [](std::uintptr_t h,float v){Moss_AudioStream3DSetPan(ptr<AudioStream3D>(h),v);});
    m.def("audio_stream3d_set_loop", [](std::uintptr_t h,bool v){Moss_AudioStream3DSetLoop(ptr<AudioStream3D>(h),v);});
    m.def("audio_stream3d_set_position_legacy", [](std::uintptr_t h){Moss_AudioStream3DSetPosition(ptr<AudioStream3D>(h));});
    m.def("audio_stream3d_set_velocity_legacy", [](std::uintptr_t h){Moss_AudioStream3DSetVelocity(ptr<AudioStream3D>(h));});
    m.def("audio_stream3d_set_max_distance_legacy", [](std::uintptr_t h){Moss_AudioStream3DSetMaxDistance(ptr<AudioStream3D>(h));});
    m.def("audio_stream3d_set_distance_model", [](std::uintptr_t h,DistanceModel d){Moss_AudioStream3DSetDistanceModel(ptr<AudioStream3D>(h),d);});
    m.def("audio_stream3d_remove", [](std::uintptr_t h){Moss_AudioStream3DRemove(ptr<AudioStream3D>(h));});

    m.def("audio_listener2d_create", [](py::tuple p){Float2 v{p[0].cast<float>(),p[1].cast<float>()};return handle(Moss_AudioCreateAudioListener2D(v));});
    m.def("audio_listener3d_create", [](py::tuple p){Float3 v{p[0].cast<float>(),p[1].cast<float>(),p[2].cast<float>()};return handle(Moss_AudioCreateAudioListener3D(v));});
    m.def("ray_audio_listener2d_create", [](py::tuple p){Float2 v{p[0].cast<float>(),p[1].cast<float>()};return handle(Moss_AudioCreateRayAudioListener2D(v));});
    m.def("ray_audio_listener3d_create", [](py::tuple p){Float3 v{p[0].cast<float>(),p[1].cast<float>(),p[2].cast<float>()};return handle(Moss_AudioCreateRayAudioListener3D(v));});
    m.def("audio_listener2d_remove", [](std::uintptr_t h){Moss_AudioRemoveAudioListener2D(ptr<AudioListener2D>(h));});
    m.def("audio_listener3d_remove", [](std::uintptr_t h){Moss_AudioRemoveAudioListener3D(ptr<AudioListener3D>(h));});
    m.def("ray_listener2d_remove", [](std::uintptr_t h){Moss_AudioRemoveRayAudioListener2D(ptr<RayAudioListener2D>(h));});
    m.def("ray_listener3d_remove", [](std::uintptr_t h){Moss_AudioRemoveRayAudioListener3D(ptr<RayAudioListener3D>(h));});
    m.def("audio_listener2d_activate", [](std::uintptr_t h,bool a){Moss_AudioActivateAudioListener2D(ptr<AudioListener2D>(h),a);});
    m.def("audio_listener3d_activate", [](std::uintptr_t h,bool a){Moss_AudioActivateAudioListener3D(ptr<AudioListener3D>(h),a);});
    m.def("ray_listener2d_activate", [](std::uintptr_t h,bool a){Moss_AudioActivateRayAudioListener2D(ptr<RayAudioListener2D>(h),a);});
    m.def("ray_listener3d_activate", [](std::uintptr_t h,bool a){Moss_AudioActivateRayAudioListener3D(ptr<RayAudioListener3D>(h),a);});
    m.def("audio_listener3d_set_orientation", [](std::uintptr_t h,py::tuple f,py::tuple u){Vec3 fv(f[0].cast<float>(),f[1].cast<float>(),f[2].cast<float>());Vec3 uv(u[0].cast<float>(),u[1].cast<float>(),u[2].cast<float>());Moss_AudioListenerSetOrientation(ptr<AudioListener3D>(h),fv,uv);});
    m.def("ray_trace_gain", [](py::dict r){Moss_AudioRayTraceResult x{};x.audible=r["audible"].cast<bool>();x.occluded=r["occluded"].cast<bool>();x.distance=r["distance"].cast<float>();x.attenuation=r["attenuation"].cast<float>();x.occlusion=r["occlusion"].cast<float>();x.transmission_gain=r["transmission_gain"].cast<float>();x.lowpass=r["lowpass"].cast<float>();x.reflection_gain=r["reflection_gain"].cast<float>();x.reflection_delay_seconds=r["reflection_delay_seconds"].cast<float>();x.delay_seconds=r["delay_seconds"].cast<float>();return Moss_AudioRayTraceComputeGain(&x);});
    m.def("ray_trace2d", [](py::tuple lp,py::tuple sp,float max_distance,uint32_t rays,float occ,float refl,float air){Moss_AudioRayTrace2DDesc d{};d.listener_position=Vec2(lp[0].cast<float>(),lp[1].cast<float>());d.source_position=Vec2(sp[0].cast<float>(),sp[1].cast<float>());d.max_distance=max_distance;d.reflection_rays=rays;d.direct_occlusion_strength=occ;d.reflection_strength=refl;d.air_absorption=air;Moss_AudioRayTraceResult r{};if(!Moss_AudioRayTrace2D(&d,&r))return py::object(py::none());return py::object(ray_result_dict(r));},py::arg("listener_position"),py::arg("source_position"),py::arg("max_distance")=100.0f,py::arg("reflection_rays")=0,py::arg("direct_occlusion_strength")=1.0f,py::arg("reflection_strength")=.35f,py::arg("air_absorption")=.02f);
    m.def("ray_trace3d", [](py::tuple lp,py::tuple sp,float max_distance,uint32_t rays,float occ,float refl,float air){Moss_AudioRayTrace3DDesc d{};d.listener_position=Vec3(lp[0].cast<float>(),lp[1].cast<float>(),lp[2].cast<float>());d.source_position=Vec3(sp[0].cast<float>(),sp[1].cast<float>(),sp[2].cast<float>());d.max_distance=max_distance;d.reflection_rays=rays;d.direct_occlusion_strength=occ;d.reflection_strength=refl;d.air_absorption=air;Moss_AudioRayTraceResult r{};if(!Moss_AudioRayTrace3D(&d,&r))return py::object(py::none());return py::object(ray_result_dict(r));},py::arg("listener_position"),py::arg("source_position"),py::arg("max_distance")=100.0f,py::arg("reflection_rays")=0,py::arg("direct_occlusion_strength")=1.0f,py::arg("reflection_strength")=.35f,py::arg("air_absorption")=.02f);
    m.def("ray_trace_from_listener2d", [](std::uintptr_t h,py::tuple sp){Vec2 v(sp[0].cast<float>(),sp[1].cast<float>());Moss_AudioRayTraceResult r{};if(!Moss_AudioRayTraceFromListener2D(ptr<RayAudioListener2D>(h),&v,&r))return py::object(py::none());return py::object(ray_result_dict(r));});
    m.def("ray_trace_from_listener3d", [](std::uintptr_t h,py::tuple sp){Vec3 v(sp[0].cast<float>(),sp[1].cast<float>(),sp[2].cast<float>());Moss_AudioRayTraceResult r{};if(!Moss_AudioRayTraceFromListener3D(ptr<RayAudioListener3D>(h),&v,&r))return py::object(py::none());return py::object(ray_result_dict(r));});

    // Speaker / microphone.
    m.def("speaker_ready", &Moss_IsSpeakerDeviceReady);
    m.def("speaker_open", &Moss_AudioSpeakerOpen);
    m.def("speaker_pause", &Moss_AudioSpeakerPause);
    m.def("speaker_resume", &Moss_AudioSpeakerResume);
    m.def("speaker_is_paused", &Moss_AudioSpeakerIsPaused);
    m.def("select_speaker_device", &Moss_AudioSelectSpeakerDevice);
    m.def("current_speaker_device", &Moss_GetCurrentSpeakerDeviceID);
    m.def("speaker_device_name", [](int i){const char*s=Moss_GetSpeakerDeviceName(i);return s?s:"";});
    m.def("list_speaker_devices", &Moss_ListSpeakerDevices);
    m.def("microphone_device_count", &Moss_MicrophoneGetDeviceCount);
    m.def("microphone_device_name", [](uint32_t i){const char*s=Moss_MicrophoneGetDeviceName(i);return s?s:"";});
    m.def("microphone_open", [](py::dict x){Moss_MicrophoneDesc d{};if(x.contains("device_index"))d.device_index=x["device_index"].cast<uint32_t>();if(x.contains("sample_rate"))d.sample_rate=x["sample_rate"].cast<uint32_t>();if(x.contains("channels"))d.channels=x["channels"].cast<uint32_t>();if(x.contains("buffer_frames"))d.buffer_frames=x["buffer_frames"].cast<uint32_t>();if(x.contains("ring_buffer_frames"))d.ring_buffer_frames=x["ring_buffer_frames"].cast<uint32_t>();if(x.contains("start_immediately"))d.start_immediately=x["start_immediately"].cast<bool>();if(x.contains("enable_voice_metrics"))d.enable_voice_metrics=x["enable_voice_metrics"].cast<bool>();return handle(Moss_MicrophoneOpen(&d));});
    m.def("microphone_close", [](std::uintptr_t h){Moss_MicrophoneClose(ptr<Moss_Microphone>(h));});
    m.def("microphone_start", [](std::uintptr_t h){return Moss_MicrophoneStart(ptr<Moss_Microphone>(h));});
    m.def("microphone_stop", [](std::uintptr_t h){Moss_MicrophoneStop(ptr<Moss_Microphone>(h));});
    m.def("microphone_read", [](std::uintptr_t h,uint32_t max_frames){std::vector<float> out(max_frames);auto n=Moss_MicrophoneRead(ptr<Moss_Microphone>(h),out.data(),max_frames);out.resize(n);return out;});
    m.def("microphone_set_gain", [](std::uintptr_t h,float g){Moss_MicrophoneSetGain(ptr<Moss_Microphone>(h),g);});
    m.def("microphone_sample_rate", [](std::uintptr_t h){return Moss_MicrophoneGetSampleRate(ptr<Moss_Microphone>(h));});
    m.def("microphone_channels", [](std::uintptr_t h){return Moss_MicrophoneGetChannels(ptr<Moss_Microphone>(h));});
    m.def("microphone_levels", [](std::uintptr_t h){return microphone_levels_dict(Moss_MicrophoneGetLevels(ptr<Moss_Microphone>(h)));});
    m.def("microphone_rms", [](std::uintptr_t h){return Moss_MicrophoneGetLevelRMS(ptr<Moss_Microphone>(h));});
    m.def("microphone_peak", [](std::uintptr_t h){return Moss_MicrophoneGetLevelPeak(ptr<Moss_Microphone>(h));});
    m.def("microphone_smoothed_volume", [](std::uintptr_t h){return Moss_MicrophoneGetSmoothedVolume(ptr<Moss_Microphone>(h));});
    m.def("microphone_voice_activity", [](std::uintptr_t h){return Moss_MicrophoneGetVoiceActivity(ptr<Moss_Microphone>(h));});
    m.def("microphone_ready", &Moss_IsMicrophoneDeviceReady);
    m.def("audio_microphone_open", &Moss_AudioMicrophoneOpen);
    m.def("audio_microphone_close", &Moss_AudioMicrophoneClose);
    m.def("audio_microphone_play", &Moss_AudioMicrophonePlay);
    m.def("audio_microphone_stop", &Moss_AudioMicrophoneStop);
    m.def("audio_microphone_id", &Moss_AudioMicrophoneID);
    m.def("select_microphone_device", &Moss_AudioSelectMicrophoneDevice);
    m.def("microphone_name", [](int i){const char*s=Moss_GetMicrophoneDeviceName(i);return s?s:"";});
    m.def("list_microphone_devices", &Moss_ListMicrophoneDevices);
    m.def("audio_microphone_set_gain", [](std::uintptr_t h,float g){Moss_AudioMicrophoneSetGain(ptr<Moss_Microphone>(h),g);});
    m.def("audio_microphone_sample_rate", [](std::uintptr_t h){return Moss_AudioMicrophoneGetSampleRate(ptr<Moss_Microphone>(h));});
    m.def("audio_microphone_channels", [](std::uintptr_t h){return Moss_AudioMicrophoneGetChannels(ptr<Moss_Microphone>(h));});
    m.def("audio_stream_set_callback", [](std::uintptr_t, py::function){ throw py::not_implemented_error("AudioStreamCallback trampoline should be added in the runtime ABI layer."); });
    m.def("microphone_set_callback", [](std::uintptr_t, py::function){ throw py::not_implemented_error("MicrophoneCallback trampoline should be added in the runtime ABI layer."); });
}
