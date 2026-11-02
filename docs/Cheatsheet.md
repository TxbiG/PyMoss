# PyMoss Python Cheatsheet

## Package

- Distribution: `PyMoss` 0.1.0.
- Python: >=3.10.
- Binding: pybind11 native extension `pymoss._pymoss`.

## Package-root imports

```python
import pymoss

with pymoss.Window("Moss", 1280, 720) as window:
    while not window.should_close():
        pymoss.poll_events()
```

`Window(title, width, height)` validates positive dimensions. Methods: `should_close()`, `is_open()`, `close()`, `set_title(title)`, `native_handle()`, context-manager methods.

Root helper functions:

- `poll_events()`
- `primary_monitor_name()`
- `primary_monitor_position()`
- `primary_monitor_physical_size()`
- `primary_monitor_content_scale()`
- `window_size()`
- `mouse_position()`
- `set_mouse_position(...)`

## Extension-level surface

Not all extension names are re-exported by `pymoss`:

- Classes: `Monitor`, `Gamepad`.
- Enums: `Keyboard`, `Mouse`, `GamepadAxis`, `GamepadButton`.
- Models: `model_load`, `model_load_obj`, `model_load_fbx`, `model_load_glb`, `model_remove`, `model_last_load_error`, `model_last_load_error_message`, bone/morph helpers.
- Renderer: `renderer_draw_model`, `renderer_draw_mesh`, `debug_draw_list_render`.
- GPU: device/feature queries, buffer download, command-buffer and command-recording operations.
- XR: initialization, sessions, frame timing, views, swapchains, anchors and actions.
- Physics: broadphase table creation/configuration and collision layer operations.

Check the installed extension for exact signatures and ensure the name is actually available before using it.

## Lifetime reminders

- Use `with` for `Window`.
- Do not keep `native_handle()` after the owner is closed.
- Release gamepad/model/GPU/XR/physics resources with their matching wrapper operation.
- Check model load errors before attempting to draw a failed model.

See [API Reference](API_Reference.md) for the root-versus-extension distinction and API groups.
