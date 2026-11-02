# PyMoss Cheatsheet

> Quick reference for the idiomatic Python public API. The companion [API Reference](API_Reference.md) defines the complete feature map and all exported native-to-language projections. The public calls below are the target design, not a claim that the current binding has implemented every member yet.

## Language design at a glance

The public surface is a Python package. Use snake_case module functions, PascalCase classes, type annotations, keyword-only options, Python exceptions, context managers, iterators and properties for stable state.

## Subsystem map

| Moss subsystem | Public module / facade | Responsibility |
|---|---|---|
| Core | `moss.core` | Foundational value types, logging, memory, profiler, jobs, streams and utilities. |
| Platform | `moss.platform` | Windows/surfaces, monitors, events, input, gamepads, sensors, cameras, storage and dialogs. |
| GPU | `moss.gpu` | Devices, buffers, textures, samplers, shaders, pipelines, command recording, swapchains and synchronization. |
| Renderer | `moss.renderer` | Frame lifecycle, materials, resource sets, framebuffers, meshes, models, fonts, sprites, cameras and debug draw. |
| Audio | `moss.audio` | Audio engine, sources, streams, channels, effects, listeners, acoustics, speaker and microphone capture. |
| GUI | `moss.gui` | Contexts, windows, containers, widgets, text/scalar input and drawing. |
| Physics | `moss.physics` | Bodies, shapes, filters, collisions, constraints, characters, vehicles, soft bodies and navigation integration. |
| Network | `moss.network` | Hosts, peers, packets, transports, HTTP/TLS, WebSocket and WebRTC where enabled. |
| XR | `moss.xr` | Runtime/session lifecycle, views, actions, tracking, haptics, hand/body/face tracking and composition. |

## 1. Import Moss

```python
from collections.abc import Sequence
import moss
from moss import platform, gpu, renderer, audio
```

## 2. Create a window and pump events

```python
import moss

with moss.Application() as app:
    window = app.platform.create_window(
        title="Moss application",
        size=(1280, 720),
    )
    while not window.should_close:
        app.platform.poll_events()
        # Update application state.
# Application and Window release their native handles.
```

## 3. Create and upload a GPU buffer

```python
device = app.gpu.create_device(moss.gpu.GpuDeviceDesc())
with device.create_buffer(
    size=vertices.nbytes,
    usage=moss.gpu.GpuBufferUsage.VERTEX,
) as buffer:
    buffer.upload(memoryview(vertices))
# Buffer closes before the device.
```

## 4. Render a frame

```python
with app.renderer.begin_frame() as frame:
    frame.camera = camera
    frame.draw_sprite(sprite)
    frame.draw_text("Hello Moss", position, text_style)
```

## 5. Play audio

```python
moss.audio.initialize()
try:
    with moss.audio.load_wav("assets/theme.wav") as source:
        source.play()
        moss.audio.update(delta_time)
finally:
    moss.audio.shutdown()
```

## 6. Build a GUI

```python
with app.gui.create_context() as ui:
    with ui.window("Inspector") as panel:
        panel.text("Player")
        enabled = panel.checkbox("Enabled", current_value)
```

## 7. Simulate physics

```python
with app.physics.create_system(settings) as physics:
    body = physics.bodies.create(
        shape=moss.physics.Shape.box(size),
        motion_type=moss.physics.MotionType.DYNAMIC,
    )
    physics.step(delta_time)
    contacts = physics.query.contacts(body)
```

## 8. Networking

```python
with app.network.create_host(host_options) as host:
    for event in host.events(timeout=0.01):
        if event.type is moss.network.EventType.RECEIVED:
            handle_packet(event.peer, event.packet)
```

## 9. XR session

```python
with app.xr.create_session(renderer=app.renderer) as session:
    for event in session.events():
        session.handle_event(event)
        if session.begin_frame():
            views = session.locate_views()
            # Render active views.
            session.end_frame()
```

## 10. Common type families

| Category | Public types |
|---|---|
| Vectors | `Vec2`, `Vec3`, `Vec4`, float/double/integer variants |
| Matrices and transforms | `Mat22`, `Mat33`, `Mat44`, `DMat44`, `Quat`, `Basis` |
| Geometry | `AABB2`, `AABB3`, `OBB2`, `OBB3`, `Plane`, `Rect`, `Triangle` |
| Colour | `Color` with explicit component range and storage precision |
| Containers | language-native lists/maps/sets or typed Moss containers when allocator semantics matter |
| Results and references | target-language result/error types and explicit owned/borrowed resource wrappers |
| Jobs and synchronization | job system, thread pool, barrier, mutex, semaphore and scoped locks |

## 11. Resource-management rules

Owned resources implement `close()` and context-manager methods; use `with` wherever possible. A resource retains its owning device/context. Borrowed views are read-only wrappers and do not own the pointer. Use `memoryview`/buffer protocol for synchronous data transfer; copy by default when native code retains data. Unregister callbacks before dropping references.

## 12. Error-handling rules

Raise `MossError` (with subclasses for initialization, resource, backend and state failures). Use `None` for ordinary absence, `False` for a negative query and exceptions for failure. Do not return C-style error integers unless they are meaningful application data. Python callbacks must not allow exceptions to unwind through native frames.

## 13. Feature and platform checks

- A declaration in the reference does not mean every backend implements it. Check the selected GPU backend, platform, XR runtime and optional networking/codec features.
- Keep GPU resources associated with the device that created them; release child resources before the device.
- Pair renderer frame begin/end, GUI begin/end, render-pass begin/end and XR frame begin/end operations. Prefer scoped/structured helpers so early returns cannot leave a subsystem unbalanced.
- Poll platform and network events at the cadence required by the subsystem. Do not block a browser/UI thread waiting for asynchronous I/O.
- Use the API Reference's subsystem and header inventory when looking for advanced Navigation, Geometry, Physics, Resources, Skeleton or Variants APIs.

## 14. Where to look next

See [API_Reference.md](API_Reference.md) for the full native symbol crosswalk, target-language signatures, resource models, threading rules, feature coverage and the complete 270-header / 19-inline-header inventory.
