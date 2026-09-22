#include <stdexcept>
#include <string>

#include <pybind11/pybind11.h>
#include <Moss/Moss_Platform.h>

namespace py = pybind11;

namespace {
class Window {
public:
    Window(const std::string &title, int width, int height) {
        if (width <= 0 || height <= 0) throw py::value_error("width and height must be positive");
        window_ = Moss_CreateWindow(title.c_str(), width, height, nullptr, nullptr);
        if (window_ == nullptr) throw std::runtime_error("Moss_CreateWindow failed");
    }
    Window(const Window &) = delete;
    Window &operator=(const Window &) = delete;
    ~Window() { close(); }

    bool should_close() const { return window_ == nullptr || Moss_ShouldWindowClose(window_); }
    void close() { if (window_ != nullptr) { Moss_TerminateWindow(window_); window_ = nullptr; } }
    void set_title(const std::string &title) { require_open(); Moss_SetWindowTitle(window_, title.c_str()); }
    void set_always_on_top(bool enabled) { require_open(); if (!Moss_SetWindowAlwaysOnTop(window_, enabled)) throw std::runtime_error("Moss could not change the always-on-top state"); }
    void set_borderless(bool enabled) { require_open(); if (!Moss_SetWindowBorderless(window_, enabled)) throw std::runtime_error("Moss could not change the borderless state"); }
private:
    void require_open() const { if (window_ == nullptr) throw std::runtime_error("the Moss window is closed"); }
    Moss_Window *window_ = nullptr;
};

py::tuple monitor_position(Moss_Monitor *monitor) { int x = 0, y = 0; Moss_MonitorGetPosition(monitor, &x, &y); return py::make_tuple(x, y); }
py::tuple monitor_physical_size(Moss_Monitor *monitor) { int width = 0, height = 0; Moss_MonitorGetPhysicalSize(monitor, &width, &height); return py::make_tuple(width, height); }
py::tuple monitor_content_scale(Moss_Monitor *monitor) { float x = 0, y = 0; Moss_MonitorGetContentScale(monitor, &x, &y); return py::make_tuple(x, y); }
} // namespace

PYBIND11_MODULE(_pymoss, m) {
    m.doc() = "Native Python bindings for the Moss game framework";
    py::class_<Window>(m, "Window")
        .def(py::init<const std::string &, int, int>(), py::arg("title"), py::arg("width"), py::arg("height"))
        .def("should_close", &Window::should_close)
        .def("close", &Window::close)
        .def("set_title", &Window::set_title)
        .def("set_always_on_top", &Window::set_always_on_top)
        .def("set_borderless", &Window::set_borderless)
        .def("__enter__", [](Window &self) -> Window & { return self; }, py::return_value_policy::reference_internal)
        .def("__exit__", [](Window &self, py::object, py::object, py::object) { self.close(); });

    m.def("poll_events", &Moss_PollEvents, "Process pending platform events.");
    m.def("window_size", [] { return py::make_tuple(Moss_GetWindowWidth(), Moss_GetWindowHeight()); });
    m.def("mouse_position", [] { int x = 0, y = 0; Moss_GetMousePosition(&x, &y); return py::make_tuple(x, y); });
    m.def("set_mouse_position", &Moss_SetMousePosition, py::arg("x"), py::arg("y"));
    m.def("primary_monitor_name", []() -> py::object { auto *monitor = Moss_MonitorGetPrimary(); if (!monitor) return py::none(); const char *name = Moss_MonitorGetName(monitor); return name ? py::cast(name) : py::none(); });
    m.def("primary_monitor_position", []() -> py::object { auto *monitor = Moss_MonitorGetPrimary(); return monitor ? py::cast(monitor_position(monitor)) : py::none(); });
    m.def("primary_monitor_physical_size", []() -> py::object { auto *monitor = Moss_MonitorGetPrimary(); return monitor ? py::cast(monitor_physical_size(monitor)) : py::none(); });
    m.def("primary_monitor_content_scale", []() -> py::object { auto *monitor = Moss_MonitorGetPrimary(); return monitor ? py::cast(monitor_content_scale(monitor)) : py::none(); });
}