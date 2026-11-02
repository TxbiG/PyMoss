# PyMoss API Reference

> Python binding reference for PyMoss. This document distinguishes the package-level exports from additional methods registered by the native `_pymoss` extension. Package metadata inspected: PyMoss 0.1.0, Python >=3.10. **Audit snapshot: 2 November 2026.**

## 1. Overview and installation/build

PyMoss uses pybind11 to expose selected Moss APIs to Python. The package metadata declares:

- Distribution name: `PyMoss`.
- Version: `0.1.0`.
- Python requirement: `>=3.10`.
- Native extension module: `pymoss._pymoss`.

Build and installation commands depend on the repository's native Moss and pybind11 configuration. Use the repository build instructions for the exact CMake options and native library path; do not assume a pure-Python wheel contains every native backend.

## 2. Package-level public API (`import pymoss`)

The inspected `python/pymoss/__init__.py` re-exports the following names from `._pymoss`:

| Name | Kind | Description |
|---|---|---|
| `Window` | Class | Platform window wrapper. |
| `poll_events` | Function | Poll platform events. |
| `primary_monitor_name` | Function | Get primary monitor name. |
| `primary_monitor_position` | Function | Get primary monitor position. |
| `primary_monitor_physical_size` | Function | Get primary monitor physical dimensions. |
| `primary_monitor_content_scale` | Function | Get primary monitor content scale. |
| `window_size` | Function | Get current window size. |
| `mouse_position` | Function | Get mouse position. |
| `set_mouse_position` | Function | Set mouse position. |

These are the names listed in the package root `__all__`. This is the safest import surface to start with.

### `Window`

Verified constructor:

```python
Window(title: str, width: int, height: int)
```

The constructor rejects non-positive dimensions and raises when native window creation fails. Verified methods:

| Method | Description |
|---|---|
| `should_close()` | Query whether the window should close. |
| `is_open()` | Query whether the window is open. |
| `close()` | Close/release the window. |
| `set_title(title)` | Set the window title. |
| `native_handle()` | Retrieve the native handle value. |
| `__enter__()` / `__exit__(...)` | Context-manager support. |

Use the context manager to ensure deterministic close:

```python
import pymoss

with pymoss.Window("Moss window", 1280, 720) as window:
    while not window.should_close():
        pymoss.poll_events()
```

## 3. Monitor and gamepad classes in the extension

The native extension source also registers `Monitor` and `Gamepad` classes. These are not included in the inspected package root `__init__.py` re-export list. Use them through `pymoss._pymoss` only if they are present in the installed extension build.

### `Monitor` (extension-level)

The registered class exposes monitor validity, name, position, physical size, content scale and native-handle access. Verify the exact constructor/factory before instantiating it; the package-root helper functions may be preferable for primary-monitor information.

### `Gamepad` (extension-level)

The registered class exposes close/connection state, button pressed/just-pressed/just-released queries, axis values and gamepad name. The extension also registers `Keyboard`, `Mouse`, `GamepadAxis` and `GamepadButton` enums.

Do not assume these names are importable directly from `pymoss`; the root package does not re-export them in the inspected initializer.

## 4. Additional extension-level API

The pybind11 source registers additional functions directly on `_pymoss`. The inspected source includes the following API groups and representative names.

### Models and skeletal animation

- `model_load`, `model_load_obj`, `model_load_fbx`, `model_load_glb`.
- `model_remove`, `model_last_load_error`, `model_last_load_error_message`.
- `model_find_bone`, `model_find_morph_target`.
- `model_set_bone_transform`, `model_set_morph_weight`.
- `renderer_draw_model`, `renderer_draw_mesh`.

### Debug rendering

- `debug_draw_list_render`.

### GPU/device and command recording

- `gpu_device_driver`.
- `gpu_buffer_download` and texture calculation helpers.
- External feature mask/availability/name queries.
- Command-buffer acquire/cancel/submit operations.
- Command operations for render-pass begin/end, pipeline/resource-set binding, vertex/index-buffer binding, draw/draw-indexed, dispatch and clear.

The exact function argument types and resource ownership must be read from the binding definitions in the installed revision. GPU commands are not a simple stateless API; device, pipeline, resource-set, buffer and command-buffer lifetimes must be coordinated.

### XR

The inspected extension source registers XR groups for initialization/shutdown, validation layers, capabilities, session create/destroy/state, frame begin/end, predicted display time/delta, event handling, view count/view retrieval, swapchain create/destroy/acquire/release, anchors and actions.

### Physics

The extension includes physics configuration and broadphase/collision-layer functions, including `physics_broadphase_table_create` and associated configuration/layer operations. Consult the exact extension signature for each function; do not infer the whole native Jolt API is available in Python.

## 5. Root exports versus extension exports

The distinction matters:

```python
import pymoss

# Package-root exports (verified in __init__.py)
window = pymoss.Window("Demo", 800, 600)
pymoss.poll_events()

# Extension-level names are separate and are not all re-exported:
from pymoss import _pymoss
# Inspect the extension for the class/function before calling it.
```

Do not update a package-level cheatsheet from `_pymoss` registrations unless the package initializer is also changed to re-export those names.

## 6. Enums and input

Extension-level enums registered in the inspected source:

- `Keyboard`
- `Mouse`
- `GamepadAxis`
- `GamepadButton`

Use enum members from the installed extension rather than passing arbitrary integer values where a pybind enum is expected. For gamepad polling, refresh/update state in the way the binding requires before reading transition queries.

## 7. Lifetime, errors and native handles

- Prefer `with Window(...) as window` for deterministic window release.
- Do not retain the value returned by `native_handle()` beyond the native owner's lifetime.
- Close gamepads and other extension resources using their own close/remove/release operations where exposed.
- Check model load results and inspect `model_last_load_error_message` after a failed model load.
- Do not share GPU/XR/physics resources across devices/worlds unless the wrapper explicitly permits it.
- pybind11 exceptions and native status codes may surface differently depending on each wrapper; inspect the specific binding function.

## 8. API coverage and follow-up audit

This reference separates names confirmed in the package initializer from names registered by the native extension source. For complete signature documentation, add parameter names/types, defaults, return types and exceptions from every `m.def`/`class_` declaration. Then add those extension names to `pymoss.__init__` only when they are intended as stable package-level API.
