# PyMoss API Reference

> **Complete-parity public API design for Python.** This document specifies the intended idiomatic API when the binding exposes the full Moss feature set. It is a language-facing design contract, not a statement that every projected member is implemented in the current wrapper.

## 1. API design principles

Moss C++/CMoss is the canonical feature and behaviour baseline. The language binding is a **native Python library**, not a mechanical translation of C++ identifiers. The FFI may use C-compatible handles and exported `Moss_*` symbols internally, but ordinary users should not see those details.

The public surface is a Python package. Use snake_case module functions, PascalCase classes, type annotations, keyword-only options, Python exceptions, context managers, iterators and properties for stable state.

The binding must preserve feature coverage and semantics, not the number or spelling of native functions. Several native calls may become one managed method, a property, an iterator, a scoped object or a language protocol. Conversely, an overloaded native function may become distinct named methods when the language does not support overloads cleanly.

### Non-negotiable rules

- Keep the complete native feature set: Core, Platform, GPU, Renderer, Audio, GUI, Physics, Networking, XR, Navigation, Geometry, Resources, Skeleton, TriangleSplitter and Variants.
- Expose resource objects rather than asking users to pass opaque pointers repeatedly.
- Hide allocator plumbing, user-data pointers, C calling conventions, struct packing and raw WASM/JNI/P/Invoke handles behind the binding layer.
- Preserve status-query meaning. `false`, `0`, an empty collection and an absent optional value are not interchangeable with an error.
- Use asynchronous operations only when the target runtime requires them. WebAssembly startup and browser fetches are asynchronous; ordinary native API calls are not made asynchronous merely for stylistic consistency.
- Document platform/backend feature gates. A declared API is not proof that a backend or optional transport is available in a particular build.

## 2. Package and module layout

```python
from collections.abc import Sequence
import moss
from moss import platform, gpu, renderer, audio
```

### Subsystem map

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

The public package/module root should follow the repository's established package metadata. The names in this document are the recommended public design; implementation namespaces can be adjusted to the actual package identifier without changing the member design.

## 3. Core language type system

| Native Moss concept | Python public representation | Design requirement |
|---|---|---|
| `bool` | `bool` | Keep boolean status and query values boolean. |
| Fixed-width signed/unsigned integers | `int` | Preserve width and unsigned semantics at the ABI boundary. |
| `size_t` | `int` | Must be large enough for the host architecture. |
| `float` / `double` | `float` | Do not narrow double precision silently. |
| UTF-8 text | `str` | Copy or borrow according to documented native lifetime; validate encoding. |
| Opaque native resource | `typed class with close()/context manager` | Never expose raw pointers as normal user-facing values. |
| Pointer + element count | `memoryview / bytes / sequence` | Validate length, element type and retention duration. |
| Function pointer + user data | `callable retained until unregister` | Unregister before callback state is released. |

### Value types and descriptors

Vectors, matrices, quaternions, colours, bounds, planes, rectangles and descriptor types should be real Python values. Use value structs/records where copy semantics are cheap and safe; use resource classes only for objects with identity or native lifetime. Descriptor objects should have named fields/options instead of long sequences of positional primitives. Validate invalid enum combinations before crossing FFI.

## 4. Public resource model

| Moss native resource family | Recommended public type | Ownership model |
|---|---|---|
| Window / monitor / gamepad / camera / capture / storage | `Window`, `Monitor`, `Gamepad`, `Camera`, `Capture`, `Storage` | The application owns platform state; each opened resource has explicit close/dispose semantics. |
| GPU device / buffer / texture / sampler / shader / pipeline / command buffer / fence / semaphore | `GpuDevice`, `GpuBuffer`, `GpuTexture`, `GpuSampler`, `GpuShader`, `GpuPipeline`, `GpuCommandBuffer`, `GpuFence`, `GpuSemaphore` | Child objects retain or borrow their creating device; destroy children before device shutdown. |
| Renderer / material / framebuffer / mesh / model / font / sprite / batch | `Renderer`, `Material`, `Framebuffer`, `Mesh`, `Model`, `Font`, `Sprite`, `SpriteBatch` | Renderer-owned resources cannot outlive the renderer unless explicitly reference-counted. |
| Audio source / stream / listener / effect / microphone | `AudioSource`, `AudioStream`, `AudioListener`, `AudioEffect`, `Microphone` | Release sources, streams and listeners before audio shutdown. |
| GUI context | `GuiContext` | Context scopes widget calls; begin/end pairs are balanced by scoped helpers or callbacks. |
| Physics system / body / shape / constraint / character / vehicle / soft body | `PhysicsSystem`, `Body`, `Shape`, `Constraint`, `Character`, `Vehicle`, `SoftBody` | System and job/allocator dependencies outlive bodies and simulation work. |
| Network host / peer / packet / HTTP request / WebSocket / WebRTC | `NetworkHost`, `Peer`, `Packet`, `HttpRequest`, `WebSocket`, `WebRtcConnection` | Explicit ownership of hosts, packets, requests and connections. |
| XR instance / session / action set / action / hand / body / face | `XrRuntime`, `XrSession`, `XrActionSet`, `XrAction`, `HandTracker`, `BodyTracker`, `FaceTracker` | Session-bound objects cannot outlive the session/runtime they depend on. |

The exact implementation may consolidate thin native objects where that is safer (for example, a renderer may own framebuffers or a device may own command pools). The public API must still make ownership unambiguous.

## 5. Lifecycle, errors, callbacks and concurrency

### Resource lifetime

Owned resources implement `close()` and context-manager methods; use `with` wherever possible. A resource retains its owning device/context. Borrowed views are read-only wrappers and do not own the pointer. Use `memoryview`/buffer protocol for synchronous data transfer; copy by default when native code retains data. Unregister callbacks before dropping references.

### Errors and results

Raise `MossError` (with subclasses for initialization, resource, backend and state failures). Use `None` for ordinary absence, `False` for a negative query and exceptions for failure. Do not return C-style error integers unless they are meaningful application data. Python callbacks must not allow exceptions to unwind through native frames.

### Threading and callbacks

- State which thread owns window/event pumping, renderer submission, audio callbacks, physics updates and XR frame operations.
- Do not promise thread safety unless the native subsystem guarantees it.
- Keep callback state alive until native unregistration completes; no callback may run after disposal/close returns.
- Never allow a managed exception, panic or language-runtime longjmp to unwind through a C ABI frame.
- For asynchronous APIs, define cancellation, completion and disposal behaviour. Do not return a view over memory that is freed when the Promise/callback completes.

## 6. Idiomatic quick-start patterns

### Platform and event loop

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

### GPU resource creation

```python
device = app.gpu.create_device(moss.gpu.GpuDeviceDesc())
with device.create_buffer(
    size=vertices.nbytes,
    usage=moss.gpu.GpuBufferUsage.VERTEX,
) as buffer:
    buffer.upload(memoryview(vertices))
# Buffer closes before the device.
```

### Renderer frame

```python
with app.renderer.begin_frame() as frame:
    frame.camera = camera
    frame.draw_sprite(sprite)
    frame.draw_text("Hello Moss", position, text_style)
```

### Audio playback

```python
moss.audio.initialize()
try:
    with moss.audio.load_wav("assets/theme.wav") as source:
        source.play()
        moss.audio.update(delta_time)
finally:
    moss.audio.shutdown()
```

### GUI context and widgets

```python
with app.gui.create_context() as ui:
    with ui.window("Inspector") as panel:
        panel.text("Player")
        enabled = panel.checkbox("Enabled", current_value)
```

### Physics world

```python
with app.physics.create_system(settings) as physics:
    body = physics.bodies.create(
        shape=moss.physics.Shape.box(size),
        motion_type=moss.physics.MotionType.DYNAMIC,
    )
    physics.step(delta_time)
    contacts = physics.query.contacts(body)
```

### Networking

```python
with app.network.create_host(host_options) as host:
    for event in host.events(timeout=0.01):
        if event.type is moss.network.EventType.RECEIVED:
            handle_packet(event.peer, event.packet)
```

### XR session

```python
with app.xr.create_session(renderer=app.renderer) as session:
    for event in session.events():
        session.handle_event(event)
        if session.begin_frame():
            views = session.locate_views()
            # Render active views.
            session.end_frame()
```

The examples show the intended public shape and lifecycle rules. Names and overloads in the index below are the parity mapping target; implementation-specific initialization parameters can be added to the relevant options objects without exposing the FFI.

## 7. Complete public API projection index

The following table maps every exported callable/allocator callback in the supplied CMoss declaration catalogue to a proposed Python public entry point. The native name is included **only for traceability**. It is not the public name that developers should call. Target signatures use idiomatic types and hide receiver/context handles where those belong to an object.

- **1063 indexed entries:** 1,058 callable/constructor declarations and five allocator callback values.
- Duplicate native overloads are retained as separate rows where their parameters differ; the public API should resolve each to a distinct overload or a disambiguated method name.
- Native pointer/out-parameter patterns should become owned resource values, spans/slices, tuples/records or callback objects as appropriate to the language.

### `Moss_Audio.h` → `moss.audio`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_Init_Audio` | `moss.audio.initialize(...)` | `moss.audio.initialize() -> None` | function |
| `Moss_Terminate_Audio` | `moss.audio.shutdown(...)` | `moss.audio.shutdown() -> None` | function |
| `Moss_AudioUpdate` | `moss.audio.update(...)` | `moss.audio.update(delta_time: float) -> None` | function |
| `Moss_AudioLoadWavFile` | `moss.audio.load_wav_file(...)` | `moss.audio.load_wav_file(path: str \| PathLike[str]) -> AudioSource` | function |
| `Moss_AudioLoadWav` | `moss.audio.load_wav(...)` | `moss.audio.load_wav() -> AudioSource` | function |
| `Moss_AudioLoadOgg` | `moss.audio.load_ogg(...)` | `moss.audio.load_ogg(filename: str, type: AudioLoadType) -> AudioSource` | function |
| `Moss_AudioLoadMP3` | `moss.audio.load_mp3(...)` | `moss.audio.load_mp3(filename: str) -> AudioSource` | function |
| `Moss_AudioSourceDestroy` | `AudioSource.close(...)` | `AudioSource.close(self) -> None` | function |
| `Moss_AudioCaptureMicrophone` | `moss.audio.capture_microphone(...)` | `moss.audio.capture_microphone(microphone: Microphone) -> AudioSource` | function |
| `Moss_AudioCreateEffect` (overload 1) | `moss.audio.create_effect(...)` | `moss.audio.create_effect(type: AudioEffectType) -> AudioEffect` | function |
| `Moss_AudioCreateEffect` (overload 2) | `AudioEffect.set_parameter(...)` | `AudioEffect.set_parameter(self, param_name: str, value: float) -> None` | function |
| `Moss_AudioRemoveEffect` | `AudioEffect.close(...)` | `AudioEffect.close(self) -> None` | function |
| `Moss_AudioCreateChannel` | `moss.audio.create_channel(...)` | `moss.audio.create_channel(channel: ChannelId) -> ChannelId` | function |
| `Audio_RemoveChannel` | `moss.audio.remove_channel(...)` | `moss.audio.remove_channel(channel: ChannelId) -> None` | function |
| `Moss_AudioGetMasterChannel` | `moss.audio.get_master_channel(...)` | `moss.audio.get_master_channel() -> ChannelId` | function |
| `Moss_AudioSetChannelVolume` | `moss.audio.set_channel_volume(...)` | `moss.audio.set_channel_volume(channel: ChannelId, volume: float) -> None` | function |
| `Moss_AudioSetChannelMute` | `moss.audio.set_channel_mute(...)` | `moss.audio.set_channel_mute(channel: ChannelId, mute: bool) -> None` | function |
| `Moss_AudioAddChannelEffect` | `moss.audio.add_effect_to_channel(...)` | `moss.audio.add_effect_to_channel(channel: ChannelId, effect: AudioEffect) -> None` | function |
| `Moss_AudioRemoveChannelEffect` | `moss.audio.remove_effect_from_channel(...)` | `moss.audio.remove_effect_from_channel(channel: ChannelId, effect: AudioEffect) -> None` | function |
| `Moss_AudioRemoveAllChannelEffects` | `moss.audio.clear_channel_effects(...)` | `moss.audio.clear_channel_effects(channel: ChannelId) -> None` | function |
| `Moss_AudioStreamCreate` | `moss.audio.create_stream(...)` | `moss.audio.create_stream() -> AudioStream` | function |
| `Moss_AudioStreamPlay` | `AudioStream.play(...)` | `AudioStream.play(self) -> None` | function |
| `Moss_AudioStreamStop` | `AudioStream.stop(...)` | `AudioStream.stop(self) -> None` | function |
| `Moss_AudioStreamSetVolume` | `AudioStream.set_volume(...)` | `AudioStream.set_volume(self, volume: float) -> None` | function |
| `Moss_AudioStreamSetPitch` | `AudioStream.set_pitch(...)` | `AudioStream.set_pitch(self, pitch: float) -> None` | function |
| `Moss_AudioStreamSetPlaybackRate` | `AudioStream.set_playback_rate(...)` | `AudioStream.set_playback_rate(self, rate: float) -> None` | function |
| `Moss_AudioStreamSetPan` | `AudioStream.set_pan(...)` | `AudioStream.set_pan(self, pan: float) -> None` | function |
| `Moss_AudioStreamSetLoop` | `AudioStream.set_loop(...)` | `AudioStream.set_loop(self, loop: bool) -> None` | function |
| `Moss_AudioStreamRemove` | `AudioStream.close(...)` | `AudioStream.close(self) -> None` | function |
| `Moss_AudioStream2DCreate` | `moss.audio.create_stream_dimension2_d(...)` | `moss.audio.create_stream_dimension2_d() -> AudioStreamDIMENSION2D` | function |
| `Moss_AudioStream2DPlay` | `AudioStreamDIMENSION2D.play(...)` | `AudioStreamDIMENSION2D.play(self) -> None` | function |
| `Moss_AudioStream2DStop` | `AudioStreamDIMENSION2D.stop(...)` | `AudioStreamDIMENSION2D.stop(self) -> None` | function |
| `Moss_AudioStream2DSetVolume` | `AudioStreamDIMENSION2D.set_volume(...)` | `AudioStreamDIMENSION2D.set_volume(self, volume: float) -> None` | function |
| `Moss_AudioStream2DSetPitch` | `AudioStreamDIMENSION2D.set_pitch(...)` | `AudioStreamDIMENSION2D.set_pitch(self, pitch: float) -> None` | function |
| `Moss_AudioStream2DSetPan` | `AudioStreamDIMENSION2D.set_pan(...)` | `AudioStreamDIMENSION2D.set_pan(self, pan: float) -> None` | function |
| `Moss_AudioStream2DSetLoop` | `AudioStreamDIMENSION2D.set_loop(...)` | `AudioStreamDIMENSION2D.set_loop(self, loop: bool) -> None` | function |
| `Moss_AudioStream3DSetPosition` (overload 1) | `AudioStreamDIMENSION2D.stream_dimension3_set_position(...)` | `AudioStreamDIMENSION2D.stream_dimension3_set_position(self) -> None` | function |
| `Moss_AudioStream3DSetVelocity` (overload 1) | `AudioStreamDIMENSION2D.stream_dimension3_set_velocity(...)` | `AudioStreamDIMENSION2D.stream_dimension3_set_velocity(self) -> None` | function |
| `Moss_AudioStream3DSetMaxDistance` (overload 1) | `AudioStreamDIMENSION2D.stream_dimension3_set_max_distance(...)` | `AudioStreamDIMENSION2D.stream_dimension3_set_max_distance(self) -> None` | function |
| `Moss_AudioStream2DRemove` | `AudioStreamDIMENSION2D.close(...)` | `AudioStreamDIMENSION2D.close(self) -> None` | function |
| `Moss_AudioStream3DCreate` | `moss.audio.create_stream_dimension3_d(...)` | `moss.audio.create_stream_dimension3_d() -> AudioStreamDIMENSION3D` | function |
| `Moss_AudioStream3DPlay` | `AudioStreamDIMENSION3D.play(...)` | `AudioStreamDIMENSION3D.play(self) -> None` | function |
| `Moss_AudioStream3DStop` | `AudioStreamDIMENSION3D.stop(...)` | `AudioStreamDIMENSION3D.stop(self) -> None` | function |
| `Moss_AudioStream3DSetVolume` | `AudioStreamDIMENSION3D.set_volume(...)` | `AudioStreamDIMENSION3D.set_volume(self, volume: float) -> None` | function |
| `Moss_AudioStream3DSetPitch` | `AudioStreamDIMENSION3D.set_pitch(...)` | `AudioStreamDIMENSION3D.set_pitch(self, pitch: float) -> None` | function |
| `Moss_AudioStream3DSetPlaybackRate` | `AudioStreamDIMENSION3D.set_playback_rate(...)` | `AudioStreamDIMENSION3D.set_playback_rate(self, rate: float) -> None` | function |
| `Moss_AudioStream3DSetPan` | `AudioStreamDIMENSION3D.set_pan(...)` | `AudioStreamDIMENSION3D.set_pan(self, pan: float) -> None` | function |
| `Moss_AudioStream3DSetLoop` | `AudioStreamDIMENSION3D.set_loop(...)` | `AudioStreamDIMENSION3D.set_loop(self, loop: bool) -> None` | function |
| `Moss_AudioStream3DSetPosition` (overload 2) | `AudioStreamDIMENSION3D.set_position(...)` | `AudioStreamDIMENSION3D.set_position(self) -> None` | function |
| `Moss_AudioStream3DSetVelocity` (overload 2) | `AudioStreamDIMENSION3D.set_velocity(...)` | `AudioStreamDIMENSION3D.set_velocity(self) -> None` | function |
| `Moss_AudioStream3DSetMaxDistance` (overload 2) | `AudioStreamDIMENSION3D.set_max_distance(...)` | `AudioStreamDIMENSION3D.set_max_distance(self) -> None` | function |
| `Moss_AudioStream3DSetDistanceModel` | `AudioStreamDIMENSION3D.set_distance_model(...)` | `AudioStreamDIMENSION3D.set_distance_model(self, model: DistanceModel) -> None` | function |
| `Moss_AudioStream3DRemove` | `AudioStreamDIMENSION3D.close(...)` | `AudioStreamDIMENSION3D.close(self) -> None` | function |
| `Moss_AudioCreateAudioListener2D` | `moss.audio.create_audio_listener_dimension2_d(...)` | `moss.audio.create_audio_listener_dimension2_d(position: Float2) -> AudioListenerDIMENSION2D` | function |
| `Moss_AudioCreateAudioListener3D` | `moss.audio.create_audio_listener_dimension3_d(...)` | `moss.audio.create_audio_listener_dimension3_d(position: Float3) -> AudioListenerDIMENSION3D` | function |
| `Moss_AudioCreateRayAudioListener2D` | `moss.audio.create_ray_audio_listener_dimension2_d(...)` | `moss.audio.create_ray_audio_listener_dimension2_d(position: Float2) -> RayAudioListenerDIMENSION2D` | function |
| `Moss_AudioCreateRayAudioListener3D` | `moss.audio.create_ray_audio_listener_dimension3_d(...)` | `moss.audio.create_ray_audio_listener_dimension3_d(position: Float3) -> RayAudioListenerDIMENSION3D` | function |
| `Moss_AudioRemoveAudioListener2D` | `AudioListenerDIMENSION2D.close(...)` | `AudioListenerDIMENSION2D.close(self) -> None` | function |
| `Moss_AudioRemoveAudioListener3D` | `AudioListenerDIMENSION3D.close(...)` | `AudioListenerDIMENSION3D.close(self) -> None` | function |
| `Moss_AudioRemoveRayAudioListener2D` | `RayAudioListenerDIMENSION2D.close(...)` | `RayAudioListenerDIMENSION2D.close(self) -> None` | function |
| `Moss_AudioRemoveRayAudioListener3D` | `RayAudioListenerDIMENSION3D.close(...)` | `RayAudioListenerDIMENSION3D.close(self) -> None` | function |
| `Moss_AudioActivateAudioListener2D` | `AudioListenerDIMENSION2D.activate(...)` | `AudioListenerDIMENSION2D.activate(self, activate: bool) -> None` | function |
| `Moss_AudioActivateAudioListener3D` | `AudioListenerDIMENSION3D.activate(...)` | `AudioListenerDIMENSION3D.activate(self, activate: bool) -> None` | function |
| `Moss_AudioActivateRayAudioListener2D` | `RayAudioListenerDIMENSION2D.activate(...)` | `RayAudioListenerDIMENSION2D.activate(self, activate: bool) -> None` | function |
| `Moss_AudioActivateRayAudioListener3D` | `RayAudioListenerDIMENSION3D.activate(...)` | `RayAudioListenerDIMENSION3D.activate(self, activate: bool) -> None` | function |
| `Moss_AudioListenerSetOrientation` | `AudioListenerDIMENSION3D.listener_set_orientation(...)` | `AudioListenerDIMENSION3D.listener_set_orientation(self, forward: Vec3, up: Vec3) -> None` | function |
| `Moss_AudioRayListener2DSetPhysics` | `RayAudioListenerDIMENSION2D.ray_set_physics(...)` | `RayAudioListenerDIMENSION2D.ray_set_physics(self, physics: PhysicsSystem, raycast: AudioRaycastDIMENSION2DCallback) -> None` | function |
| `Moss_AudioRayListener3DSetPhysics` | `RayAudioListenerDIMENSION3D.ray_set_physics(...)` | `RayAudioListenerDIMENSION3D.ray_set_physics(self, physics: PhysicsSystem, raycast: AudioRaycastDIMENSION3DCallback) -> None` | function |
| `Moss_AudioRayListener2DSetTraceSettings` | `RayAudioListenerDIMENSION2D.ray_set_trace_settings(...)` | `RayAudioListenerDIMENSION2D.ray_set_trace_settings(self, max_distance: float, reflection_rays: int) -> None` | function |
| `Moss_AudioRayListener3DSetTraceSettings` | `RayAudioListenerDIMENSION3D.ray_set_trace_settings(...)` | `RayAudioListenerDIMENSION3D.ray_set_trace_settings(self, max_distance: float, reflection_rays: int) -> None` | function |
| `Moss_AudioRayTrace2D` | `moss.audio.ray_trace_dimension2_d(...)` | `moss.audio.ray_trace_dimension2_d(desc: AudioRayTraceDIMENSION2DDesc, out_result: AudioRayTraceResult) -> bool` | function |
| `Moss_AudioRayTrace3D` | `moss.audio.ray_trace_dimension3_d(...)` | `moss.audio.ray_trace_dimension3_d(desc: AudioRayTraceDIMENSION3DDesc, out_result: AudioRayTraceResult) -> bool` | function |
| `Moss_AudioRayTraceFromListener2D` | `RayAudioListenerDIMENSION2D.ray_trace_from(...)` | `RayAudioListenerDIMENSION2D.ray_trace_from(self, source_position: Vec2, out_result: AudioRayTraceResult) -> bool` | function |
| `Moss_AudioRayTraceFromListener3D` | `RayAudioListenerDIMENSION3D.ray_trace_from(...)` | `RayAudioListenerDIMENSION3D.ray_trace_from(self, source_position: Vec3, out_result: AudioRayTraceResult) -> bool` | function |
| `Moss_AudioRayTraceComputeGain` | `AudioRayTraceResult.ray_trace_compute_gain(...)` | `AudioRayTraceResult.ray_trace_compute_gain(self) -> float` | function |
| `Moss_IsSpeakerDeviceReady` | `moss.audio.is_speaker_device_ready(...)` | `moss.audio.is_speaker_device_ready() -> bool` | function |
| `Moss_AudioSpeakerOpen` | `moss.audio.speaker_open(...)` | `moss.audio.speaker_open() -> None` | function |
| `Moss_AudioSpeakerPause` | `moss.audio.speaker_pause(...)` | `moss.audio.speaker_pause() -> None` | function |
| `Moss_AudioSpeakerResume` | `moss.audio.speaker_resume(...)` | `moss.audio.speaker_resume() -> None` | function |
| `Moss_AudioSpeakerIsPaused` | `moss.audio.speaker_is_paused(...)` | `moss.audio.speaker_is_paused() -> bool` | function |
| `Moss_AudioSelectSpeakerDevice` | `moss.audio.select_speaker_device(...)` | `moss.audio.select_speaker_device(id: int) -> bool` | function |
| `Moss_GetCurrentSpeakerDeviceID` | `moss.audio.get_current_speaker_device_id(...)` | `moss.audio.get_current_speaker_device_id() -> int` | function |
| `Moss_GetSpeakerDeviceName` | `moss.audio.get_speaker_device_name(...)` | `moss.audio.get_speaker_device_name(id: int) -> str \| None` | function |
| `Moss_ListSpeakerDevices` | `moss.audio.list_speaker_devices(...)` | `moss.audio.list_speaker_devices() -> int` | function |
| `Moss_MicrophoneGetDeviceCount` | `moss.audio.microphone_get_device_count(...)` | `moss.audio.microphone_get_device_count() -> int` | function |
| `Moss_MicrophoneGetDeviceName` | `moss.audio.microphone_get_device_name(...)` | `moss.audio.microphone_get_device_name(device_index: int) -> str \| None` | function |
| `Moss_MicrophoneOpen` | `moss.audio.microphone_open(...)` | `moss.audio.microphone_open(desc: MicrophoneDesc) -> Microphone` | function |
| `Moss_MicrophoneClose` | `Microphone.close(...)` | `Microphone.close(self) -> None` | function |
| `Moss_MicrophoneStart` | `Microphone.start(...)` | `Microphone.start(self) -> bool` | function |
| `Moss_MicrophoneStop` | `Microphone.stop(...)` | `Microphone.stop(self) -> None` | function |
| `Moss_MicrophoneRead` | `Microphone.read(...)` | `Microphone.read(self, out_samples: MutableSequence[float], max_frames: int) -> int` | function |
| `Moss_MicrophoneSetGain` | `Microphone.set_gain(...)` | `Microphone.set_gain(self, gain: float) -> None` | function |
| `Moss_MicrophoneGetSampleRate` | `Microphone.get_sample_rate(...)` | `Microphone.get_sample_rate(self) -> int` | function |
| `Moss_MicrophoneGetChannels` | `Microphone.get_channels(...)` | `Microphone.get_channels(self) -> int` | function |
| `Moss_MicrophoneSetCallback` | `Microphone.set_callback(...)` | `Microphone.set_callback(self, callback: MicrophoneCallback) -> None` | function |
| `Moss_MicrophoneGetLevels` | `Microphone.get_levels(...)` | `Microphone.get_levels(self) -> MicrophoneLevels` | function |
| `Moss_MicrophoneGetLevelRMS` | `Microphone.get_level_rms(...)` | `Microphone.get_level_rms(self) -> float` | function |
| `Moss_MicrophoneGetLevelPeak` | `Microphone.get_level_peak(...)` | `Microphone.get_level_peak(self) -> float` | function |
| `Moss_MicrophoneGetSmoothedVolume` | `Microphone.get_smoothed_volume(...)` | `Microphone.get_smoothed_volume(self) -> float` | function |
| `Moss_MicrophoneGetVoiceActivity` | `Microphone.get_voice_activity(...)` | `Microphone.get_voice_activity(self) -> float` | function |
| `Moss_IsMicrophoneDeviceReady` | `moss.audio.is_microphone_device_ready(...)` | `moss.audio.is_microphone_device_ready() -> bool` | function |
| `Moss_AudioMicrophoneOpen` | `moss.audio.microphone_open(...)` | `moss.audio.microphone_open() -> int` | function |
| `Moss_AudioMicrophoneClose` | `moss.audio.microphone_close(...)` | `moss.audio.microphone_close() -> None` | function |
| `Moss_AudioMicrophonePlay` | `moss.audio.microphone_play(...)` | `moss.audio.microphone_play() -> None` | function |
| `Moss_AudioMicrophoneStop` | `moss.audio.microphone_stop(...)` | `moss.audio.microphone_stop() -> None` | function |
| `Moss_AudioMicrophoneID` | `moss.audio.microphone_id(...)` | `moss.audio.microphone_id() -> int` | function |
| `Moss_AudioSelectMicrophoneDevice` | `moss.audio.select_microphone_device(...)` | `moss.audio.select_microphone_device(id: int) -> bool` | function |
| `Moss_GetMicrophoneDeviceName` | `moss.audio.get_microphone_device_name(...)` | `moss.audio.get_microphone_device_name(index: int) -> str \| None` | function |
| `Moss_ListMicrophoneDevices` | `moss.audio.list_microphone_devices(...)` | `moss.audio.list_microphone_devices() -> int` | function |
| `Moss_AudioMicrophoneSetGain` | `Microphone.set_gain(...)` | `Microphone.set_gain(self, gain: float) -> None` | function |
| `Moss_AudioMicrophoneGetSampleRate` | `Microphone.get_sample_rate(...)` | `Microphone.get_sample_rate(self) -> int` | function |
| `Moss_AudioMicrophoneGetChannels` | `Microphone.get_channels(...)` | `Microphone.get_channels(self) -> int` | function |
| `Moss_AudioStreamSetCallback` | `AudioStream.set_callback(...)` | `AudioStream.set_callback(self, callback: AudioStreamCallback) -> None` | function |

### `Moss_GPU.h` → `moss.gpu`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_AcquireGPUCommandBuffer` | `GpuDevice.acquire_command_buffer(...)` | `GpuDevice.acquire_command_buffer(self) -> GpuCommandBuffer` | function |
| `Moss_BeginGPUComputePass` | `GpuCommandBuffer.begin_compute_pass(...)` | `GpuCommandBuffer.begin_compute_pass(self) -> None` | function |
| `Moss_BeginGPUCopyPass` | `GpuCommandBuffer.begin_copy_pass(...)` | `GpuCommandBuffer.begin_copy_pass(self) -> None` | function |
| `Moss_CreateGPUBuffer` | `GpuDevice.create_buffer(...)` | `GpuDevice.create_buffer(self, desc: GpuBufferDesc) -> GpuBuffer` | function |
| `Moss_DestroyGPUDevice` | `GpuDevice.close(...)` | `GpuDevice.close(self) -> None` | function |
| `Moss_BeginGPURenderPass` | `GpuCommandBuffer.begin_render_pass(...)` | `GpuCommandBuffer.begin_render_pass(self, framebuffer: Framebuffer) -> None` | function |
| `Moss_BindGPUComputeSamplers` | `GpuCommandBuffer.bind_compute_samplers(...)` | `GpuCommandBuffer.bind_compute_samplers(self, first_slot: int, samplers: Sequence[GpuSampler*]) -> None` | function |
| `Moss_BindGPUComputeTextures` | `GpuCommandBuffer.bind_compute_textures(...)` | `GpuCommandBuffer.bind_compute_textures(self, first_slot: int, textures: Sequence[GpuTexture*]) -> None` | function |
| `Moss_BindGPUComputeUniformBuffers` | `GpuCommandBuffer.bind_compute_uniform_buffers(...)` | `GpuCommandBuffer.bind_compute_uniform_buffers(self, first_slot: int, bindings: Sequence[GpuBufferBinding]) -> None` | function |
| `Moss_BindGPUFragmentSamplers` | `GpuCommandBuffer.bind_fragment_samplers(...)` | `GpuCommandBuffer.bind_fragment_samplers(self, first_slot: int, samplers: Sequence[GpuSampler*]) -> None` | function |
| `Moss_BindGPUFragmentTextures` | `GpuCommandBuffer.bind_fragment_textures(...)` | `GpuCommandBuffer.bind_fragment_textures(self, first_slot: int, textures: Sequence[GpuTexture*]) -> None` | function |
| `Moss_BindGPUFragmentUniformBuffers` | `GpuCommandBuffer.bind_fragment_uniform_buffers(...)` | `GpuCommandBuffer.bind_fragment_uniform_buffers(self, first_slot: int, bindings: Sequence[GpuBufferBinding]) -> None` | function |
| `Moss_BindGPUIndexBuffer` | `GpuCommandBuffer.bind_index(...)` | `GpuCommandBuffer.bind_index(self, buffer: GpuBuffer, offset: int, index_type: GpuIndexType) -> None` | function |
| `Moss_BindGPUPipeline` | `GpuCommandBuffer.bind_pipeline(...)` | `GpuCommandBuffer.bind_pipeline(self, pipeline: PipelineState) -> None` | function |
| `Moss_BindGPUResourceSet` | `GpuCommandBuffer.bind_resource_set(...)` | `GpuCommandBuffer.bind_resource_set(self, set_index: int, set: ResourceSet) -> None` | function |
| `Moss_BindGPUVertexBuffers` | `GpuCommandBuffer.bind_vertex_buffers(...)` | `GpuCommandBuffer.bind_vertex_buffers(self, first_binding: int, bindings: Sequence[GpuBufferBinding]) -> None` | function |
| `Moss_BindGPUVertexSamplers` | `GpuCommandBuffer.bind_vertex_samplers(...)` | `GpuCommandBuffer.bind_vertex_samplers(self, first_slot: int, samplers: Sequence[GpuSampler*]) -> None` | function |
| `Moss_BindGPUVertexTextures` | `GpuCommandBuffer.bind_vertex_textures(...)` | `GpuCommandBuffer.bind_vertex_textures(self, first_slot: int, textures: Sequence[GpuTexture*]) -> None` | function |
| `Moss_BindGPUVertexUniformBuffers` | `GpuCommandBuffer.bind_vertex_uniform_buffers(...)` | `GpuCommandBuffer.bind_vertex_uniform_buffers(self, first_slot: int, bindings: Sequence[GpuBufferBinding]) -> None` | function |
| `Moss_BlitGPUTexture` | `GpuCommandBuffer.blit_texture(...)` | `GpuCommandBuffer.blit_texture(self, src: Texture, dst: Texture, region: GpuBlitRegion) -> None` | function |
| `Moss_CalculateGPUTextureFormatSize` | `moss.gpu.calculate_texture_format_size(...)` | `moss.gpu.calculate_texture_format_size(format: TextureFormat, width: int, height: int, depth: int) -> int` | function |
| `Moss_CancelGPUCommandBuffer` | `GpuCommandBuffer.cancel(...)` | `GpuCommandBuffer.cancel(self) -> None` | function |
| `Moss_CopyGPUBufferToBuffer` | `GpuCommandBuffer.copy_to_buffer(...)` | `GpuCommandBuffer.copy_to_buffer(self, src: GpuBuffer, dst: GpuBuffer, src_region: GpuBufferRegion, dst_region: GpuBufferRegion) -> None` | function |
| `Moss_CopyGPUBufferToTexture` | `GpuCommandBuffer.copy_to_texture(...)` | `GpuCommandBuffer.copy_to_texture(self, src: GpuBuffer, dst: GpuTexture, src_region: GpuBufferRegion, dst_region: GpuTextureRegion) -> None` | function |
| `Moss_CopyGPUTextureToBuffer` | `GpuCommandBuffer.copy_texture_to(...)` | `GpuCommandBuffer.copy_texture_to(self, src: GpuTexture, dst: GpuBuffer, src_region: GpuTextureRegion, dst_region: GpuBufferRegion) -> None` | function |
| `Moss_CopyGPUTextureToTexture` | `GpuCommandBuffer.copy_texture_to_texture(...)` | `GpuCommandBuffer.copy_texture_to_texture(self, src: GpuTexture, dst: GpuTexture, src_region: GpuTextureRegion, dst_region: GpuTextureRegion) -> None` | function |
| `Moss_CreateGPUSampler` | `GpuDevice.create_sampler(...)` | `GpuDevice.create_sampler(self, create_info: GpuSamplerCreateInfo) -> GpuSampler` | function |
| `Moss_CreateGPUShader` | `GpuDevice.create_shader(...)` | `GpuDevice.create_shader(self, create_info: GpuShaderCreateInfo) -> GpuShader` | function |
| `Moss_CreateGPUComputePipeline` | `GpuDevice.create_compute_pipeline(...)` | `GpuDevice.create_compute_pipeline(self, create_info: GpuComputePipelineCreateInfo) -> ComputePipelineState` | function |
| `Moss_CreateGPUTexture` | `GpuDevice.create_texture(...)` | `GpuDevice.create_texture(self, create_info: GpuTextureCreateInfo) -> GpuTexture` | function |
| `Moss_CreateGPUTextureView` | `GpuDevice.create_texture_view(...)` | `GpuDevice.create_texture_view(self, create_info: GpuTextureViewCreateInfo) -> GpuTextureView` | function |
| `Moss_CreateGPUTransferBuffer` | `GpuDevice.create_transfer_buffer(...)` | `GpuDevice.create_transfer_buffer(self, create_info: GpuTransferBufferCreateInfo) -> GpuTransferBuffer` | function |
| `Moss_CreateGPUFramebuffer` | `GpuDevice.create_framebuffer(...)` | `GpuDevice.create_framebuffer(self, create_info: GpuFramebufferCreateInfo) -> Framebuffer` | function |
| `Moss_CreateGPUResourceSetLayout` | `GpuDevice.create_resource_set_layout(...)` | `GpuDevice.create_resource_set_layout(self, create_info: GpuResourceSetLayoutCreateInfo) -> ResourceSetLayout` | function |
| `Moss_CreateGPUResourceSet` | `GpuDevice.create_resource_set(...)` | `GpuDevice.create_resource_set(self, create_info: GpuResourceSetCreateInfo) -> ResourceSet` | function |
| `Moss_CreateGPUQueryPool` | `GpuDevice.create_query_pool(...)` | `GpuDevice.create_query_pool(self, create_info: GpuQueryPoolCreateInfo) -> GpuQueryPool` | function |
| `Moss_DispatchGPUCompute` | `GpuCommandBuffer.dispatch_compute(...)` | `GpuCommandBuffer.dispatch_compute(self, group_count_x: int, group_count_y: int, group_count_z: int) -> None` | function |
| `Moss_DispatchGPUComputeIndirect` | `GpuCommandBuffer.dispatch_compute_indirect(...)` | `GpuCommandBuffer.dispatch_compute_indirect(self, indirect_buffer: GpuBuffer, offset: int) -> None` | function |
| `Moss_DownloadFromGPUBuffer` | `GpuBuffer.download_from(...)` | `GpuBuffer.download_from(self, dst_data: memoryview, size: int, src_offset: int) -> None` | function |
| `Moss_DownloadFromGPUTexture` | `GpuTexture.download_from(...)` | `GpuTexture.download_from(self, src_region: GpuTextureRegion, dst_data: memoryview, dst_row_pitch: int, dst_slice_pitch: int) -> None` | function |
| `Moss_DrawGPUIndexedPrimitives` | `GpuCommandBuffer.draw_indexed_primitives(...)` | `GpuCommandBuffer.draw_indexed_primitives(self, index_count: int, instance_count: int, first_index: int, vertex_offset: int, first_instance: int) -> None` | function |
| `Moss_DrawGPUIndexedPrimitivesIndirect` | `GpuCommandBuffer.draw_indexed_primitives_indirect(...)` | `GpuCommandBuffer.draw_indexed_primitives_indirect(self, indirect_buffer: GpuBuffer, offset: int, draw_count: int, stride: int) -> None` | function |
| `Moss_DrawGPUPrimitives` | `GpuCommandBuffer.draw_primitives(...)` | `GpuCommandBuffer.draw_primitives(self, vertex_count: int, instance_count: int, first_vertex: int, first_instance: int) -> None` | function |
| `Moss_DrawGPUPrimitivesIndirect` | `GpuCommandBuffer.draw_primitives_indirect(...)` | `GpuCommandBuffer.draw_primitives_indirect(self, indirect_buffer: GpuBuffer, offset: int, draw_count: int, stride: int) -> None` | function |
| `Moss_EndGPUComputePass` | `GpuCommandBuffer.end_compute_pass(...)` | `GpuCommandBuffer.end_compute_pass(self) -> None` | function |
| `Moss_EndGPUCopyPass` | `GpuCommandBuffer.end_copy_pass(...)` | `GpuCommandBuffer.end_copy_pass(self) -> None` | function |
| `Moss_EndGPURenderPass` | `GpuCommandBuffer.end_render_pass(...)` | `GpuCommandBuffer.end_render_pass(self) -> None` | function |
| `Moss_GDKResumeGPU` | `GpuDevice.gdk_resume(...)` | `GpuDevice.gdk_resume(self) -> None` | function |
| `Moss_GDKSuspendGPU` | `GpuDevice.gdk_suspend(...)` | `GpuDevice.gdk_suspend(self) -> None` | function |
| `Moss_GenerateMipmapsForGPUTexture` | `GpuCommandBuffer.generate_mipmaps_for_texture(...)` | `GpuCommandBuffer.generate_mipmaps_for_texture(self, texture: GpuTexture) -> None` | function |
| `Moss_GetGPUDriver` | `moss.gpu.get_driver(...)` | `moss.gpu.get_driver(driver_index: int) -> str \| None` | function |
| `Moss_GetCompiledGPUBackendType` | `moss.gpu.get_compiled_backend_type(...)` | `moss.gpu.get_compiled_backend_type() -> GpuBackendType` | function |
| `Moss_GetGPUDeviceBackendType` | `GpuDevice.get_backend_type(...)` | `GpuDevice.get_backend_type(self) -> GpuBackendType` | function |
| `Moss_GPUBackendGetName` | `moss.gpu.backend_get_name(...)` | `moss.gpu.backend_get_name(backend: GpuBackendType) -> str \| None` | function |
| `Moss_GPUBackendGetShaderFormats` | `moss.gpu.backend_get_shader_formats(...)` | `moss.gpu.backend_get_shader_formats(backend: GpuBackendType) -> int` | function |
| `Moss_GPUBackendIsCompiled` | `moss.gpu.backend_is_compiled(...)` | `moss.gpu.backend_is_compiled(backend: GpuBackendType) -> bool` | function |
| `Moss_GPUBackendIsSupportedOnPlatform` | `moss.gpu.backend_is_supported_on_platform(...)` | `moss.gpu.backend_is_supported_on_platform(backend: GpuBackendType) -> bool` | function |
| `Moss_GetGPUBackendInfo` | `moss.gpu.get_backend_info(...)` | `moss.gpu.get_backend_info(backend: GpuBackendType, out_info: GpuBackendInfo) -> bool` | function |
| `Moss_RegisterNativeGPUBackend` | `moss.gpu.register_native_backend(...)` | `moss.gpu.register_native_backend(backend: GpuBackendType, native_table: memoryview) -> bool` | function |
| `Moss_HasNativeGPUBackend` | `moss.gpu.has_native_backend(...)` | `moss.gpu.has_native_backend(backend: GpuBackendType) -> bool` | function |
| `Moss_GetGPUShaderFormats` | `GpuDevice.get_shader_formats(...)` | `GpuDevice.get_shader_formats(self) -> int` | function |
| `Moss_GetGPUTextureFormatFromPixelFormat` | `moss.gpu.get_texture_format_from_pixel_format(...)` | `moss.gpu.get_texture_format_from_pixel_format(pixel_format: PixelFormat) -> TextureFormat` | function |
| `Moss_GetNumGPUDrivers` | `moss.gpu.get_num_drivers(...)` | `moss.gpu.get_num_drivers() -> int` | function |
| `Moss_GetPixelFormatFromGPUTextureFormat` | `moss.gpu.get_pixel_format_from_texture_format(...)` | `moss.gpu.get_pixel_format_from_texture_format(texture_format: TextureFormat) -> PixelFormat` | function |
| `Moss_GPUSupportsProperties` | `GpuDevice.supports_properties(...)` | `GpuDevice.supports_properties(self) -> bool` | function |
| `Moss_GPUSupportsShaderFormats` | `GpuDevice.supports_shader_formats(...)` | `GpuDevice.supports_shader_formats(self, shader_format_mask: int) -> bool` | function |
| `Moss_GPUTextureFormatTexelBlockSize` | `moss.gpu.texture_format_texel_block_size(...)` | `moss.gpu.texture_format_texel_block_size(format: TextureFormat) -> int` | function |
| `Moss_GPUTextureSupportsFormat` | `GpuDevice.texture_supports_format(...)` | `GpuDevice.texture_supports_format(self, format: TextureFormat, usage: TextureUsage) -> bool` | function |
| `Moss_GPUTextureSupportsSampleCount` | `GpuDevice.texture_supports_sample_count(...)` | `GpuDevice.texture_supports_sample_count(self, format: TextureFormat, sample_count: int) -> bool` | function |
| `Moss_InsertGPUDebugLabel` | `GpuCommandBuffer.insert_debug_label(...)` | `GpuCommandBuffer.insert_debug_label(self, label: str) -> None` | function |
| `Moss_MapGPUTransferBuffer` | `GpuTransferBuffer.map(...)` | `GpuTransferBuffer.map(self) -> memoryview` | function |
| `Moss_PopGPUDebugGroup` | `GpuCommandBuffer.pop_debug_group(...)` | `GpuCommandBuffer.pop_debug_group(self) -> None` | function |
| `Moss_PushGPUComputeUniformData` | `GpuCommandBuffer.push_compute_uniform_data(...)` | `GpuCommandBuffer.push_compute_uniform_data(self, slot: int, data: memoryview, size: int) -> None` | function |
| `Moss_PushGPUDebugGroup` | `GpuCommandBuffer.push_debug_group(...)` | `GpuCommandBuffer.push_debug_group(self, label: str) -> None` | function |
| `Moss_PushGPUFragmentUniformData` | `GpuCommandBuffer.push_fragment_uniform_data(...)` | `GpuCommandBuffer.push_fragment_uniform_data(self, slot: int, data: memoryview, size: int) -> None` | function |
| `Moss_PushGPUVertexUniformData` | `GpuCommandBuffer.push_vertex_uniform_data(...)` | `GpuCommandBuffer.push_vertex_uniform_data(self, slot: int, data: memoryview, size: int) -> None` | function |
| `Moss_QueryGPUFence` | `GpuFence.query(...)` | `GpuFence.query(self) -> bool` | function |
| `Moss_ReleaseGPUBuffer` | `GpuBuffer.release(...)` | `GpuBuffer.release(self) -> None` | function |
| `Moss_ReleaseGPUFence` | `GpuFence.release(...)` | `GpuFence.release(self) -> None` | function |
| `Moss_ReleaseGPUSampler` | `GpuSampler.release(...)` | `GpuSampler.release(self) -> None` | function |
| `Moss_ReleaseGPUShader` | `GpuShader.release(...)` | `GpuShader.release(self) -> None` | function |
| `Moss_ReleaseGPUComputePipeline` | `GpuDevice.release_compute_pipeline(...)` | `GpuDevice.release_compute_pipeline(self, pipeline: ComputePipelineState) -> None` | function |
| `Moss_ReleaseGPUTexture` | `GpuTexture.release(...)` | `GpuTexture.release(self) -> None` | function |
| `Moss_ReleaseGPUTextureView` | `GpuTextureView.release(...)` | `GpuTextureView.release(self) -> None` | function |
| `Moss_ReleaseGPUTransferBuffer` | `GpuTransferBuffer.release(...)` | `GpuTransferBuffer.release(self) -> None` | function |
| `Moss_ReleaseGPUFramebuffer` | `Framebuffer.release(...)` | `Framebuffer.release(self) -> None` | function |
| `Moss_ReleaseGPUResourceSetLayout` | `ResourceSetLayout.release(...)` | `ResourceSetLayout.release(self) -> None` | function |
| `Moss_ReleaseGPUResourceSet` | `ResourceSet.release(...)` | `ResourceSet.release(self) -> None` | function |
| `Moss_ReleaseGPUQueryPool` | `GpuQueryPool.release(...)` | `GpuQueryPool.release(self) -> None` | function |
| `Moss_SetGPUAllowedFramesInFlight` | `GpuDevice.set_allowed_frames_in_flight(...)` | `GpuDevice.set_allowed_frames_in_flight(self, frames_in_flight: int) -> None` | function |
| `Moss_SetGPUBlendConstants` | `GpuCommandBuffer.set_blend_constants(...)` | `GpuCommandBuffer.set_blend_constants(self, r: float, g: float, b: float, a: float) -> None` | function |
| `Moss_SetGPUBufferName` | `GpuBuffer.set_name(...)` | `GpuBuffer.set_name(self, name: str) -> None` | function |
| `Moss_SetGPUScissor` (overload 1) | `GpuCommandBuffer.set_scissor(...)` | `GpuCommandBuffer.set_scissor(self, scissor: Rect) -> None` | function |
| `Moss_SetGPUStencilReference` | `GpuCommandBuffer.set_stencil_reference(...)` | `GpuCommandBuffer.set_stencil_reference(self, reference: int) -> None` | function |
| `Moss_SetGPUTextureName` | `GpuTexture.set_name(...)` | `GpuTexture.set_name(self, name: str) -> None` | function |
| `Moss_SetGPUViewport` | `GpuCommandBuffer.set_viewport(...)` | `GpuCommandBuffer.set_viewport(self, viewport: GpuViewport) -> None` | function |
| `Moss_SetGPUScissor` (overload 2) | `GpuCommandBuffer.set_scissor(...)` | `GpuCommandBuffer.set_scissor(self, scissors: Rect) -> None` | function |
| `Moss_SubmitGPUCommandBuffer` | `GpuCommandBuffer.submit(...)` | `GpuCommandBuffer.submit(self) -> None` | function |
| `Moss_SubmitGPUCommandBufferAndAcquireFence` | `GpuCommandBuffer.submit_and_acquire_fence(...)` | `GpuCommandBuffer.submit_and_acquire_fence(self) -> GpuFence` | function |
| `Moss_BindGPUComputePipeline` | `GpuCommandBuffer.bind_compute_pipeline(...)` | `GpuCommandBuffer.bind_compute_pipeline(self, pipeline: ComputePipelineState) -> None` | function |
| `Moss_BarrierGPUResources` | `GpuCommandBuffer.barrier_resources(...)` | `GpuCommandBuffer.barrier_resources(self, barrier_info: GpuBarrierInfo) -> None` | function |
| `Moss_ResetGPUQueryPool` | `GpuCommandBuffer.reset_query_pool(...)` | `GpuCommandBuffer.reset_query_pool(self, query_pool: GpuQueryPool, first_query: int, query_count: int) -> None` | function |
| `Moss_WriteGPUTimestamp` | `GpuCommandBuffer.write_timestamp(...)` | `GpuCommandBuffer.write_timestamp(self, query_pool: GpuQueryPool, query_index: int) -> None` | function |
| `Moss_BeginGPUQuery` | `GpuCommandBuffer.begin_query(...)` | `GpuCommandBuffer.begin_query(self, query_pool: GpuQueryPool, query_index: int) -> None` | function |
| `Moss_EndGPUQuery` | `GpuCommandBuffer.end_query(...)` | `GpuCommandBuffer.end_query(self, query_pool: GpuQueryPool, query_index: int) -> None` | function |
| `Moss_GetGPUQueryResults` | `GpuQueryPool.get_query_results(...)` | `GpuQueryPool.get_query_results(self, first_query: int, query_count: int, data: memoryview, data_size: int, stride: int, flags: GpuQueryResultFlags) -> bool` | function |
| `Moss_TransitionGPUBuffer` | `GpuCommandBuffer.transition(...)` | `GpuCommandBuffer.transition(self, buffer: GpuBuffer, old_state: ResourceState, new_state: ResourceState) -> None` | function |
| `Moss_TransitionGPUTexture` | `GpuCommandBuffer.transition_texture(...)` | `GpuCommandBuffer.transition_texture(self, texture: GpuTexture, old_state: ResourceState, new_state: ResourceState) -> None` | function |
| `Moss_UnmapGPUTransferBuffer` | `GpuTransferBuffer.unmap(...)` | `GpuTransferBuffer.unmap(self) -> None` | function |
| `Moss_UploadToGPUBuffer` | `GpuBuffer.upload_to(...)` | `GpuBuffer.upload_to(self, src_data: memoryview, size: int, dst_offset: int) -> None` | function |
| `Moss_UploadToGPUTexture` | `GpuTexture.upload_to(...)` | `GpuTexture.upload_to(self, transfer_info: GpuTextureTransferInfo) -> None` | function |
| `Moss_WaitForGPUFences` | `GpuDevice.wait_for_fences(...)` | `GpuDevice.wait_for_fences(self, fences: Sequence[GpuFence*], wait_all: bool, timeout_ns: int) -> None` | function |
| `Moss_WaitForGPUIdle` | `GpuDevice.wait_for_idle(...)` | `GpuDevice.wait_for_idle(self) -> None` | function |
| `Moss_WindowSupportsGPUPresentMode` | `Window.supports_present_mode(...)` | `Window.supports_present_mode(self, present_mode: GpuPresentMode) -> bool` | function |
| `Moss_GPUBlitInfo` | `GpuBlitInfo(...)` | `GpuBlitInfo() -> GpuBlitInfo` | constructor |
| `Moss_GPUBlitRegion` | `GpuBlitRegion(...)` | `GpuBlitRegion() -> GpuBlitRegion` | constructor |
| `Moss_GPUBufferBinding` | `GpuBufferBinding(...)` | `GpuBufferBinding() -> GpuBufferBinding` | constructor |
| `Moss_GPUBufferCreateInfo` | `GpuBufferCreateInfo(...)` | `GpuBufferCreateInfo() -> GpuBufferCreateInfo` | constructor |
| `Moss_GPUBufferLocation` | `GpuBufferLocation(...)` | `GpuBufferLocation() -> GpuBufferLocation` | constructor |
| `Moss_GPUBufferRegion` | `GpuBufferRegion(...)` | `GpuBufferRegion() -> GpuBufferRegion` | constructor |
| `Moss_GPUColorTargetBlendState` | `GpuColorTargetBlendState(...)` | `GpuColorTargetBlendState() -> GpuColorTargetBlendState` | constructor |
| `Moss_GPUColorTargetDescription` | `GpuColorTargetDescription(...)` | `GpuColorTargetDescription() -> GpuColorTargetDescription` | constructor |
| `Moss_GPUColorTargetInfo` | `GpuColorTargetInfo(...)` | `GpuColorTargetInfo() -> GpuColorTargetInfo` | constructor |
| `Moss_GPUDepthStencilState` | `GpuDepthStencilState(...)` | `GpuDepthStencilState() -> GpuDepthStencilState` | constructor |
| `Moss_GPUDepthStencilTargetInfo` | `GpuDepthStencilTargetInfo(...)` | `GpuDepthStencilTargetInfo() -> GpuDepthStencilTargetInfo` | constructor |
| `Moss_GPUIndexedIndirectDrawCommand` | `GpuIndexedIndirectDrawCommand(...)` | `GpuIndexedIndirectDrawCommand() -> GpuIndexedIndirectDrawCommand` | constructor |
| `Moss_GPUIndirectDispatchCommand` | `GpuIndirectDispatchCommand(...)` | `GpuIndirectDispatchCommand() -> GpuIndirectDispatchCommand` | constructor |
| `Moss_GPUIndirectDrawCommand` | `GpuIndirectDrawCommand(...)` | `GpuIndirectDrawCommand() -> GpuIndirectDrawCommand` | constructor |
| `Moss_GPUMultisampleState` | `GpuMultisampleState(...)` | `GpuMultisampleState() -> GpuMultisampleState` | constructor |
| `Moss_GPURasterizerState` | `GpuRasterizerState(...)` | `GpuRasterizerState() -> GpuRasterizerState` | constructor |
| `Moss_GPUSamplerCreateInfo` | `GpuSamplerCreateInfo(...)` | `GpuSamplerCreateInfo() -> GpuSamplerCreateInfo` | constructor |
| `Moss_GPUShaderCreateInfo` | `GpuShaderCreateInfo(...)` | `GpuShaderCreateInfo() -> GpuShaderCreateInfo` | constructor |
| `Moss_GPUStencilOpState` | `GpuStencilOpState(...)` | `GpuStencilOpState() -> GpuStencilOpState` | constructor |
| `Moss_GPUTextureCreateInfo` | `GpuTextureCreateInfo(...)` | `GpuTextureCreateInfo() -> GpuTextureCreateInfo` | constructor |
| `Moss_GPUTextureLocation` | `GpuTextureLocation(...)` | `GpuTextureLocation() -> GpuTextureLocation` | constructor |
| `Moss_GPUTextureRegion` | `GpuTextureRegion(...)` | `GpuTextureRegion() -> GpuTextureRegion` | constructor |
| `Moss_GPUTextureSamplerBinding` | `GpuTextureSamplerBinding(...)` | `GpuTextureSamplerBinding() -> GpuTextureSamplerBinding` | constructor |
| `Moss_GPUTextureTransferInfo` | `GpuTextureTransferInfo(...)` | `GpuTextureTransferInfo() -> GpuTextureTransferInfo` | constructor |
| `Moss_GPUTransferBufferCreateInfo` | `GpuTransferBufferCreateInfo(...)` | `GpuTransferBufferCreateInfo() -> GpuTransferBufferCreateInfo` | constructor |
| `Moss_GPUTransferBufferLocation` | `GpuTransferBufferLocation(...)` | `GpuTransferBufferLocation() -> GpuTransferBufferLocation` | constructor |
| `Moss_GPUVertexAttribute` | `GpuVertexAttribute(...)` | `GpuVertexAttribute() -> GpuVertexAttribute` | constructor |
| `Moss_GPUVertexBufferDescription` | `GpuVertexBufferDescription(...)` | `GpuVertexBufferDescription() -> GpuVertexBufferDescription` | constructor |
| `Moss_GPUVertexInputState` | `GpuVertexInputState(...)` | `GpuVertexInputState() -> GpuVertexInputState` | constructor |
| `Moss_GPUViewport` | `GpuViewport(...)` | `GpuViewport() -> GpuViewport` | constructor |
| `Moss_BindGPUComputeStorageBuffers` | `GpuCommandBuffer.bind_compute_storage_buffers(...)` | `GpuCommandBuffer.bind_compute_storage_buffers(self, first_slot: int, bindings: Sequence[GpuStorageBufferReadWriteBinding]) -> None` | function |
| `Moss_BindGPUComputeStorageTextures` | `GpuCommandBuffer.bind_compute_storage_textures(...)` | `GpuCommandBuffer.bind_compute_storage_textures(self, first_slot: int, bindings: Sequence[GpuStorageTextureReadWriteBinding]) -> None` | function |
| `Moss_BindGPUFragmentStorageBuffers` | `GpuCommandBuffer.bind_fragment_storage_buffers(...)` | `GpuCommandBuffer.bind_fragment_storage_buffers(self, first_slot: int, bindings: Sequence[GpuStorageBufferReadWriteBinding]) -> None` | function |
| `Moss_BindGPUFragmentStorageTextures` | `GpuCommandBuffer.bind_fragment_storage_textures(...)` | `GpuCommandBuffer.bind_fragment_storage_textures(self, first_slot: int, bindings: Sequence[GpuStorageTextureReadWriteBinding]) -> None` | function |
| `Moss_BindGPUVertexStorageBuffers` | `GpuCommandBuffer.bind_vertex_storage_buffers(...)` | `GpuCommandBuffer.bind_vertex_storage_buffers(self, first_slot: int, bindings: Sequence[GpuStorageBufferReadWriteBinding]) -> None` | function |
| `Moss_BindGPUVertexStorageTextures` | `GpuCommandBuffer.bind_vertex_storage_textures(...)` | `GpuCommandBuffer.bind_vertex_storage_textures(self, first_slot: int, bindings: Sequence[GpuStorageTextureReadWriteBinding]) -> None` | function |
| `Moss_GetGPUDeviceDriver` | `GpuDevice.get_driver(...)` | `GpuDevice.get_driver(self) -> str \| None` | function |
| `Moss_GetGPUDeviceProperties` | `GpuDevice.get_properties(...)` | `GpuDevice.get_properties(self) -> GpuDeviceProperties` | function |
| `Moss_ReleaseWindowFromGPUDevice` | `GpuDevice.release_window_from(...)` | `GpuDevice.release_window_from(self, window: Window) -> None` | function |
| `Moss_ClaimWindowForGPUDevice` | `GpuDevice.claim_window_for(...)` | `GpuDevice.claim_window_for(self, window: Window) -> bool` | function |
| `Moss_UpdateGPUResourceSet` | `ResourceSet.update(...)` | `ResourceSet.update(self, bindings: Sequence[GpuResourceBinding]) -> None` | function |
| `Moss_CreateGPUDevice` | `moss.gpu.create_device(...)` | `moss.gpu.create_device(descriptor: GpuDeviceDesc) -> GpuDevice` | function |
| `Moss_CreateGPUDeviceWithProperties` | `moss.gpu.create_device_with_properties(...)` | `moss.gpu.create_device_with_properties(descriptor: GpuDeviceDesc, requested_properties: GpuDeviceProperties) -> GpuDevice` | function |
| `Moss_GPUBufferCreate` | `GpuDevice.create_buffer(...)` | `GpuDevice.create_buffer(self, descriptor: GpuBufferDescriptor) -> GpuBuffer` | function |
| `Moss_GPUBufferUpload` | `GpuBuffer.upload(...)` | `GpuBuffer.upload(self, data: bytes \| bytearray \| memoryview, offset: int = 0) -> None` | function |
| `Moss_GPUBufferMap` | `GpuBuffer.map(...)` | `GpuBuffer.map(self) -> memoryview` | function |
| `Moss_GPUBufferUnmap` | `GpuBuffer.unmap(...)` | `GpuBuffer.unmap(self) -> None` | function |
| `Moss_GPUBufferDestroy` | `GpuBuffer.close(...)` | `GpuBuffer.close(self) -> None` | function |
| `Moss_AcquireGPUSwapchainTexture` | `GpuTexture.acquire_swapchain(...)` | `GpuTexture.acquire_swapchain(self, window: Window) -> bool` | function |
| `Moss_AcquireGPUSwapchainTextureStatus` | `GpuTexture.acquire_swapchain_status(...)` | `GpuTexture.acquire_swapchain_status(self, window: Window) -> GpuSwapchainResult` | function |
| `Moss_GetGPUSwapchainTextureFormat` | `GpuDevice.get_swapchain_texture_format(...)` | `GpuDevice.get_swapchain_texture_format(self, window: Window) -> TextureFormat` | function |
| `Moss_SetGPUSwapchainParameters` | `GpuDevice.set_swapchain_parameters(...)` | `GpuDevice.set_swapchain_parameters(self, window: Window, width: int, height: int, present_mode: GpuPresentMode) -> bool` | function |
| `Moss_ResizeGPUSwapchain` | `GpuDevice.resize_swapchain(...)` | `GpuDevice.resize_swapchain(self, window: Window, width: int, height: int) -> GpuSwapchainResult` | function |
| `Moss_WaitAndAcquireGPUSwapchainTexture` | `GpuTexture.wait_and_acquire_swapchain(...)` | `GpuTexture.wait_and_acquire_swapchain(self, window: Window, timeout_ns: int) -> bool` | function |
| `Moss_WaitAndAcquireGPUSwapchainTextureStatus` | `GpuTexture.wait_and_acquire_swapchain_status(...)` | `GpuTexture.wait_and_acquire_swapchain_status(self, window: Window, timeout_ns: int) -> GpuSwapchainResult` | function |
| `Moss_WaitForGPUSwapchain` | `GpuDevice.wait_for_swapchain(...)` | `GpuDevice.wait_for_swapchain(self, window: Window, timeout_ns: int) -> bool` | function |
| `Moss_WindowSupportsGPUSwapchainComposition` | `Window.supports_swapchain_composition(...)` | `Window.supports_swapchain_composition(self) -> bool` | function |
| `Moss_PresentGPUSwapchain` | `GpuDevice.present_swapchain(...)` | `GpuDevice.present_swapchain(self, window: Window, texture: GpuTexture) -> bool` | function |
| `Moss_PresentGPUSwapchainStatus` | `GpuDevice.present_swapchain_status(...)` | `GpuDevice.present_swapchain_status(self, window: Window, texture: GpuTexture) -> GpuSwapchainResult` | function |
| `Moss_GPUClear` | `GpuCommandBuffer.clear(...)` | `GpuCommandBuffer.clear(self, r: float, g: float, b: float, a: float) -> None` | function |
| `Moss_GPUSamplerCreate` | `GpuDevice.create_sampler(...)` | `GpuDevice.create_sampler(self, desc: GpuSamplerDesc) -> GpuSampler` | function |
| `Moss_GPUSamplerDestroy` | `GpuSampler.close(...)` | `GpuSampler.close(self) -> None` | function |
| `Moss_TextureCreate` | `GpuDevice.create_texture(...)` | `GpuDevice.create_texture(self, desc: TextureDesc) -> Texture` | function |
| `Moss_TextureDestroy` | `Texture.close(...)` | `Texture.close(self) -> None` | function |
| `Moss_TextureUpload` | `Texture.upload(...)` | `Texture.upload(self, desc: TextureUploadDesc) -> None` | function |
| `Moss_TextureGenerateMips` | `Texture.generate_mips(...)` | `Texture.generate_mips(self) -> None` | function |
| `Moss_GPUGetExternalFeatureMask` | `moss.gpu.get_external_feature_mask(...)` | `moss.gpu.get_external_feature_mask() -> int` | function |
| `Moss_GPUExternalFeatureAvailable` | `moss.gpu.external_feature_available(...)` | `moss.gpu.external_feature_available(feature: GpuExternalFeature) -> bool` | function |
| `Moss_GPUGetExternalFeatures` | `moss.gpu.get_external_features(...)` | `moss.gpu.get_external_features(out_features: GpuExternalFeatures) -> None` | function |
| `Moss_GPUExternalFeatureName` | `moss.gpu.external_feature_name(...)` | `moss.gpu.external_feature_name(feature: GpuExternalFeature) -> str \| None` | function |
| `Moss_FSR1BuildEasuConstants` | `moss.gpu.fsr1_build_easu_constants(...)` | `moss.gpu.fsr1_build_easu_constants(out_constants16: int, input_viewport_width: float, input_viewport_height: float, input_width: float, input_height: float, output_width: float, output_height: float) -> None` | function |
| `Moss_FSR2GetUpscaleRatio` | `moss.gpu.fsr2_get_upscale_ratio(...)` | `moss.gpu.fsr2_get_upscale_ratio(quality: FSR2QualityMode) -> float` | function |
| `Moss_FSR2GetRenderResolution` | `moss.gpu.fsr2_get_render_resolution(...)` | `moss.gpu.fsr2_get_render_resolution(display_width: int, display_height: int, quality: FSR2QualityMode, out_resolution: FSR2RenderResolution) -> bool` | function |
| `Moss_FSR2GetJitterPhaseCount` | `moss.gpu.fsr2_get_jitter_phase_count(...)` | `moss.gpu.fsr2_get_jitter_phase_count(render_width: int, display_width: int) -> int` | function |
| `Moss_FSR2GetJitterOffset` | `moss.gpu.fsr2_get_jitter_offset(...)` | `moss.gpu.fsr2_get_jitter_offset(out_x: MutableSequence[float], out_y: MutableSequence[float], index: int, phase_count: int) -> bool` | function |
| `Moss_MeshGenerateVertexRemap` | `moss.gpu.mesh_generate_vertex_remap(...)` | `moss.gpu.mesh_generate_vertex_remap(destination: MutableSequence[int], indices: Sequence[Uint32T], vertices: memoryview, vertex_count: int, vertex_size: int) -> int` | function |
| `Moss_MeshOptimizeVertexCache` | `moss.gpu.mesh_optimize_vertex_cache(...)` | `moss.gpu.mesh_optimize_vertex_cache(destination: MutableSequence[int], indices: Sequence[Uint32T], vertex_count: int) -> bool` | function |
| `Moss_MeshOptimizeOverdraw` | `moss.gpu.mesh_optimize_overdraw(...)` | `moss.gpu.mesh_optimize_overdraw(destination: MutableSequence[int], indices: Sequence[Uint32T], vertex_positions: Sequence[Float], vertex_positions_stride: int, threshold: float) -> bool` | function |
| `Moss_MeshOptimizeVertexFetch` | `moss.gpu.mesh_optimize_vertex_fetch(...)` | `moss.gpu.mesh_optimize_vertex_fetch(destination: memoryview, indices: MutableSequence[int], index_count: int, vertices: memoryview, vertex_count: int, vertex_size: int) -> int` | function |
| `Moss_SMOLVEncode` | `moss.gpu.smolv_encode(...)` | `moss.gpu.smolv_encode(spirv_data: memoryview, spirv_size: int, out_smolv_data: memoryview, inout_smolv_size: MutableSequence[int], flags: int) -> bool` | function |
| `Moss_SMOLVGetDecodedSize` | `moss.gpu.smolv_get_decoded_size(...)` | `moss.gpu.smolv_get_decoded_size(smolv_data: memoryview, smolv_size: int) -> int` | function |
| `Moss_SMOLVDecode` | `moss.gpu.smolv_decode(...)` | `moss.gpu.smolv_decode(smolv_data: memoryview, smolv_size: int, out_spirv_data: memoryview, spirv_size: int, flags: int) -> bool` | function |
| `Moss_GPUVulkanMemoryAllocatorAvailable` | `moss.gpu.memory_allocator_available(...)` | `moss.gpu.memory_allocator_available() -> bool` | function |
| `Moss_GPUClothSolverCreate` | `GpuDevice.create_cloth_solver(...)` | `GpuDevice.create_cloth_solver(self, desc: GpuClothSolverDesc) -> GpuClothSolver` | function |
| `Moss_GPUClothSolverDestroy` | `GpuClothSolver.close(...)` | `GpuClothSolver.close(self) -> None` | function |
| `Moss_GPUClothSolverDispatch` | `GpuCommandBuffer.cloth_solver_dispatch(...)` | `GpuCommandBuffer.cloth_solver_dispatch(self, solver: GpuClothSolver, desc: GpuClothDispatchDesc) -> bool` | function |
| `Moss_GPUCommandBufferBegin` | `GpuDevice.create_command_buffer(...)` | `GpuDevice.create_command_buffer(self, queue: CommandQueue) -> GpuCommandBuffer` | function |
| `Moss_GPUCommandBufferEnd` | `GpuCommandBuffer.end(...)` | `GpuCommandBuffer.end(self) -> None` | function |
| `Moss_GPUCommandBufferSubmit` | `GpuCommandBuffer.submit(...)` | `GpuCommandBuffer.submit(self) -> None` | function |
| `Moss_CmdBeginRenderPass` | `GpuCommandBuffer.cmd_begin_render_pass(...)` | `GpuCommandBuffer.cmd_begin_render_pass(self, fb: Framebuffer) -> None` | function |
| `Moss_CmdEndRenderPass` | `GpuCommandBuffer.cmd_end_render_pass(...)` | `GpuCommandBuffer.cmd_end_render_pass(self) -> None` | function |
| `Moss_CmdBindPipeline` | `GpuCommandBuffer.cmd_bind_pipeline(...)` | `GpuCommandBuffer.cmd_bind_pipeline(self, pipeline: PipelineState) -> None` | function |
| `Moss_CmdBindResourceSet` | `GpuCommandBuffer.cmd_bind_resource_set(...)` | `GpuCommandBuffer.cmd_bind_resource_set(self, set_index: int, set: ResourceSet) -> None` | function |
| `Moss_CmdBindVertexBuffer` | `GpuCommandBuffer.cmd_bind_vertex(...)` | `GpuCommandBuffer.cmd_bind_vertex(self, buffer: GpuBuffer, offset: int) -> None` | function |
| `Moss_CmdBindIndexBuffer` | `GpuCommandBuffer.cmd_bind_index(...)` | `GpuCommandBuffer.cmd_bind_index(self, buffer: GpuBuffer, offset: int) -> None` | function |
| `Moss_CmdDraw` | `GpuCommandBuffer.cmd_draw(...)` | `GpuCommandBuffer.cmd_draw(self, vertex_count: int, first_vertex: int) -> None` | function |
| `Moss_CmdDrawIndexed` | `GpuCommandBuffer.cmd_draw_indexed(...)` | `GpuCommandBuffer.cmd_draw_indexed(self, index_count: int, first_index: int, vertex_offset: int) -> None` | function |
| `Moss_CmdDispatch` | `GpuCommandBuffer.cmd_dispatch(...)` | `GpuCommandBuffer.cmd_dispatch(self, x: int, y: int, z: int) -> None` | function |
| `Moss_RGPassReadTexture` | `RGPass.read_texture(...)` | `RGPass.read_texture(self, texture: RGTexture, state: ResourceState) -> RGTexture` | function |
| `Moss_RGPassWriteTexture` | `RGPass.write_texture(...)` | `RGPass.write_texture(self, texture: RGTexture, state: ResourceState) -> RGTexture` | function |
| `Moss_GPUDeviceSupportsAsyncCompute` | `GpuDevice.supports_async_compute(...)` | `GpuDevice.supports_async_compute(self) -> int` | function |
| `Moss_TextureSetResidency` | `Texture.set_residency(...)` | `Texture.set_residency(self, state: ResidencyState) -> None` | function |
| `Moss_ShaderCreate` | `GpuDevice.create_shader(...)` | `GpuDevice.create_shader(self, desc: ShaderDesc) -> Shader` | function |
| `Moss_ShaderDestroy` | `Shader.close(...)` | `Shader.close(self) -> None` | function |
| `Moss_BindlessRegisterTexture` | `GpuDevice.bindless_register_texture(...)` | `GpuDevice.bindless_register_texture(self, texture: memoryview) -> BindlessHandle` | function |
| `Moss_BindlessRegisterBuffer` | `GpuBuffer.bindless_register(...)` | `GpuBuffer.bindless_register(self) -> BindlessHandle` | function |
| `Moss_BindlessUnregister` | `GpuDevice.bindless_unregister(...)` | `GpuDevice.bindless_unregister(self, handle: BindlessHandle) -> None` | function |
| `Moss_PipelineCreate` | `GpuDevice.create_pipeline(...)` | `GpuDevice.create_pipeline(self, desc: PipelineDesc) -> PipelineState` | function |
| `Moss_PipelineDestroy` | `GpuDevice.pipeline_destroy(...)` | `GpuDevice.pipeline_destroy(self, pipeline: PipelineState) -> None` | function |
| `Moss_WindowResizeVK` | `moss.gpu.window_resize_vk(...)` | `moss.gpu.window_resize_vk(physical_device: VkPhysicalDevice, device: VkDevice, swapchain: VkSwapchainKHR, swapchain_extent: VkExtentDIMENSION2D, swapchain_framebuffers: StdVector<VkFramebuffer>, swapchain_image_views: StdVector<VkImageView>, depth_image: VkImage, depth_image_memory: VkDeviceMemory, depth_image_view: VkImageView) -> None` | function |
| `Moss_GPUFatalError` | `moss.gpu.fatal_error(...)` | `moss.gpu.fatal_error(result: MossResult) -> None` | function |
| `Moss_SetGPUViewports` | `GpuCommandBuffer.set_viewports(...)` | `GpuCommandBuffer.set_viewports(self, viewports: Sequence[GpuViewport]) -> None` | function |
| `Moss_SetGPUScissors` | `GpuCommandBuffer.set_scissors(...)` | `GpuCommandBuffer.set_scissors(self, scissors: Sequence[Rect]) -> None` | function |
| `Moss_ClearGPUTexture` | `GpuCommandBuffer.clear_texture(...)` | `GpuCommandBuffer.clear_texture(self, texture: GpuTexture, color: float) -> None` | function |
| `Moss_ClearGPUDepthStencilTexture` | `GpuCommandBuffer.clear_depth_stencil_texture(...)` | `GpuCommandBuffer.clear_depth_stencil_texture(self, texture: GpuTexture, depth: float, stencil: int) -> None` | function |
| `Moss_DrawGPUIndexedPrimitivesIndirectCount` | `GpuCommandBuffer.draw_indexed_primitives_indirect_count(...)` | `GpuCommandBuffer.draw_indexed_primitives_indirect_count(self, indirect_buffer: GpuBuffer, offset: int, count_buffer: GpuBuffer, count_offset: int, max_draw_count: int, stride: int) -> None` | function |
| `Moss_DrawGPUPrimitivesIndirectCount` | `GpuCommandBuffer.draw_primitives_indirect_count(...)` | `GpuCommandBuffer.draw_primitives_indirect_count(self, indirect_buffer: GpuBuffer, offset: int, count_buffer: GpuBuffer, count_offset: int, max_draw_count: int, stride: int) -> None` | function |
| `Moss_SetGPUDepthBias` | `GpuCommandBuffer.set_depth_bias(...)` | `GpuCommandBuffer.set_depth_bias(self, constant_factor: float, clamp: float, slope_factor: float) -> None` | function |
| `Moss_CreateGPUSemaphore` | `GpuDevice.create_semaphore(...)` | `GpuDevice.create_semaphore(self) -> GpuSemaphore` | function |
| `Moss_ReleaseGPUSemaphore` | `GpuSemaphore.release(...)` | `GpuSemaphore.release(self) -> None` | function |
| `Moss_SignalGPUSemaphoreValue` | `GpuSemaphore.signal_value(...)` | `GpuSemaphore.signal_value(self, value: int) -> None` | function |
| `Moss_WaitGPUSemaphoreValue` | `GpuSemaphore.wait_value(...)` | `GpuSemaphore.wait_value(self, value: int, timeout_ns: int) -> bool` | function |
| `Moss_SubmitGPUCommandBufferWithSemaphores` | `GpuCommandBuffer.submit_with_semaphores(...)` | `GpuCommandBuffer.submit_with_semaphores(self, wait_semaphores: GpuSemaphore, wait_values: Sequence[Uint64T], signal_semaphores: GpuSemaphore, signal_values: Sequence[Uint64T]) -> None` | function |
| `Moss_GetGPUBindlessCapacity` | `GpuDevice.get_bindless_capacity(...)` | `GpuDevice.get_bindless_capacity(self, type: ShaderResourceType) -> int` | function |
| `Moss_CreateGPUPipelineCache` | `GpuDevice.create_pipeline_cache(...)` | `GpuDevice.create_pipeline_cache(self, initial_data: memoryview, initial_size: int) -> GpuPipelineCache` | function |
| `Moss_GetGPUPipelineCacheData` | `GpuPipelineCache.get_data(...)` | `GpuPipelineCache.get_data(self, out_data: memoryview, out_size: int) -> int` | function |
| `Moss_ReleaseGPUPipelineCache` | `GpuPipelineCache.release(...)` | `GpuPipelineCache.release(self) -> None` | function |

### `Moss_GUI.h` → `moss.gui`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `CreateContext` | `moss.gui.create_context(...)` | `moss.gui.create_context(shared_font_atlas: FontAtlas) -> GuiContext` | function |
| `DestroyContext` | `GuiContext.close(...)` | `GuiContext.close(self) -> None` | function |
| `GetCurrentContext` | `moss.gui.get_current_context(...)` | `moss.gui.get_current_context() -> GuiContext` | function |
| `SetCurrentContext` | `GuiContext.set_current(...)` | `GuiContext.set_current(self) -> None` | function |
| `BeginWindow` | `GuiContext.begin_window(...)` | `GuiContext.begin_window(self, name: str, p_open: MutableSequence[bool], flags: GuiWindowFlags) -> bool` | function |
| `EndWindow` | `GuiContext.end_window(...)` | `GuiContext.end_window(self) -> None` | function |
| `BeginWindowChild` (overload 1) | `GuiContext.begin_window_child(...)` | `GuiContext.begin_window_child(self, str_id: str, size: Float2, child_flags: GuiChildFlags, window_flags: GuiWindowFlags) -> bool` | function |
| `BeginWindowChild` (overload 2) | `GuiContext.begin_window_child(...)` | `GuiContext.begin_window_child(self, id: int, size: Float2, child_flags: GuiChildFlags, window_flags: GuiWindowFlags) -> bool` | function |
| `EndWindowChild` | `GuiContext.end_window_child(...)` | `GuiContext.end_window_child(self) -> None` | function |
| `BeginContainer` | `GuiContext.begin_container(...)` | `GuiContext.begin_container(self, rect: Rect) -> None` | function |
| `EndContainer` | `GuiContext.end_container(...)` | `GuiContext.end_container(self) -> None` | function |
| `BeginVContainer` | `GuiContext.begin_v_container(...)` | `GuiContext.begin_v_container(self, rect: Rect, spacing: float) -> None` | function |
| `EndVContainer` | `GuiContext.end_v_container(...)` | `GuiContext.end_v_container(self) -> None` | function |
| `BeginHContainer` | `GuiContext.begin_h_container(...)` | `GuiContext.begin_h_container(self, rect: Rect, spacing: float) -> None` | function |
| `EndHContainer` | `GuiContext.end_h_container(...)` | `GuiContext.end_h_container(self) -> None` | function |
| `BeginCenterContainer` | `GuiContext.begin_center_container(...)` | `GuiContext.begin_center_container(self, rect: Rect) -> None` | function |
| `EndCenterContainer` | `GuiContext.end_center_container(...)` | `GuiContext.end_center_container(self) -> None` | function |
| `BeginMarginContainer` | `GuiContext.begin_margin_container(...)` | `GuiContext.begin_margin_container(self, left_margin: float, right_margin: float, top_margin: float, bottom_margin: float) -> None` | function |
| `EndMarginContainer` | `GuiContext.end_margin_container(...)` | `GuiContext.end_margin_container(self) -> None` | function |
| `BeginVScrollContainer` | `GuiContext.begin_v_scroll_container(...)` | `GuiContext.begin_v_scroll_container(self) -> None` | function |
| `EndVScrollContainer` | `GuiContext.end_v_scroll_container(...)` | `GuiContext.end_v_scroll_container(self) -> None` | function |
| `BeginHScrollContainer` | `GuiContext.begin_h_scroll_container(...)` | `GuiContext.begin_h_scroll_container(self) -> None` | function |
| `EndHScrollContainer` | `GuiContext.end_h_scroll_container(...)` | `GuiContext.end_h_scroll_container(self) -> None` | function |
| `BeginGridContainer` | `GuiContext.begin_grid_container(...)` | `GuiContext.begin_grid_container(self) -> None` | function |
| `EndGridContainer` | `GuiContext.end_grid_container(...)` | `GuiContext.end_grid_container(self) -> None` | function |
| `ColorRect` | `GuiContext.color_rect(...)` | `GuiContext.color_rect(self, color: Color, size: Float2) -> None` | function |
| `Text` | `GuiContext.text(...)` | `GuiContext.text(self, fmt: str, args: ...) -> None` | function |
| `RichText` | `GuiContext.rich_text(...)` | `GuiContext.rich_text(self, label: str) -> None` | function |
| `ProgressBar` | `GuiContext.progress_bar(...)` | `GuiContext.progress_bar(self) -> None` | function |
| `TextLink` | `GuiContext.text_link(...)` | `GuiContext.text_link(self, label: str) -> bool` | function |
| `TextLinkOpenURL` | `GuiContext.text_link_open_url(...)` | `GuiContext.text_link_open_url(self, label: str, url: str) -> None` | function |
| `Button` | `GuiContext.button(...)` | `GuiContext.button(self, label: str, rect_ptr: Rect, vp_matrix: Mat44, default_width: float, default_height: float) -> bool` | function |
| `ToggleSwitch` | `GuiContext.toggle_switch(...)` | `GuiContext.toggle_switch(self, active: bool) -> bool` | function |
| `RadioButton` | `GuiContext.radio_button(...)` | `GuiContext.radio_button(self, label: str, active: bool) -> bool` | function |
| `Checkbox` | `GuiContext.checkbox(...)` | `GuiContext.checkbox(self, label: str, value: MutableSequence[bool], rect_ptr: Rect, vp_matrix: Mat44, default_size: float) -> bool` | function |
| `SliderFloat` | `GuiContext.slider_float(...)` | `GuiContext.slider_float(self, min_value: float, max_value: float, value: float) -> None` | function |
| `SliderDouble` | `GuiContext.slider_double(...)` | `GuiContext.slider_double(self, min_value: float, max_value: float, value: float) -> None` | function |
| `SliderInt` | `GuiContext.slider_int(...)` | `GuiContext.slider_int(self, min_value: int, max_value: int, value: int) -> None` | function |
| `SliderFloat2` | `GuiContext.slider_float2(...)` | `GuiContext.slider_float2(self, min_value: Float2, max_value: Float2, value: Float2) -> None` | function |
| `SliderDouble2` | `GuiContext.slider_double2(...)` | `GuiContext.slider_double2(self, min_value: Double2, max_value: Double2, value: Double2) -> None` | function |
| `SliderInt2` | `GuiContext.slider_int2(...)` | `GuiContext.slider_int2(self, min_value: Int2, max_value: Int2, value: Int2) -> None` | function |
| `SliderFloat3` | `GuiContext.slider_float3(...)` | `GuiContext.slider_float3(self, min_value: Float3, max_value: Float3, value: Float3) -> None` | function |
| `SliderDouble3` | `GuiContext.slider_double3(...)` | `GuiContext.slider_double3(self, min_value: Double3, max_value: Double3, value: Double3) -> None` | function |
| `SliderInt3` | `GuiContext.slider_int3(...)` | `GuiContext.slider_int3(self, min_value: Int3, max_value: Int3, value: Int3) -> None` | function |
| `SliderFloat4` | `GuiContext.slider_float4(...)` | `GuiContext.slider_float4(self, min_value: Float4, max_value: Float4, value: Float4) -> None` | function |
| `SliderDouble4` | `GuiContext.slider_double4(...)` | `GuiContext.slider_double4(self, min_value: Double4, max_value: Double4, value: Double4) -> None` | function |
| `SliderInt4` | `GuiContext.slider_int4(...)` | `GuiContext.slider_int4(self, min_value: Int4, max_value: Int4, value: Int4) -> None` | function |
| `DragFloat` | `GuiContext.drag_float(...)` | `GuiContext.drag_float(self, label: str, value: MutableSequence[float], v_speed: float, v_min: float, v_max: float, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragFloat2` | `GuiContext.drag_float2(...)` | `GuiContext.drag_float2(self, label: str, value: Float2, v_speed: float, v_min: float, v_max: float, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragFloat3` | `GuiContext.drag_float3(...)` | `GuiContext.drag_float3(self, label: str, value: Float3, v_speed: float, v_min: float, v_max: float, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragFloat4` | `GuiContext.drag_float4(...)` | `GuiContext.drag_float4(self, label: str, value: Float4, v_speed: float, v_min: float, v_max: float, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragFloatRange2` | `GuiContext.drag_float_range2(...)` | `GuiContext.drag_float_range2(self, label: str, v_current_min: MutableSequence[float], v_current_max: MutableSequence[float], v_speed: float, v_min: float, v_max: float, format: str, format_max: str, flags: GuiSliderFlags) -> bool` | function |
| `DragInt` | `GuiContext.drag_int(...)` | `GuiContext.drag_int(self, label: str, v: MutableSequence[int], v_speed: float, v_min: int, v_max: int, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragInt2` | `GuiContext.drag_int2(...)` | `GuiContext.drag_int2(self, label: str, v: int, v_speed: float, v_min: int, v_max: int, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragInt3` | `GuiContext.drag_int3(...)` | `GuiContext.drag_int3(self, label: str, v: int, v_speed: float, v_min: int, v_max: int, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragInt4` | `GuiContext.drag_int4(...)` | `GuiContext.drag_int4(self, label: str, v: int, v_speed: float, v_min: int, v_max: int, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragIntRange2` | `GuiContext.drag_int_range2(...)` | `GuiContext.drag_int_range2(self, label: str, v_current_min: MutableSequence[int], v_current_max: MutableSequence[int], v_speed: float, v_min: int, v_max: int, format: str, format_max: str, flags: GuiSliderFlags) -> bool` | function |
| `DragScalar` | `GuiContext.drag_scalar(...)` | `GuiContext.drag_scalar(self, label: str, data_type: GuiDataType, p_data: memoryview, v_speed: float, p_min: memoryview, p_max: memoryview, format: str, flags: GuiSliderFlags) -> bool` | function |
| `DragScalarN` | `GuiContext.drag_scalar_n(...)` | `GuiContext.drag_scalar_n(self, label: str, data_type: GuiDataType, p_data: memoryview, components: int, v_speed: float, p_min: memoryview, p_max: memoryview, format: str, flags: GuiSliderFlags) -> bool` | function |
| `InputText` | `GuiContext.input_text(...)` | `GuiContext.input_text(self, label: str, buf: bytearray, buf_size: int, flags: GuiInputTextFlags, callback: GuiInputTextCallback) -> bool` | function |
| `InputTextMultiline` | `GuiContext.input_text_multiline(...)` | `GuiContext.input_text_multiline(self, label: str, buf: bytearray, buf_size: int, size: Float2, flags: GuiInputTextFlags, callback: GuiInputTextCallback) -> bool` | function |
| `InputTextWithHint` | `GuiContext.input_text_with_hint(...)` | `GuiContext.input_text_with_hint(self, label: str, hint: str, buf: bytearray, buf_size: int, flags: GuiInputTextFlags, callback: GuiInputTextCallback) -> bool` | function |
| `InputFloat` | `GuiContext.input_float(...)` | `GuiContext.input_float(self, label: str, v: MutableSequence[float], step: float, step_fast: float, format: str, flags: GuiInputTextFlags) -> bool` | function |
| `InputFloat2` | `GuiContext.input_float2(...)` | `GuiContext.input_float2(self, label: str, v: float, format: str, flags: GuiInputTextFlags) -> bool` | function |
| `InputFloat3` | `GuiContext.input_float3(...)` | `GuiContext.input_float3(self, label: str, v: float, format: str, flags: GuiInputTextFlags) -> bool` | function |
| `InputFloat4` | `GuiContext.input_float4(...)` | `GuiContext.input_float4(self, label: str, v: float, format: str, flags: GuiInputTextFlags) -> bool` | function |
| `InputInt` | `GuiContext.input_int(...)` | `GuiContext.input_int(self, label: str, v: MutableSequence[int], step: int, step_fast: int, flags: GuiInputTextFlags) -> bool` | function |
| `InputInt2` | `GuiContext.input_int2(...)` | `GuiContext.input_int2(self, label: str, v: int, flags: GuiInputTextFlags) -> bool` | function |
| `InputInt3` | `GuiContext.input_int3(...)` | `GuiContext.input_int3(self, label: str, v: int, flags: GuiInputTextFlags) -> bool` | function |
| `InputInt4` | `GuiContext.input_int4(...)` | `GuiContext.input_int4(self, label: str, v: int, flags: GuiInputTextFlags) -> bool` | function |
| `InputDouble` | `GuiContext.input_double(...)` | `GuiContext.input_double(self, label: str, v: MutableSequence[float], step: float, step_fast: float, format: str, flags: GuiInputTextFlags) -> bool` | function |
| `InputScalar` | `GuiContext.input_scalar(...)` | `GuiContext.input_scalar(self, label: str, data_type: GuiDataType, p_data: memoryview, p_step: memoryview, p_step_fast: memoryview, format: str, flags: GuiInputTextFlags) -> bool` | function |
| `InputScalarN` | `GuiContext.input_scalar_n(...)` | `GuiContext.input_scalar_n(self, label: str, data_type: GuiDataType, p_data: memoryview, components: int, p_step: memoryview, p_step_fast: memoryview, format: str, flags: GuiInputTextFlags) -> bool` | function |

### `Moss_Network.h` → `moss.network`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_Init_Network` | `moss.network.initialize(...)` | `moss.network.initialize() -> None` | function |
| `enet_initialize_with_callbacks` | `NetworkCallbacks.initialize_with(...)` | `NetworkCallbacks.initialize_with(self) -> int` | function |
| `Moss_TerminateNetwork` | `moss.network.shutdown(...)` | `moss.network.shutdown() -> None` | function |
| `enet_list_clear` | `NetworkList.clear(...)` | `NetworkList.clear(self) -> None` | function |
| `enet_list_insert` | `moss.network.list_insert(...)` | `moss.network.list_insert(net_list_iterator: E, value: memoryview) -> NetworkListIterator` | function |
| `enet_list_remove` | `moss.network.list_remove(...)` | `moss.network.list_remove(net_list_iterator: E) -> memoryview` | function |
| `enet_list_move` | `moss.network.list_move(...)` | `moss.network.list_move(net_list_iterator: E, value: memoryview, value: memoryview) -> NetworkListIterator` | function |
| `enet_list_size` | `NetworkList.size(...)` | `NetworkList.size(self) -> int` | function |
| `enet_time_get` | `moss.network.time_get(...)` | `moss.network.time_get() -> int` | function |
| `enet_time_set` | `moss.network.time_set(...)` | `moss.network.time_set(int32_t: U) -> None` | function |
| `enet_socket_create` | `moss.network.create_socket(...)` | `moss.network.create_socket(adress_type: NetworkAddressType, socket_type: NetworkSocketType) -> NetworkSocket` | function |
| `enet_socket_bind` | `NetworkAddress.socket_bind(...)` | `NetworkAddress.socket_bind(self, net_socket: E) -> int` | function |
| `enet_socket_get_address` | `NetworkAddress.socket_get(...)` | `NetworkAddress.socket_get(self, net_socket: E) -> int` | function |
| `enet_socket_listen` | `moss.network.socket_listen(...)` | `moss.network.socket_listen(net_socket: E, nt: I) -> int` | function |
| `enet_socket_accept` | `NetworkAddress.socket_accept(...)` | `NetworkAddress.socket_accept(self, net_socket: E) -> NetworkSocket` | function |
| `enet_socket_connect` | `NetworkAddress.socket_connect(...)` | `NetworkAddress.socket_connect(self, net_socket: E) -> int` | function |
| `enet_socket_send` | `NetworkAddress.socket_send(...)` | `NetworkAddress.socket_send(self, net_socket: E, value: NetworkBuffer, ize_t: S) -> int` | function |
| `enet_socket_receive` | `NetworkAddress.socket_receive(...)` | `NetworkAddress.socket_receive(self, net_socket: E, value: NetworkBuffer, ize_t: S) -> int` | function |
| `enet_socket_wait` | `moss.network.socket_wait(...)` | `moss.network.socket_wait(net_socket: E, value: MutableSequence[int], int32_t: U) -> int` | function |
| `enet_socket_set_option` | `moss.network.socket_set_option(...)` | `moss.network.socket_set_option(net_socket: E, net_socket_option: E, nt: I) -> int` | function |
| `enet_socket_get_option` | `moss.network.socket_get_option(...)` | `moss.network.socket_get_option(net_socket: E, net_socket_option: E, value: MutableSequence[int]) -> int` | function |
| `enet_socket_shutdown` | `moss.network.socket_shutdown(...)` | `moss.network.socket_shutdown(net_socket: E, net_socket_shutdown: E) -> int` | function |
| `enet_socket_destroy` | `moss.network.socket_destroy(...)` | `moss.network.socket_destroy(net_socket: E) -> None` | function |
| `enet_socketset_select` | `NetworkSocketSet.socketset_select(...)` | `NetworkSocketSet.socketset_select(self, net_socket: E, value: NetworkSocketSet, int32_t: U) -> int` | function |
| `enet_address_set_host_ip` | `NetworkAddress.set_host_ip(...)` | `NetworkAddress.set_host_ip(self, host_name: str) -> int` | function |
| `enet_address_set_host` | `NetworkAddress.set_host(...)` | `NetworkAddress.set_host(self, type: NetworkAddressType, host_name: str) -> int` | function |
| `enet_address_get_host_ip` | `NetworkAddress.get_host_ip(...)` | `NetworkAddress.get_host_ip(self, host_name: bytearray, name_length: int) -> int` | function |
| `enet_address_get_host` | `NetworkAddress.get_host(...)` | `NetworkAddress.get_host(self, host_name: bytearray, name_length: int) -> int` | function |
| `enet_address_equal_host` | `NetworkAddress.equal_host(...)` | `NetworkAddress.equal_host(self, second_address: NetworkAddress) -> int` | function |
| `enet_address_is_broadcast` | `NetworkAddress.is_broadcast(...)` | `NetworkAddress.is_broadcast(self) -> int` | function |
| `enet_address_build_any` | `NetworkAddress.build_any(...)` | `NetworkAddress.build_any(self, type: NetworkAddressType) -> None` | function |
| `enet_address_build_loopback` | `NetworkAddress.build_loopback(...)` | `NetworkAddress.build_loopback(self, type: NetworkAddressType) -> None` | function |
| `enet_address_convert_ipv6` | `NetworkAddress.convert_ipv6(...)` | `NetworkAddress.convert_ipv6(self) -> None` | function |
| `enet_crc32` | `NetworkBuffer.crc32(...)` | `NetworkBuffer.crc32(self, ize_t: S) -> int` | function |
| `enet_host_create` | `NetworkAddress.create_host(...)` | `NetworkAddress.create_host(self, type: NetworkAddressType, ize_t: S, ize_t: S, int32_t: U, int32_t: U) -> NetworkHost` | function |
| `enet_host_destroy` | `NetworkHost.close(...)` | `NetworkHost.close(self) -> None` | function |
| `enet_host_check_events` | `NetworkHost.check_events(...)` | `NetworkHost.check_events(self, value: NetworkEvent) -> int` | function |
| `enet_host_service` | `NetworkHost.service(...)` | `NetworkHost.service(self, value: NetworkEvent, int32_t: U) -> int` | function |
| `enet_host_flush` (overload 1) | `NetworkHost.flush(...)` | `NetworkHost.flush(self) -> None` | function |
| `enet_host_broadcast` (overload 1) | `NetworkHost.broadcast(...)` | `NetworkHost.broadcast(self, int8_t: U, value: NetworkPacket) -> None` | function |
| `enet_host_compress` | `NetworkHost.compress(...)` | `NetworkHost.compress(self, value: NetworkCompressor) -> None` | function |
| `enet_host_compress_with_range_coder` | `NetworkHost.compress_with_range_coder(...)` | `NetworkHost.compress_with_range_coder(self) -> int` | function |
| `enet_host_channel_limit` | `NetworkHost.channel_limit(...)` | `NetworkHost.channel_limit(self, ize_t: S) -> None` | function |
| `enet_host_bandwidth_limit` | `NetworkHost.bandwidth_limit(...)` | `NetworkHost.bandwidth_limit(self, int32_t: U, int32_t: U) -> None` | function |
| `enet_host_bandwidth_throttle` | `NetworkHost.bandwidth_throttle(...)` | `NetworkHost.bandwidth_throttle(self) -> None` | function |
| `enet_host_random_seed` | `moss.network.host_random_seed(...)` | `moss.network.host_random_seed() -> int` | function |
| `enet_host_random` | `NetworkHost.random(...)` | `NetworkHost.random(self) -> int` | function |
| `enet_host_connect` (overload 1) | `NetworkHost.connect(...)` | `NetworkHost.connect(self, value: NetworkAddress, ize_t: S, int32_t: U) -> NetworkPeer` | function |
| `enet_peer_send` | `NetworkPeer.send(...)` | `NetworkPeer.send(self, int8_t: U, value: NetworkPacket) -> int` | function |
| `enet_peer_ping` (overload 1) | `NetworkPeer.ping(...)` | `NetworkPeer.ping(self) -> None` | function |
| `enet_peer_ping_interval` | `NetworkPeer.ping_interval(...)` | `NetworkPeer.ping_interval(self, int32_t: U) -> None` | function |
| `enet_peer_timeout` (overload 1) | `NetworkPeer.timeout(...)` | `NetworkPeer.timeout(self, int32_t: U, int32_t: U, int32_t: U) -> None` | function |
| `enet_peer_reset` (overload 1) | `NetworkPeer.reset(...)` | `NetworkPeer.reset(self) -> None` | function |
| `enet_peer_disconnect` (overload 1) | `NetworkPeer.disconnect(...)` | `NetworkPeer.disconnect(self, int32_t: U) -> None` | function |
| `enet_peer_disconnect_now` (overload 1) | `NetworkPeer.disconnect_now(...)` | `NetworkPeer.disconnect_now(self, int32_t: U) -> None` | function |
| `enet_peer_disconnect_later` | `NetworkPeer.disconnect_later(...)` | `NetworkPeer.disconnect_later(self, int32_t: U) -> None` | function |
| `enet_peer_throttle_configure` | `NetworkPeer.throttle_configure(...)` | `NetworkPeer.throttle_configure(self, int32_t: U, int32_t: U, int32_t: U) -> None` | function |
| `enet_peer_throttle` | `NetworkPeer.throttle(...)` | `NetworkPeer.throttle(self, int32_t: U) -> int` | function |
| `enet_peer_reset_queues` | `NetworkPeer.reset_queues(...)` | `NetworkPeer.reset_queues(self) -> None` | function |
| `enet_peer_has_outgoing_commands` | `NetworkPeer.has_outgoing_commands(...)` | `NetworkPeer.has_outgoing_commands(self) -> int` | function |
| `enet_peer_setup_outgoing_command` | `NetworkOutgoingCommand.peer_setup(...)` | `NetworkOutgoingCommand.peer_setup(self, value: NetworkPeer) -> None` | function |
| `enet_peer_queue_outgoing_command` | `NetworkPeer.queue_outgoing_command(...)` | `NetworkPeer.queue_outgoing_command(self, value: NetworkProtocol, value: NetworkPacket, int32_t: U, int16_t: U) -> NetworkOutgoingCommand` | function |
| `enet_peer_queue_incoming_command` | `NetworkPeer.queue_incoming_command(...)` | `NetworkPeer.queue_incoming_command(self, value: NetworkProtocol, value: memoryview, ize_t: S, int32_t: U, int32_t: U) -> NetworkIncomingCommand` | function |
| `enet_peer_queue_acknowledgement` | `NetworkPeer.queue_acknowledgement(...)` | `NetworkPeer.queue_acknowledgement(self, value: NetworkProtocol, int16_t: U) -> NetworkAcknowledgement` | function |
| `enet_peer_dispatch_incoming_unreliable_commands` | `NetworkIncomingCommand.peer_dispatch_incoming_unreliable_commands(...)` | `NetworkIncomingCommand.peer_dispatch_incoming_unreliable_commands(self, value: NetworkPeer, value: NetworkChannel) -> None` | function |
| `enet_peer_dispatch_incoming_reliable_commands` | `NetworkIncomingCommand.peer_dispatch_incoming_reliable_commands(...)` | `NetworkIncomingCommand.peer_dispatch_incoming_reliable_commands(self, value: NetworkPeer, value: NetworkChannel) -> None` | function |
| `enet_peer_on_connect` | `NetworkPeer.on_connect(...)` | `NetworkPeer.on_connect(self) -> None` | function |
| `enet_peer_on_disconnect` | `NetworkPeer.on_disconnect(...)` | `NetworkPeer.on_disconnect(self) -> None` | function |
| `enet_packet_create` | `moss.network.create_packet(...)` | `moss.network.create_packet(value: memoryview, ize_t: S, int32_t: U) -> NetworkPacket` | function |
| `enet_packet_destroy` | `NetworkPacket.close(...)` | `NetworkPacket.close(self) -> None` | function |
| `enet_packet_resize` | `NetworkPacket.resize(...)` | `NetworkPacket.resize(self, ize_t: S) -> int` | function |
| `enet_peer_receive` | `NetworkPeer.receive(...)` | `NetworkPeer.receive(self, channel_id: bytearray) -> NetworkPacket` | function |
| `enet_range_coder_create` | `moss.network.create_range_coder(...)` | `moss.network.create_range_coder() -> memoryview` | function |
| `enet_range_coder_destroy` | `moss.network.range_coder_destroy(...)` | `moss.network.range_coder_destroy(value: memoryview) -> None` | function |
| `enet_range_coder_compress` | `NetworkBuffer.range_coder_compress(...)` | `NetworkBuffer.range_coder_compress(self, value: memoryview, ize_t: S, ize_t: S, value: bytearray, ize_t: S) -> int` | function |
| `enet_range_coder_decompress` | `moss.network.range_coder_decompress(...)` | `moss.network.range_coder_decompress(value: memoryview, value: bytes, ize_t: S, value: bytearray, ize_t: S) -> int` | function |
| `enet_protocol_command_size` | `moss.network.protocol_command_size(...)` | `moss.network.protocol_command_size(int8_t: U) -> int` | function |
| `enet_packet_get_data` | `NetworkPacket.get_data(...)` | `NetworkPacket.get_data(self) -> memoryview` | function |
| `enet_packet_get_user_data` | `NetworkPacket.get_user_data(...)` | `NetworkPacket.get_user_data(self) -> memoryview` | function |
| `enet_packet_set_user_data` | `NetworkPacket.set_user_data(...)` | `NetworkPacket.set_user_data(self) -> None` | function |
| `enet_packet_get_length` | `NetworkPacket.get_length(...)` | `NetworkPacket.get_length(self) -> int` | function |
| `enet_packet_set_acknowledge_callback` | `NetworkPacket.set_acknowledge_callback(...)` | `NetworkPacket.set_acknowledge_callback(self, net_packet_acknowledged_callback: E) -> None` | function |
| `enet_packet_set_free_callback` | `NetworkPacket.set_free_callback(...)` | `NetworkPacket.set_free_callback(self, net_packet_free_callback: E) -> None` | function |
| `enet_packet_check_references` | `NetworkPacket.check_references(...)` | `NetworkPacket.check_references(self) -> int` | function |
| `enet_packet_dispose` | `NetworkPacket.dispose(...)` | `NetworkPacket.dispose(self) -> None` | function |
| `enet_host_get_peers_count` | `NetworkHost.get_peers_count(...)` | `NetworkHost.get_peers_count(self) -> int` | function |
| `enet_host_get_packets_sent` | `NetworkHost.get_packets_sent(...)` | `NetworkHost.get_packets_sent(self) -> int` | function |
| `enet_host_get_packets_received` | `NetworkHost.get_packets_received(...)` | `NetworkHost.get_packets_received(self) -> int` | function |
| `enet_host_get_bytes_sent` | `NetworkHost.get_bytes_sent(...)` | `NetworkHost.get_bytes_sent(self) -> int` | function |
| `enet_host_get_bytes_received` | `NetworkHost.get_bytes_received(...)` | `NetworkHost.get_bytes_received(self) -> int` | function |
| `enet_host_set_max_duplicate_peers` | `NetworkHost.set_max_duplicate_peers(...)` | `NetworkHost.set_max_duplicate_peers(self, int16_t: U) -> None` | function |
| `enet_host_set_intercept_callback` | `NetworkHost.set_intercept_callback(...)` | `NetworkHost.set_intercept_callback(self, net_intercept_callback: E) -> None` | function |
| `enet_host_set_checksum_callback` | `NetworkHost.set_checksum_callback(...)` | `NetworkHost.set_checksum_callback(self, net_checksum_callback: E) -> None` | function |
| `enet_peer_get_id` | `NetworkPeer.get_id(...)` | `NetworkPeer.get_id(self) -> int` | function |
| `enet_peer_get_ip` | `NetworkPeer.get_ip(...)` | `NetworkPeer.get_ip(self, value: bytearray, ize_t: S) -> int` | function |
| `enet_peer_get_port` | `NetworkPeer.get_port(...)` | `NetworkPeer.get_port(self) -> int` | function |
| `enet_peer_get_mtu` | `NetworkPeer.get_mtu(...)` | `NetworkPeer.get_mtu(self) -> int` | function |
| `enet_peer_get_state` | `NetworkPeer.get_state(...)` | `NetworkPeer.get_state(self) -> NetworkPeerState` | function |
| `enet_peer_get_rtt` | `NetworkPeer.get_rtt(...)` | `NetworkPeer.get_rtt(self) -> int` | function |
| `enet_peer_get_last_rtt` | `NetworkPeer.get_last_rtt(...)` | `NetworkPeer.get_last_rtt(self) -> int` | function |
| `enet_peer_get_lastsendtime` | `NetworkPeer.get_lastsendtime(...)` | `NetworkPeer.get_lastsendtime(self) -> int` | function |
| `enet_peer_get_lastreceivetime` | `NetworkPeer.get_lastreceivetime(...)` | `NetworkPeer.get_lastreceivetime(self) -> int` | function |
| `enet_peer_get_packets_throttle` | `NetworkPeer.get_packets_throttle(...)` | `NetworkPeer.get_packets_throttle(self) -> float` | function |
| `enet_peer_get_data` | `NetworkPeer.get_data(...)` | `NetworkPeer.get_data(self) -> memoryview` | function |
| `enet_peer_set_data` | `NetworkPeer.set_data(...)` | `NetworkPeer.set_data(self, value: memoryview) -> None` | function |
| `enet_host_create_with_transport` (overload 1) | `NetworkAddress.host_create_with_transport(...)` | `NetworkAddress.host_create_with_transport(self, peer_count: int, channel_limit: int, incoming_bandwidth: int, outgoing_bandwidth: int, transport: NetworkTransportType) -> NetworkHost` | function |
| `enet_host_connect` (overload 2) | `NetworkHost.connect(...)` | `NetworkHost.connect(self, address: NetworkAddress, channel_count: int, data: int) -> NetworkPeer` | function |
| `enet_host_connect_ex` (overload 1) | `NetworkHost.connect_ex(...)` | `NetworkHost.connect_ex(self, endpoint: str, channel_count: int, data: int) -> NetworkPeer` | function |
| `enet_host_transport_caps` (overload 1) | `NetworkHost.transport_caps(...)` | `NetworkHost.transport_caps(self) -> int` | function |
| `enet_transport_handle_event` | `NetworkTransportEvent.transport_handle(...)` | `NetworkTransportEvent.transport_handle(self, host: NetworkHost) -> None` | function |
| `enet_peer_reset` (overload 2) | `NetworkPeer.reset(...)` | `NetworkPeer.reset(self) -> None` | function |
| `enet_peer_timeout` (overload 2) | `NetworkPeer.timeout(...)` | `NetworkPeer.timeout(self) -> None` | function |
| `enet_peer_ping` (overload 2) | `NetworkPeer.ping(...)` | `NetworkPeer.ping(self) -> None` | function |
| `enet_packet_encode` | `NetPacket.encode(...)` | `NetPacket.encode(self, out_buffer: Uint8, buffer_size: int) -> int` | function |
| `enet_packet_decode` | `Uint8.packet_decode(...)` | `Uint8.packet_decode(self, out_packet: NetPacket, buffer_size: int) -> bool` | function |
| `enet_fragment_send` | `NetworkPeer.fragment_send(...)` | `NetworkPeer.fragment_send(self, channel_id: Uint8, data: Sequence[Uint8]) -> None` | function |
| `enet_fragment_receive` | `NetworkPeer.fragment_receive(...)` | `NetworkPeer.fragment_receive(self, fragment: NetPacket, out_packet: NetPacket) -> bool` | function |
| `enet_bandwidth_update` | `NetworkHost.bandwidth_update(...)` | `NetworkHost.bandwidth_update(self, delta_time: float) -> None` | function |
| `enet_peer_send_window` | `NetworkPeer.send_window(...)` | `NetworkPeer.send_window(self) -> int` | function |
| `enet_peer_create_channel` | `NetworkPeer.create_channel(...)` | `NetworkPeer.create_channel(self, reliable: bool, ordered: bool) -> int` | function |
| `enet_peer_reset_channels` | `NetworkPeer.reset_channels(...)` | `NetworkPeer.reset_channels(self) -> None` | function |
| `enet_peer_disconnect` (overload 2) | `NetworkPeer.disconnect(...)` | `NetworkPeer.disconnect(self, reason: Uint32TT) -> None` | function |
| `enet_peer_disconnect_now` (overload 2) | `NetworkPeer.disconnect_now(...)` | `NetworkPeer.disconnect_now(self) -> None` | function |
| `enet_peer_get_stats` | `NetworkPeer.get_stats(...)` | `NetworkPeer.get_stats(self, rtt: MutableSequence[float], packet_loss: MutableSequence[float], sent: Uint32TT, received: Uint32TT) -> None` | function |
| `enet_host_flush` (overload 2) | `NetworkHost.flush(...)` | `NetworkHost.flush(self) -> None` | function |
| `enet_host_broadcast` (overload 2) | `NetworkHost.broadcast(...)` | `NetworkHost.broadcast(self, channel_id: Uint8, data: Sequence[Uint8]) -> None` | function |
| `enet_host_set_protocol_id` | `NetworkHost.set_protocol_id(...)` | `NetworkHost.set_protocol_id(self, protocol_id: Uint16TT) -> None` | function |
| `enet_host_get_protocol_id` | `NetworkHost.get_protocol_id(...)` | `NetworkHost.get_protocol_id(self) -> Uint16TT` | function |
| `enet_host_create_with_transport` (overload 2) | `NetworkAddress.host_create_with_transport(...)` | `NetworkAddress.host_create_with_transport(self, peer_count: int, channel_limit: int, incoming_bandwidth: int, outgoing_bandwidth: int, transport: NetworkTransportType) -> NetworkHost` | function |
| `enet_host_connect` (overload 3) | `NetworkHost.connect(...)` | `NetworkHost.connect(self, value: NetworkAddress, ize_t: S, int32_t: U) -> NetworkPeer` | function |
| `enet_host_connect_ex` (overload 2) | `NetworkHost.connect_ex(...)` | `NetworkHost.connect_ex(self, endpoint: str, ize_t: S, int32_t: U) -> NetworkPeer` | function |
| `enet_host_transport_caps` (overload 2) | `NetworkHost.transport_caps(...)` | `NetworkHost.transport_caps(self) -> int` | function |
| `enet_http_server_start` | `moss.network.http_server_start(...)` | `moss.network.http_server_start(bind_address: str, port: NetworkUint16T) -> NetworkHttpServer` | function |
| `enet_http_server_stop` | `NetworkHttpServer.stop(...)` | `NetworkHttpServer.stop(self) -> None` | function |
| `enet_http_register_endpoint` | `NetworkHttpServer.http_register_endpoint(...)` | `NetworkHttpServer.http_register_endpoint(self, path: str, handler: NetworkHttpEndpointHandler) -> None` | function |
| `enet_host_create_secure` | `NetworkAddress.host_create_secure(...)` | `NetworkAddress.host_create_secure(self, peer_count: int, channel_limit: int, incoming_bandwidth: int, outgoing_bandwidth: int, transport: NetworkTransportType, security: NetworkTransportSecurity) -> NetworkHost` | function |
| `enet_http_request_async` | `moss.network.http_request_async(...)` | `moss.network.http_request_async(desc: NetworkHttpRequestDesc, cb: NetworkHttpResponseCallback) -> NetworkHttpRequest` | function |
| `enet_http_request_cancel` | `NetworkHttpRequest.cancel(...)` | `NetworkHttpRequest.cancel(self) -> None` | function |
| `enet_http_request_release` | `NetworkHttpRequest.release(...)` | `NetworkHttpRequest.release(self) -> None` | function |
| `enet_host_set_connect_validator` | `NetworkHost.set_connect_validator(...)` | `NetworkHost.set_connect_validator(self, validator: NetworkConnectValidator) -> None` | function |
| `enet_transport_send_stream` | `moss.network.transport_send_stream(...)` | `moss.network.transport_send_stream(args: ...) -> int` | function |
| `enet_transport_receive_stream` | `moss.network.transport_receive_stream(...)` | `moss.network.transport_receive_stream(args: ...) -> int` | function |
| `enet_transport_ws_close` | `NetworkTransportPeer.transport_ws_close(...)` | `NetworkTransportPeer.transport_ws_close(self, code: int) -> None` | function |
| `enet_transport_webrtc_signal` | `NetworkWebRTCSignal.transport_webrtc(...)` | `NetworkWebRTCSignal.transport_webrtc(self, value: NetworkTransport, value: NetworkTransportPeer) -> None` | function |
| `enet_host_set_webrtc_signal_callback` | `NetworkHost.set_webrtc_signal_callback(...)` | `NetworkHost.set_webrtc_signal_callback(self, net_web_rtc_signal_callback: E, value: memoryview) -> None` | function |
| `enet_webrtc_handle_signal` | `NetworkWebRTCSignal.webrtc_handle(...)` | `NetworkWebRTCSignal.webrtc_handle(self, value: NetworkHost, value: NetworkPeer) -> int` | function |

### `Moss_Physics.h` → `moss.physics`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `CollideShapeResult_FreeMembers` | `CollideShapeResult.free_members(...)` | `CollideShapeResult.free_members(self) -> None` | function |
| `CollisionEstimationResult_FreeMembers` | `CollisionEstimationResult.free_members(...)` | `CollisionEstimationResult.free_members(self) -> None` | function |
| `BroadPhaseLayerInterfaceMask_Create` | `moss.physics.create_broad_phase_layer_interface_mask(...)` | `moss.physics.create_broad_phase_layer_interface_mask(num_broad_phase_layers: int) -> BroadPhaseLayerInterface` | function |
| `BroadPhaseLayerInterfaceMask_ConfigureLayer` | `BroadPhaseLayerInterface.mask_configure_layer(...)` | `BroadPhaseLayerInterface.mask_configure_layer(self, broad_phase_layer: BroadPhaseLayer, groups_to_include: int, groups_to_exclude: int) -> None` | function |
| `BroadPhaseLayerInterfaceTable_Create` | `moss.physics.create_broad_phase_layer_interface_table(...)` | `moss.physics.create_broad_phase_layer_interface_table(num_object_layers: int, num_broad_phase_layers: int) -> BroadPhaseLayerInterface` | function |
| `BroadPhaseLayerInterfaceTable_MapObjectToBroadPhaseLayer` | `BroadPhaseLayerInterface.table_map_object_to_broad_phase_layer(...)` | `BroadPhaseLayerInterface.table_map_object_to_broad_phase_layer(self, object_layer: ObjectLayer, broad_phase_layer: BroadPhaseLayer) -> None` | function |
| `ObjectLayerPairFilterMask_Create` | `moss.physics.create_object_layer_pair_filter_mask(...)` | `moss.physics.create_object_layer_pair_filter_mask() -> ObjectLayerPairFilter` | function |
| `ObjectLayerPairFilterMask_GetObjectLayer` | `moss.physics.object_layer_pair_filter_mask_get_object_layer(...)` | `moss.physics.object_layer_pair_filter_mask_get_object_layer(group: int, mask: int) -> ObjectLayer` | function |
| `ObjectLayerPairFilterMask_GetGroup` | `moss.physics.object_layer_pair_filter_mask_get_group(...)` | `moss.physics.object_layer_pair_filter_mask_get_group(layer: ObjectLayer) -> int` | function |
| `ObjectLayerPairFilterMask_GetMask` | `moss.physics.object_layer_pair_filter_mask_get_mask(...)` | `moss.physics.object_layer_pair_filter_mask_get_mask(layer: ObjectLayer) -> int` | function |
| `ObjectLayerPairFilterTable_Create` | `moss.physics.create_object_layer_pair_filter_table(...)` | `moss.physics.create_object_layer_pair_filter_table(num_object_layers: int) -> ObjectLayerPairFilter` | function |
| `ObjectLayerPairFilterTable_DisableCollision` | `ObjectLayerPairFilter.table_disable_collision(...)` | `ObjectLayerPairFilter.table_disable_collision(self, layer1: ObjectLayer, layer2: ObjectLayer) -> None` | function |
| `ObjectLayerPairFilterTable_EnableCollision` | `ObjectLayerPairFilter.table_enable_collision(...)` | `ObjectLayerPairFilter.table_enable_collision(self, layer1: ObjectLayer, layer2: ObjectLayer) -> None` | function |
| `ObjectLayerPairFilterTable_ShouldCollide` | `ObjectLayerPairFilter.table_should_collide(...)` | `ObjectLayerPairFilter.table_should_collide(self, layer1: ObjectLayer, layer2: ObjectLayer) -> bool` | function |
| `ObjectVsBroadPhaseLayerFilterMask_Create` | `BroadPhaseLayerInterface.create_object_vs_broad_phase_layer_filter_mask(...)` | `BroadPhaseLayerInterface.create_object_vs_broad_phase_layer_filter_mask(self) -> ObjectVsBroadPhaseLayerFilter` | function |
| `ObjectVsBroadPhaseLayerFilterTable_Create` | `BroadPhaseLayerInterface.create_object_vs_broad_phase_layer_filter_table(...)` | `BroadPhaseLayerInterface.create_object_vs_broad_phase_layer_filter_table(self, num_broad_phase_layers: int, object_layer_pair_filter: ObjectLayerPairFilter, num_object_layers: int) -> ObjectVsBroadPhaseLayerFilter` | function |
| `DrawSettings_InitDefault` | `DrawSettings.init_default(...)` | `DrawSettings.init_default(self) -> None` | function |
| `ContactManifold_GetWorldSpaceNormal` | `ContactManifold.get_world_space_normal(...)` | `ContactManifold.get_world_space_normal(self, result: Vec3) -> None` | function |
| `ContactManifold_GetPenetrationDepth` | `ContactManifold.get_penetration_depth(...)` | `ContactManifold.get_penetration_depth(self) -> float` | function |
| `ContactManifold_GetSubShapeID1` | `ContactManifold.get_sub_shape_id1(...)` | `ContactManifold.get_sub_shape_id1(self) -> SubShapeId` | function |
| `ContactManifold_GetSubShapeID2` | `ContactManifold.get_sub_shape_id2(...)` | `ContactManifold.get_sub_shape_id2(self) -> SubShapeId` | function |
| `ContactManifold_GetPointCount` | `ContactManifold.get_point_count(...)` | `ContactManifold.get_point_count(self) -> int` | function |
| `ContactManifold_GetWorldSpaceContactPointOn1` | `ContactManifold.get_world_space_contact_point_on1(...)` | `ContactManifold.get_world_space_contact_point_on1(self, index: int, result: Vec3) -> None` | function |
| `ContactManifold_GetWorldSpaceContactPointOn2` | `ContactManifold.get_world_space_contact_point_on2(...)` | `ContactManifold.get_world_space_contact_point_on2(self, index: int, result: Vec3) -> None` | function |
| `CollisionDispatch_CollideShapeVsShape` | `CollideShapeCollectorCallback.collision_dispatch_collide_shape_vs_shape(...)` | `CollideShapeCollectorCallback.collision_dispatch_collide_shape_vs_shape(self, shape1: Shape, shape2: Shape, scale1: Vec3, scale2: Vec3, center_of_mass_transform1: Mat44, center_of_mass_transform2: Mat44, collide_shape_settings: CollideShapeSettings, shape_filter: ShapeFilter) -> bool` | function |
| `CollisionDispatch_CastShapeVsShapeLocalSpace` | `CastShapeCollectorCallback.collision_dispatch_cast_shape_vs_shape_local_space(...)` | `CastShapeCollectorCallback.collision_dispatch_cast_shape_vs_shape_local_space(self, direction: Vec3, shape1: Shape, shape2: Shape, scale1_in_shape2_local_space: Vec3, scale2: Vec3, center_of_mass_transform1_in_shape2_local_space: Mat44, center_of_mass_world_transform2: Mat44, shape_cast_settings: ShapeCastSettings, shape_filter: ShapeFilter) -> bool` | function |
| `CollisionDispatch_CastShapeVsShapeWorldSpace` | `CastShapeCollectorCallback.collision_dispatch_cast_shape_vs_shape_world_space(...)` | `CastShapeCollectorCallback.collision_dispatch_cast_shape_vs_shape_world_space(self, direction: Vec3, shape1: Shape, shape2: Shape, scale1: Vec3, in_scale2: Vec3, center_of_mass_world_transform1: Mat44, center_of_mass_world_transform2: Mat44, shape_cast_settings: ShapeCastSettings, shape_filter: ShapeFilter) -> bool` | function |
| `Skeleton_Create` | `moss.physics.create_skeleton(...)` | `moss.physics.create_skeleton() -> Skeleton` | function |
| `Skeleton_Destroy` | `Skeleton.close(...)` | `Skeleton.close(self) -> None` | function |
| `Skeleton_AddJoint` | `Skeleton.add_joint(...)` | `Skeleton.add_joint(self, name: str) -> int` | function |
| `Skeleton_AddJoint2` | `Skeleton.add_joint2(...)` | `Skeleton.add_joint2(self, name: str, parent_index: int) -> int` | function |
| `Skeleton_AddJoint3` | `Skeleton.add_joint3(...)` | `Skeleton.add_joint3(self, name: str, parent_name: str) -> int` | function |
| `Skeleton_GetJointCount` | `Skeleton.get_joint_count(...)` | `Skeleton.get_joint_count(self) -> int` | function |
| `Skeleton_GetJoint` | `SkeletonJoint.skeleton_get(...)` | `SkeletonJoint.skeleton_get(self, skeleton: Skeleton, index: int) -> None` | function |
| `Skeleton_GetJointIndex` | `Skeleton.get_joint_index(...)` | `Skeleton.get_joint_index(self, name: str) -> int` | function |
| `Skeleton_CalculateParentJointIndices` | `Skeleton.calculate_parent_joint_indices(...)` | `Skeleton.calculate_parent_joint_indices(self) -> None` | function |
| `Skeleton_AreJointsCorrectlyOrdered` | `Skeleton.are_joints_correctly_ordered(...)` | `Skeleton.are_joints_correctly_ordered(self) -> bool` | function |
| `Indexify` | `moss.physics.indexify(...)` | `moss.physics.indexify(in_triangles: TriangleList, out_vertices: VertexList, out_triangles: IndexedTriangleList, in_vertex_weld_distance: float) -> None` | function |
| `Deindexify` | `moss.physics.deindexify(...)` | `moss.physics.deindexify(in_vertices: VertexList, in_triangles: IndexedTriangleList, out_triangles: TriangleList) -> None` | function |

### `Moss_Platform.h` → `moss.platform`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_CreateWindow` | `moss.platform.create_window(...)` | `moss.platform.create_window(*, title: str, width: int, height: int, monitor: Monitor \| None = None, share: Window \| None = None) -> Window` | function |
| `Moss_TerminateWindow` | `Window.close(...)` | `Window.close(self) -> None` | function |
| `Moss_CreateMessageBox` | `Window.show_message_box(...)` | `Window.show_message_box(self, title: str, message: str, flags: MessageBoxFlags) -> bool` | function |
| `Moss_ShouldWindowClose` | `Window.should_close` | `Window.should_close: bool` | function |
| `Moss_PollEvents` | `moss.platform.poll_events(...)` | `moss.platform.poll_events() -> None` | function |
| `Moss_GetWindowWidth` | `moss.platform.get_window_width(...)` | `moss.platform.get_window_width() -> int` | function |
| `Moss_GetWindowHeight` | `moss.platform.get_window_height(...)` | `moss.platform.get_window_height() -> int` | function |
| `Moss_SetWindowTitle` | `Window.title` | `Window.title: str` | function |
| `Moss_SetWindowIcon` | `Window.set_icon(...)` | `Window.set_icon(self, image: Image) -> None` | function |
| `Moss_CloseWindow` | `Window.close(...)` | `Window.close(self) -> None` | function |
| `Moss_MonitorGetPrimary` | `moss.platform.monitors.monitor_get_primary(...)` | `moss.platform.monitors.monitor_get_primary() -> Monitor` | function |
| `Moss_MonitorGetSecondary` | `moss.platform.monitors.monitor_get_secondary(...)` | `moss.platform.monitors.monitor_get_secondary() -> Monitor` | function |
| `Moss_MonitorGetPhysicalSize` | `Monitor.get_physical_size(...)` | `Monitor.get_physical_size(self, width_mm: MutableSequence[int], height_mm: MutableSequence[int]) -> None` | function |
| `Moss_MonitorGetContentScale` | `Monitor.get_content_scale(...)` | `Monitor.get_content_scale(self, xscale: MutableSequence[float], yscale: MutableSequence[float]) -> None` | function |
| `Moss_MonitorGetPosition` | `Monitor.get_position(...)` | `Monitor.get_position(self, x: MutableSequence[int], y: MutableSequence[int]) -> None` | function |
| `Moss_MonitorGetName` | `Monitor.get_name(...)` | `Monitor.get_name(self) -> str \| None` | function |
| `Moss_MonitorSetGammaRamp` | `GammaRamp.monitor_set(...)` | `GammaRamp.monitor_set(self, monitor: Monitor) -> None` | function |
| `Moss_MonitorGetGammaRamp` | `Monitor.get_gamma_ramp(...)` | `Monitor.get_gamma_ramp(self) -> GammaRamp` | function |
| `Moss_MonitorSetGamma` | `Monitor.set_gamma(...)` | `Monitor.set_gamma(self, gamma: float) -> None` | function |
| `Moss_IsKeyPressed` | `moss.platform.input.is_key_pressed(...)` | `moss.platform.input.is_key_pressed(key: Keyboard) -> bool` | function |
| `Moss_IsReleased` | `moss.platform.is_released(...)` | `moss.platform.is_released(key: Keyboard) -> bool` | function |
| `Moss_IsKeyJustPressed` | `moss.platform.input.is_key_just_pressed(...)` | `moss.platform.input.is_key_just_pressed(key: Keyboard) -> bool` | function |
| `Moss_IsKeyJustReleased` | `moss.platform.input.is_key_just_released(...)` | `moss.platform.input.is_key_just_released(key: Keyboard) -> bool` | function |
| `Moss_InputGetKey` | `moss.platform.input.input_get_key(...)` | `moss.platform.input.input_get_key() -> Keyboard` | function |
| `Moss_IsMousePressed` | `moss.platform.input.is_mouse_pressed(...)` | `moss.platform.input.is_mouse_pressed(button: Mouse) -> bool` | function |
| `Moss_IsMouseReleased` | `moss.platform.input.is_mouse_released(...)` | `moss.platform.input.is_mouse_released(button: Mouse) -> bool` | function |
| `Moss_IsMouseJustPressed` | `moss.platform.input.is_mouse_just_pressed(...)` | `moss.platform.input.is_mouse_just_pressed(button: Mouse) -> bool` | function |
| `Moss_IsMouseJustReleased` | `moss.platform.input.is_mouse_just_released(...)` | `moss.platform.input.is_mouse_just_released(button: Mouse) -> bool` | function |
| `Moss_InputGetMouseButton` | `moss.platform.input.input_get_mouse_button(...)` | `moss.platform.input.input_get_mouse_button() -> Mouse` | function |
| `Moss_GetMousePosition` | `moss.platform.input.mouse_position(...)` | `moss.platform.input.mouse_position() -> tuple[int, int]` | function |
| `Moss_SetMousePosition` | `moss.platform.input.set_mouse_position(...)` | `moss.platform.input.set_mouse_position(x: int, y: int) -> None` | function |
| `Moss_SetMouseVisible` | `moss.platform.input.set_mouse_visible(...)` | `moss.platform.input.set_mouse_visible(visible: bool) -> None` | function |
| `Moss_GetNumGamepads` | `moss.platform.get_num_gamepads(...)` | `moss.platform.get_num_gamepads() -> int` | function |
| `Moss_OpenGamepad` | `moss.platform.gamepads.open_gamepad(...)` | `moss.platform.gamepads.open_gamepad(id: GamepadId) -> Gamepad` | function |
| `Moss_CloseGamepad` | `Gamepad.close(...)` | `Gamepad.close(self) -> None` | function |
| `Moss_GamepadConnected` | `Gamepad.connected(...)` | `Gamepad.connected(self) -> bool` | function |
| `Moss_UpdateGamepads` | `moss.platform.update_gamepads(...)` | `moss.platform.update_gamepads() -> None` | function |
| `Moss_IsGamepadButtonPressed` | `Gamepad.is_button_pressed(...)` | `Gamepad.is_button_pressed(self, button: GamepadButton) -> bool` | function |
| `Moss_IsGamepadButtonJustPressed` | `Gamepad.is_button_just_pressed(...)` | `Gamepad.is_button_just_pressed(self, button: GamepadButton) -> bool` | function |
| `Moss_IsGamepadButtonJustReleased` | `Gamepad.is_button_just_released(...)` | `Gamepad.is_button_just_released(self, button: GamepadButton) -> bool` | function |
| `Moss_GetGamepadAxis` | `Gamepad.get_axis(...)` | `Gamepad.get_axis(self, axis: GamepadAxis) -> float` | function |
| `Moss_GetGamepadOrientation` | `Gamepad.get_orientation(...)` | `Gamepad.get_orientation(self, w: MutableSequence[float], x: MutableSequence[float], y: MutableSequence[float], z: MutableSequence[float]) -> bool` | function |
| `Moss_GetGamepadPlayerSpaceGyro` | `Gamepad.get_player_space_gyro(...)` | `Gamepad.get_player_space_gyro(self, out_x: MutableSequence[float], out_y: MutableSequence[float], yaw_relax_factor: float) -> None` | function |
| `Moss_GetGamepadWorldSpaceGyro` | `Gamepad.get_world_space_gyro(...)` | `Gamepad.get_world_space_gyro(self, out_x: MutableSequence[float], out_y: MutableSequence[float], side_reduction_threshold: float) -> None` | function |
| `Moss_ResetGamepadCalibration` | `Gamepad.reset_calibration(...)` | `Gamepad.reset_calibration(self) -> None` | function |
| `Moss_RumbleGamepad` | `Gamepad.rumble(...)` | `Gamepad.rumble(self, low: int, high: int, duration_ms: int) -> bool` | function |
| `Moss_RumbleGamepadTriggers` | `Gamepad.rumble_triggers(...)` | `Gamepad.rumble_triggers(self, left: int, right: int, duration_ms: int) -> bool` | function |
| `Moss_SetGamepadLED` | `Gamepad.set_led(...)` | `Gamepad.set_led(self, r: int, g: int, b: int) -> bool` | function |
| `Moss_GetGamepadName` | `Gamepad.get_name(...)` | `Gamepad.get_name(self) -> str \| None` | function |
| `Moss_GetGamepadID` | `Gamepad.get_id(...)` | `Gamepad.get_id(self) -> GamepadId` | function |
| `Moss_GetGamepadPlayerIndex` | `Gamepad.get_player_index(...)` | `Gamepad.get_player_index(self) -> int` | function |
| `Moss_GetGamepadPowerInfo` | `Gamepad.get_power_info(...)` | `Gamepad.get_power_info(self, percent: MutableSequence[int]) -> PowerState` | function |
| `Moss_GetNumGamepadTouchpads` | `Gamepad.get_num_touchpads(...)` | `Gamepad.get_num_touchpads(self) -> int` | function |
| `Moss_GetNumGamepadTouchpadFingers` | `Gamepad.get_num_touchpad_fingers(...)` | `Gamepad.get_num_touchpad_fingers(self) -> int` | function |
| `Moss_GetGamepadTouchpadFinger` | `Gamepad.get_touchpad_finger(...)` | `Gamepad.get_touchpad_finger(self, pad: int, finger: int, down: MutableSequence[bool], x: MutableSequence[float], y: MutableSequence[float], pressure: MutableSequence[float]) -> bool` | function |
| `Moss_GetGamepadMapping` | `Gamepad.get_mapping(...)` | `Gamepad.get_mapping(self) -> str \| None` | function |
| `Moss_SetGamepadMapping` | `Gamepad.set_mapping(...)` | `Gamepad.set_mapping(self, mapping: str) -> bool` | function |
| `Moss_ReloadGamepadMappings` | `moss.platform.gamepads.reload_gamepad_mappings(...)` | `moss.platform.gamepads.reload_gamepad_mappings() -> None` | function |
| `Moss_InputGetGamepadButton` | `moss.platform.gamepads.input_get_gamepad_button(...)` | `moss.platform.gamepads.input_get_gamepad_button() -> GamepadButton` | function |
| `Moss_InputGetGamepadAxis` | `moss.platform.gamepads.input_get_gamepad_axis(...)` | `moss.platform.gamepads.input_get_gamepad_axis() -> GamepadAxis` | function |
| `Moss_GetPenDeviceType` | `moss.platform.input.get_pen_device_type(...)` | `moss.platform.input.get_pen_device_type(instance_id: PenId) -> PenDeviceType` | function |
| `Moss_GetTouchDeviceName` | `moss.platform.input.get_touch_device_name(...)` | `moss.platform.input.get_touch_device_name(touch_id: TouchId) -> str \| None` | function |
| `Moss_GetTouchDevices` | `moss.platform.input.get_touch_devices(...)` | `moss.platform.input.get_touch_devices(count: MutableSequence[int]) -> TouchId` | function |
| `Moss_GetTouchDeviceType` | `moss.platform.input.get_touch_device_type(...)` | `moss.platform.input.get_touch_device_type(touch_id: TouchId) -> TouchDeviceType` | function |
| `Moss_GetTouchFingers` | `moss.platform.input.get_touch_fingers(...)` | `moss.platform.input.get_touch_fingers(touch_id: TouchId, count: MutableSequence[int]) -> Finger` | function |
| `Moss_GamepadHasSensor` | `Gamepad.has_sensor(...)` | `Gamepad.has_sensor(self, sensor: GamepadSensorType) -> bool` | function |
| `Moss_SetGamepadSensorEnabled` | `Gamepad.set_sensor_enabled(...)` | `Gamepad.set_sensor_enabled(self, sensor: GamepadSensorType, enabled: bool) -> bool` | function |
| `Moss_GetGamepadSensorData` | `GamepadSensorData.get(...)` | `GamepadSensorData.get(self, gp: Gamepad, sensor: GamepadSensorType) -> bool` | function |
| `Moss_OpenHaptic` | `moss.platform.haptics.open_haptic(...)` | `moss.platform.haptics.open_haptic(id: HapticId) -> Haptic` | function |
| `Moss_CloseHaptic` | `Haptic.close(...)` | `Haptic.close(self) -> None` | function |
| `Moss_CreateHapticEffect` | `Haptic.create_effect(...)` | `Haptic.create_effect(self) -> HapticId` | function |
| `Moss_DestroyHapticEffect` | `Haptic.destroy_effect(...)` | `Haptic.destroy_effect(self) -> None` | function |
| `Moss_GetHapticEffectStatus` | `Haptic.get_effect_status(...)` | `Haptic.get_effect_status(self) -> bool` | function |
| `Moss_GetHapticFeatures` | `Haptic.get_features(...)` | `Haptic.get_features(self) -> int` | function |
| `Moss_GetHapticFromID` | `Haptic.get_from_id(...)` | `Haptic.get_from_id(self) -> Haptic` | function |
| `Moss_GetHapticID` | `Haptic.get_id(...)` | `Haptic.get_id(self) -> HapticId` | function |
| `Moss_GetHapticName` | `Haptic.get_name(...)` | `Haptic.get_name(self) -> str \| None` | function |
| `Moss_GetHapticNameForID` | `Haptic.get_name_for_id(...)` | `Haptic.get_name_for_id(self) -> str \| None` | function |
| `Moss_GetHaptics` | `Haptic.get_haptics(...)` | `Haptic.get_haptics(self) -> HapticId` | function |
| `Moss_GetMaxHapticEffects` | `Haptic.get_max_effects(...)` | `Haptic.get_max_effects(self) -> int` | function |
| `Moss_GetMaxHapticEffectsPlaying` | `Haptic.get_max_effects_playing(...)` | `Haptic.get_max_effects_playing(self) -> int` | function |
| `Moss_GetNumHapticAxes` | `Haptic.get_num_axes(...)` | `Haptic.get_num_axes(self) -> int` | function |
| `Moss_HapticEffectSupported` | `Haptic.effect_supported(...)` | `Haptic.effect_supported(self) -> bool` | function |
| `Moss_HapticRumbleSupported` | `Haptic.rumble_supported(...)` | `Haptic.rumble_supported(self) -> bool` | function |
| `Moss_InitHapticRumble` | `Haptic.init_rumble(...)` | `Haptic.init_rumble(self) -> bool` | function |
| `Moss_IsJoystickHaptic` | `Gamepad.is_joystick_haptic(...)` | `Gamepad.is_joystick_haptic(self) -> bool` | function |
| `Moss_IsMouseHaptic` | `moss.platform.haptics.is_mouse_haptic(...)` | `moss.platform.haptics.is_mouse_haptic() -> bool` | function |
| `Moss_OpenHapticFromJoystick` | `Gamepad.open_haptic_from_joystick(...)` | `Gamepad.open_haptic_from_joystick(self) -> Haptic` | function |
| `Moss_OpenHapticFromMouse` | `moss.platform.haptics.open_haptic_from_mouse(...)` | `moss.platform.haptics.open_haptic_from_mouse() -> Haptic` | function |
| `Moss_PauseHaptic` | `Haptic.pause(...)` | `Haptic.pause(self) -> bool` | function |
| `Moss_PlayHapticRumble` | `Haptic.play_rumble(...)` | `Haptic.play_rumble(self, strength: float, length: int) -> bool` | function |
| `Moss_ResumeHaptic` | `Haptic.resume(...)` | `Haptic.resume(self) -> bool` | function |
| `Moss_RunHapticEffect` | `Haptic.run_effect(...)` | `Haptic.run_effect(self, iterations: int) -> bool` | function |
| `Moss_SetHapticAutocenter` | `Haptic.set_autocenter(...)` | `Haptic.set_autocenter(self, center: int) -> bool` | function |
| `Moss_SetHapticGain` | `Haptic.set_gain(...)` | `Haptic.set_gain(self, gain: int) -> bool` | function |
| `Moss_StopHapticEffect` | `Haptic.stop_effect(...)` | `Haptic.stop_effect(self, effect: HapticEffectId) -> bool` | function |
| `Moss_StopHapticEffects` | `Haptic.stop_effects(...)` | `Haptic.stop_effects(self) -> bool` | function |
| `Moss_StopHapticRumble` | `Haptic.stop_rumble(...)` | `Haptic.stop_rumble(self) -> bool` | function |
| `Moss_UpdateHapticEffect` | `HapticEffect.update(...)` | `HapticEffect.update(self, haptic: Haptic, effect: HapticEffectId) -> bool` | function |
| `Moss_GetAvailableCPUCores` | `moss.platform.get_available_cpu_cores(...)` | `moss.platform.get_available_cpu_cores() -> int` | function |
| `Moss_GetCPUCacheLineSize` | `moss.platform.get_cpu_cache_line_size(...)` | `moss.platform.get_cpu_cache_line_size() -> int` | function |
| `Moss_GetSystemRAM` | `moss.platform.get_system_ram(...)` | `moss.platform.get_system_ram() -> int` | function |
| `Moss_OpenURL` | `moss.platform.open_url(...)` | `moss.platform.open_url(url: str) -> bool` | function |
| `Moss_GetLocale` | `moss.platform.get_locale(...)` | `moss.platform.get_locale() -> Locale` | function |
| `Moss_GetPowerInfo` | `moss.platform.get_power_info(...)` | `moss.platform.get_power_info(seconds: MutableSequence[int], percent: MutableSequence[int]) -> PowerState` | function |
| `Moss_IsProcessRunningByName` | `moss.platform.is_process_running_by_name(...)` | `moss.platform.is_process_running_by_name(executable_path: str) -> bool` | function |
| `Moss_LoadDynamicLibrary` | `moss.platform.load_dynamic_library(...)` | `moss.platform.load_dynamic_library(lib_path: str) -> memoryview` | function |
| `Moss_GetLibrarySymbol` | `moss.platform.get_library_symbol(...)` | `moss.platform.get_library_symbol(handle: memoryview, symbol_name: str) -> memoryview` | function |
| `Moss_UnloadDynamicLibrary` | `moss.platform.unload_dynamic_library(...)` | `moss.platform.unload_dynamic_library(handle: memoryview) -> None` | function |
| `Moss_SetFramebufferResizeCallback` | `moss.platform.set_framebuffer_resize_callback(...)` | `moss.platform.set_framebuffer_resize_callback(callback: FramebufferResizeCallback) -> None` | function |
| `Moss_SetWindowSizeCallback` | `moss.platform.set_window_size_callback(...)` | `moss.platform.set_window_size_callback(callback: WindowSizeCallback) -> None` | function |
| `Moss_SetWindowResizeCallback` | `moss.platform.set_window_resize_callback(...)` | `moss.platform.set_window_resize_callback(callback: WindowResizeCallback) -> None` | function |
| `Moss_SetWindowPositionCallback` | `moss.platform.set_window_position_callback(...)` | `moss.platform.set_window_position_callback(callback: WindowPositionCallback) -> None` | function |
| `Moss_SetWindowFocusCallback` | `moss.platform.set_window_focus_callback(...)` | `moss.platform.set_window_focus_callback(callback: WindowFocusCallback) -> None` | function |
| `Moss_SetWindowContentScaleCallback` | `moss.platform.set_window_content_scale_callback(...)` | `moss.platform.set_window_content_scale_callback(callback: WindowContentScaleCallback) -> None` | function |
| `Moss_SetMonitorCallback` | `moss.platform.monitors.set_monitor_callback(...)` | `moss.platform.monitors.set_monitor_callback(callback: MonitorCallback) -> None` | function |
| `Moss_GetCameras` | `moss.platform.get_cameras(...)` | `moss.platform.get_cameras(count: MutableSequence[int]) -> CameraId` | function |
| `Moss_GetCameraName` | `moss.platform.cameras.get_camera_name(...)` | `moss.platform.cameras.get_camera_name(camera_id: CameraId) -> str \| None` | function |
| `Moss_GetCameraPosition` | `moss.platform.cameras.get_camera_position(...)` | `moss.platform.cameras.get_camera_position(camera_id: CameraId) -> CameraPosition` | function |
| `Moss_GetCurrentCameraDriver` | `moss.platform.cameras.get_current_camera_driver(...)` | `moss.platform.cameras.get_current_camera_driver() -> str \| None` | function |
| `Moss_GetNumCameraDrivers` | `moss.platform.cameras.get_num_camera_drivers(...)` | `moss.platform.cameras.get_num_camera_drivers() -> int` | function |
| `Moss_GetCameraSupportedFormats` | `moss.platform.cameras.get_camera_supported_formats(...)` | `moss.platform.cameras.get_camera_supported_formats(camera_id: CameraId, count: MutableSequence[int]) -> CameraSpec` | function |
| `Moss_AcquireCameraFrame` | `Capture.acquire_camera_frame(...)` | `Capture.acquire_camera_frame(self, timestamp_ns: MutableSequence[int]) -> Surface` | function |
| `Moss_ReleaseCameraFrame` | `Capture.release_camera_frame(...)` | `Capture.release_camera_frame(self, frame: Surface) -> None` | function |
| `Moss_GetCameraFormat` | `Capture.get_camera_format(...)` | `Capture.get_camera_format(self, out_spec: CameraSpec) -> bool` | function |
| `Moss_GetCameraPermissionState` | `Capture.get_camera_permission_state(...)` | `Capture.get_camera_permission_state(self) -> CameraPermissionState` | function |
| `Moss_GetCameraProperties` (overload 1) | `Capture.get_camera_properties(...)` | `Capture.get_camera_properties(self) -> PropertiesId` | function |
| `Moss_CloseCamera` | `Capture.close_camera(...)` | `Capture.close_camera(self) -> None` | function |
| `Moss_GetCameraID` | `Capture.get_camera_id(...)` | `Capture.get_camera_id(self) -> CameraId` | function |
| `Moss_GetCameraProperties` (overload 2) | `Capture.get_camera_properties(...)` | `Capture.get_camera_properties(self) -> PropertiesId` | function |
| `Moss_OpenCapture` | `CameraSpec.open_capture(...)` | `CameraSpec.open_capture(self, id: CameraId) -> Capture` | function |
| `Moss_CopyFile` | `moss.platform.storage.copy_file(...)` | `moss.platform.storage.copy_file(src_path: str, dst_path: str, overwrite: bool) -> bool` | function |
| `Moss_CreateDirectory` | `moss.platform.storage.create_directory(...)` | `moss.platform.storage.create_directory(path: str, recursive: bool) -> bool` | function |
| `Moss_RemovePath` | `moss.platform.storage.remove_path(...)` | `moss.platform.storage.remove_path(path: str, recursive: bool) -> bool` | function |
| `Moss_RenamePath` | `moss.platform.storage.rename_path(...)` | `moss.platform.storage.rename_path(old_path: str, new_path: str, overwrite: bool) -> bool` | function |
| `Moss_GetPathInfo` | `moss.platform.storage.get_path_info(...)` | `moss.platform.storage.get_path_info(path: str, out_info: PathInfo) -> bool` | function |
| `Moss_GetCurrentDirectory` | `moss.platform.storage.get_current_directory(...)` | `moss.platform.storage.get_current_directory(out_path: bytearray, max_len: int) -> bool` | function |
| `Moss_GetBasePath` | `moss.platform.storage.get_base_path(...)` | `moss.platform.storage.get_base_path(out_path: bytearray, max_len: int) -> bool` | function |
| `Moss_GetUserFolder` | `moss.platform.get_user_folder(...)` | `moss.platform.get_user_folder(folder: UserFolder, out_path: bytearray, max_len: int) -> bool` | function |
| `Moss_GetPrefPath` | `moss.platform.storage.get_pref_path(...)` | `moss.platform.storage.get_pref_path(org_name: str, app_name: str, out_path: bytearray, max_len: int) -> bool` | function |
| `Moss_EnumerateDirectory` | `moss.platform.storage.enumerate_directory(...)` | `moss.platform.storage.enumerate_directory(path: str, recursive: bool, callback: DirectoryIterateFn) -> bool` | function |
| `Moss_GlobDirectory` | `moss.platform.storage.glob_directory(...)` | `moss.platform.storage.glob_directory(pattern: str, callback: DirectoryIterateFn) -> bool` | function |
| `Moss_ShowFileDialogWithProperties` | `DialogFileFilter.show_file_dialog_with_properties(...)` | `DialogFileFilter.show_file_dialog_with_properties(self, callback: DialogFileCallback, window: Window, nfilters: int, default_location: str) -> None` | function |
| `Moss_ShowOpenFolderDialog` | `DialogFileFilter.show_open_folder_dialog(...)` | `DialogFileFilter.show_open_folder_dialog(self, callback: DialogFileCallback, window: Window, nfilters: int, default_location: str, allow_many: bool) -> None` | function |
| `Moss_ShowOpenFileDialog` | `Window.show_open_file_dialog(...)` | `Window.show_open_file_dialog(self, callback: DialogFileCallback, default_location: str, allow_many: bool) -> None` | function |
| `Moss_ShowSaveFileDialog` | `moss.platform.dialogs.show_save_file_dialog(...)` | `moss.platform.dialogs.show_save_file_dialog(type: FileDialogType, callback: DialogFileCallback, props: PropertiesId) -> None` | function |
| `Moss_CloseStorage` | `Storage.close(...)` | `Storage.close(self) -> bool` | function |
| `Moss_CopyStorageFile` | `Storage.copy_file(...)` | `Storage.copy_file(self, oldpath: str, newpath: str) -> bool` | function |
| `Moss_CreateStorageDirectory` | `Storage.create_directory(...)` | `Storage.create_directory(self, path: str) -> bool` | function |
| `Moss_EnumerateStorageDirectory` | `Storage.enumerate_directory(...)` | `Storage.enumerate_directory(self, path: str, callback: EnumerateDirectoryCallback) -> bool` | function |
| `Moss_GetStorageFileSize` | `Storage.get_file_size(...)` | `Storage.get_file_size(self, path: str, length: Uint64) -> bool` | function |
| `Moss_GetStorageSpaceRemaining` | `Storage.get_space_remaining(...)` | `Storage.get_space_remaining(self) -> int` | function |
| `Moss_GetStoragePathInfo` | `Storage.get_path_info(...)` | `Storage.get_path_info(self, path: str, info: PathInfo) -> bool` | function |
| `Moss_GlobStorageDirectory` | `Storage.glob_directory(...)` | `Storage.glob_directory(self, path: str, pattern: str, flags: GlobFlags, count: MutableSequence[int]) -> str \| None` | function |
| `Moss_OpenFileStorage` | `moss.platform.storage.open_file_storage(...)` | `moss.platform.storage.open_file_storage(path: str) -> Storage` | function |
| `Moss_OpenStorage` | `Storage.open(...)` | `Storage.open(self) -> Storage` | function |
| `Moss_OpenTitleStorage` | `moss.platform.storage.open_title_storage(...)` | `moss.platform.storage.open_title_storage(override: str, props: PropertiesId) -> Storage` | function |
| `Moss_OpenUserStorage` | `moss.platform.storage.open_user_storage(...)` | `moss.platform.storage.open_user_storage(org: str, app: str, props: PropertiesId) -> Storage` | function |
| `Moss_ReadStorageFile` | `Storage.read_file(...)` | `Storage.read_file(self, path: str, destination: memoryview, length: Uint64) -> bool` | function |
| `Moss_RemoveStoragePath` | `Storage.remove_path(...)` | `Storage.remove_path(self, path: str) -> bool` | function |
| `Moss_RenameStoragePath` | `Storage.rename_path(...)` | `Storage.rename_path(self, oldpath: str, newpath: str) -> bool` | function |
| `Moss_StorageReady` | `Storage.ready(...)` | `Storage.ready(self) -> bool` | function |
| `Moss_WriteStorageFile` | `Storage.write_file(...)` | `Storage.write_file(self, path: str, source: memoryview, length: Uint64) -> bool` | function |
| `Moss_CreateEmbeddedWindow` | `moss.platform.create_embedded_window(...)` | `moss.platform.create_embedded_window() -> Window` | function |
| `Moss_Terminate_EmbeddedWindow` | `moss.platform.terminate_embedded_window(...)` | `moss.platform.terminate_embedded_window() -> None` | function |
| `Moss_EmbeddedWindow_OnOrientationChanged` | `moss.platform.embedded_window_on_orientation_changed(...)` | `moss.platform.embedded_window_on_orientation_changed(focused: bool) -> None` | function |
| `Moss_EmbeddedWindow_OnResume` | `moss.platform.embedded_window_on_resume(...)` | `moss.platform.embedded_window_on_resume() -> None` | function |
| `Moss_EmbeddedWindow_OnPause` | `moss.platform.embedded_window_on_pause(...)` | `moss.platform.embedded_window_on_pause() -> None` | function |
| `Moss_MakeContextCurrent` | `Window.make_context_current(...)` | `Window.make_context_current(self) -> None` | function |
| `Moss_SwapBuffers` | `moss.platform.swap_buffers(...)` | `moss.platform.swap_buffers() -> None` | function |
| `Moss_SwapBuffersInterval` | `moss.platform.swap_buffers_interval(...)` | `moss.platform.swap_buffers_interval(interval: int) -> None` | function |
| `Moss_GetProcAddress` | `moss.platform.get_proc_address(...)` | `moss.platform.get_proc_address(procname: str) -> memoryview` | function |
| `Moss_CreateWindowSurface` | `Window.create_vulkan_surface(...)` | `Window.create_vulkan_surface(self, instance: VkInstance, allocator: VkAllocationCallbacks \| None = None) -> VkSurfaceKHR` | function |
| `Moss_VulkanSupported` | `moss.platform.supported(...)` | `moss.platform.supported() -> int` | function |
| `Moss_GetRequiredInstanceExtensions` | `moss.platform.get_required_instance_extensions(...)` | `moss.platform.get_required_instance_extensions(count: MutableSequence[int]) -> str \| None` | function |
| `Moss_GetInstanceProcAddress` | `moss.platform.get_instance_proc_address(...)` | `moss.platform.get_instance_proc_address(instance: VkInstance, procname: str) -> memoryview` | function |
| `Moss_GetPhysicalDevicePresentationSupport` | `Window.get_physical_device_presentation_support(...)` | `Window.get_physical_device_presentation_support(self, device: VkPhysicalDevice, queuefamily: int) -> int` | function |
| `Moss_Metal_CreateView` | `Window.create_view(...)` | `Window.create_view(self) -> MetalView` | function |
| `Moss_Metal_DestroyView` | `moss.platform.destroy_view(...)` | `moss.platform.destroy_view(view: MetalView) -> None` | function |
| `Moss_Metal_GetLayer` | `moss.platform.get_layer(...)` | `moss.platform.get_layer(view: MetalView) -> memoryview` | function |
| `Moss_Metal_Resize` | `moss.platform.resize(...)` | `moss.platform.resize(handle: MetalView, width: int, height: int) -> None` | function |

### `Moss_Renderer.h` → `moss.renderer`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_RendererCreateMaterial` | `Renderer.create_material(...)` | `Renderer.create_material(self, desc: MaterialDesc) -> Material` | function |
| `Moss_RendererDestroyMaterial` | `Renderer.destroy_material(...)` | `Renderer.destroy_material(self, material: Material) -> None` | function |
| `Moss_MaterialSetUniform` | `Material.set_uniform(...)` | `Material.set_uniform(self, name: str, data: memoryview, size: int) -> int` | function |
| `Moss_MaterialSetTexture` | `Material.set_texture(...)` | `Material.set_texture(self, type: MaterialTextureType, texture: Texture, sampler: GpuSampler) -> int` | function |
| `Moss_RendererCreatePipeline` | `Renderer.create_pipeline(...)` | `Renderer.create_pipeline(self, desc: PipelineDesc) -> PipelineState` | function |
| `Moss_PipelineBind` | `moss.renderer.pipeline_bind(...)` | `moss.renderer.pipeline_bind(pipeline: PipelineState) -> None` | function |
| `Moss_PipelineUnbind` | `moss.renderer.pipeline_unbind(...)` | `moss.renderer.pipeline_unbind(pipeline: PipelineState) -> None` | function |
| `Moss_PipelineSetUniform` | `moss.renderer.pipeline_set_uniform(...)` | `moss.renderer.pipeline_set_uniform(pipeline: PipelineState, name: str, data: memoryview, size: int) -> None` | function |
| `Moss_PipelineFlush` | `moss.renderer.pipeline_flush(...)` | `moss.renderer.pipeline_flush(pipeline: PipelineState) -> None` | function |
| `Moss_PipelineValidate` | `moss.renderer.pipeline_validate(...)` | `moss.renderer.pipeline_validate(pipeline: PipelineState) -> int` | function |
| `Moss_RendererCreateResourceSet` | `Renderer.create_resource_set(...)` | `Renderer.create_resource_set(self, desc: ResourceSetLayoutDesc) -> ResourceSet` | function |
| `Moss_RendererDestroyResourceSet` | `Renderer.destroy_resource_set(...)` | `Renderer.destroy_resource_set(self, set: ResourceSet) -> None` | function |
| `Moss_ResourceSetBindUniformBuffer` | `ResourceSet.bind_uniform_buffer(...)` | `ResourceSet.bind_uniform_buffer(self, binding: int, buffer: GpuBuffer) -> None` | function |
| `Moss_ResourceSetBindUniformBufferRange` | `ResourceSet.bind_uniform_buffer_range(...)` | `ResourceSet.bind_uniform_buffer_range(self, binding: int, buffer: GpuBuffer, offset: int, size: int) -> None` | function |
| `Moss_ResourceSetBindStorageBuffer` | `ResourceSet.bind_storage_buffer(...)` | `ResourceSet.bind_storage_buffer(self, binding: int, buffer: GpuBuffer) -> None` | function |
| `Moss_ResourceSetBindTexture` | `ResourceSet.bind_texture(...)` | `ResourceSet.bind_texture(self, binding: int, texture: Texture) -> None` | function |
| `Moss_ResourceSetBindSampler` | `ResourceSet.bind_sampler(...)` | `ResourceSet.bind_sampler(self, binding: int, sampler: GpuSampler) -> None` | function |
| `Moss_ResourceSetBind` | `Renderer.resource_set_bind(...)` | `Renderer.resource_set_bind(self, pipeline: PipelineState, set_index: int, set: ResourceSet) -> None` | function |
| `Moss_RendererCreateFramebuffer` | `Renderer.create_framebuffer(...)` | `Renderer.create_framebuffer(self, desc: FramebufferDesc) -> Framebuffer` | function |
| `Moss_RendererDestroyFramebuffer` | `Renderer.destroy_framebuffer(...)` | `Renderer.destroy_framebuffer(self, framebuffer: Framebuffer) -> None` | function |
| `Moss_FramebufferResize` | `Framebuffer.resize(...)` | `Framebuffer.resize(self, width: int, height: int) -> None` | function |
| `Moss_PostProcessExecute` | `Renderer.post_process_execute(...)` | `Renderer.post_process_execute(self, pass: PostProcessPass) -> None` | function |
| `Moss_FramebufferBegin` | `Renderer.framebuffer_begin(...)` | `Renderer.framebuffer_begin(self, framebuffer: Framebuffer) -> None` | function |
| `Moss_FramebufferEnd` | `Renderer.framebuffer_end(...)` | `Renderer.framebuffer_end(self, framebuffer: Framebuffer) -> None` | function |
| `Moss_TextureBarrier` | `Renderer.texture_barrier(...)` | `Renderer.texture_barrier(self, texture: Texture, old_state: ResourceState, new_state: ResourceState) -> None` | function |
| `Moss_RendererGetBackbuffer` | `Renderer.get_backbuffer(...)` | `Renderer.get_backbuffer(self) -> Framebuffer` | function |
| `Moss_RenderSubViewport` | `SubViewport.render(...)` | `SubViewport.render(self, cmd: GpuCommandBuffer, record: SubViewportRecordFn) -> None` | function |
| `Moss_RendererCreate` | `moss.renderer.create(...)` | `moss.renderer.create(desc: RendererDesc) -> Renderer` | function |
| `Moss_CreateRenderer` | `moss.renderer.create_renderer(...)` | `moss.renderer.create_renderer(window: Window) -> Renderer` | function |
| `Moss_RendererDestroy` | `Renderer.close(...)` | `Renderer.close(self) -> None` | function |
| `Moss_TerminateRenderer` | `Renderer.close(...)` | `Renderer.close(self) -> None` | function |
| `Moss_RendererBeginFrame` | `Renderer.begin_frame(...)` | `Renderer.begin_frame(self) -> None` | function |
| `Moss_RendererBeginFrameEx` | `Renderer.begin_frame_ex(...)` | `Renderer.begin_frame_ex(self, desc: RenderFrameDesc) -> None` | function |
| `Moss_RendererEndFrame` | `Renderer.end_frame(...)` | `Renderer.end_frame(self) -> None` | function |
| `Moss_RendererGetGPUDevice` | `Renderer.get_gpu_device(...)` | `Renderer.get_gpu_device(self) -> GpuDevice` | function |
| `Moss_MeshLoadFBX` | `moss.renderer.mesh_load_fbx(...)` | `moss.renderer.mesh_load_fbx(file_path: str) -> Mesh` | function |
| `Moss_MeshLoadOBJ` | `moss.renderer.mesh_load_obj(...)` | `moss.renderer.mesh_load_obj(file_path: str) -> Mesh` | function |
| `Moss_MeshLoadGLB` | `moss.renderer.mesh_load_glb(...)` | `moss.renderer.mesh_load_glb(file_path: str) -> Mesh` | function |
| `Moss_ModelLoadFBX` | `moss.renderer.model_load_fbx(...)` | `moss.renderer.model_load_fbx(file_path: str) -> Model` | function |
| `Moss_ModelLoadOBJ` | `moss.renderer.model_load_obj(...)` | `moss.renderer.model_load_obj(file_path: str) -> Model` | function |
| `Moss_ModelLoadGLB` | `moss.renderer.model_load_glb(...)` | `moss.renderer.model_load_glb(file_path: str) -> Model` | function |
| `Moss_ModelLoad` | `moss.renderer.model_load(...)` | `moss.renderer.model_load(file_path: str) -> Model` | function |
| `Moss_ModelGetLastLoadError` | `moss.renderer.model_get_last_load_error(...)` | `moss.renderer.model_get_last_load_error() -> ModelLoadError` | function |
| `Moss_ModelGetLastLoadErrorMessage` | `moss.renderer.model_get_last_load_error_message(...)` | `moss.renderer.model_get_last_load_error_message() -> str \| None` | function |
| `Moss_ModelFindBone` | `Model.find_bone(...)` | `Model.find_bone(self, name: str) -> int` | function |
| `Moss_ModelFindMorphTarget` | `Model.find_morph_target(...)` | `Model.find_morph_target(self, name: str) -> int` | function |
| `Moss_ModelSetBoneTransform` | `Model.set_bone_transform(...)` | `Model.set_bone_transform(self, bone_index: int, transform16: Sequence[float]) -> bool` | function |
| `Moss_ModelSetMorphWeight` | `Model.set_morph_weight(...)` | `Model.set_morph_weight(self, morph_index: int, weight: float) -> bool` | function |
| `Moss_RendererDrawMesh` | `Renderer.draw_mesh(...)` | `Renderer.draw_mesh(self, desc: DrawMeshDesc) -> None` | function |
| `Moss_RendererDrawModelDesc` | `Renderer.draw_model_desc(...)` | `Renderer.draw_model_desc(self, desc: DrawModelDesc) -> None` | function |
| `Moss_RendererDrawModel` | `Renderer.draw_model(...)` | `Renderer.draw_model(self, model: Model, transform: Sequence[float]) -> None` | function |
| `Moss_RendererCreateFont` | `Renderer.create_font(...)` | `Renderer.create_font(self, desc: FontDesc) -> Font` | function |
| `Moss_RendererDestroyFont` | `Renderer.destroy_font(...)` | `Renderer.destroy_font(self, font: Font) -> None` | function |
| `Moss_FontLoad` | `moss.renderer.font_load(...)` | `moss.renderer.font_load(path: str, pixel_size: float) -> Font` | function |
| `Moss_FontCreateFromMemory` | `moss.renderer.font_create_from_memory(...)` | `moss.renderer.font_create_from_memory(data: memoryview, data_size: int, pixel_size: float) -> Font` | function |
| `Moss_FontDestroy` | `Font.close(...)` | `Font.close(self) -> None` | function |
| `Moss_FontGetMetrics` | `Font.get_metrics(...)` | `Font.get_metrics(self) -> FontMetrics` | function |
| `Moss_FontGetGlyph` | `Font.get_glyph(...)` | `Font.get_glyph(self, codepoint: int) -> FontGlyph` | function |
| `Moss_FontGetAtlasPixels` | `Font.get_atlas_pixels(...)` | `Font.get_atlas_pixels(self, width: MutableSequence[int], height: MutableSequence[int], stride: MutableSequence[int]) -> UnsignedChar` | function |
| `Moss_FontGetAtlasTexture` | `Font.get_atlas_texture(...)` | `Font.get_atlas_texture(self) -> Texture` | function |
| `Moss_FontUploadAtlas` | `Renderer.font_upload_atlas(...)` | `Renderer.font_upload_atlas(self, font: Font) -> bool` | function |
| `Moss_FontMeasureText` | `Font.measure_text(...)` | `Font.measure_text(self, text: str, scale: float) -> TextMeasure` | function |
| `Moss_FontBuildTextQuads` | `moss.renderer.font_build_text_quads(...)` | `moss.renderer.font_build_text_quads(desc: DrawTextDesc, out_vertices: FontVertex, vertex_capacity: int) -> int` | function |
| `Moss_RendererDrawText` | `Renderer.draw_text(...)` | `Renderer.draw_text(self, desc: DrawTextDesc) -> None` | function |
| `Moss_RendererGetFontBackendMask` | `moss.renderer.get_font_backend_mask(...)` | `moss.renderer.get_font_backend_mask() -> int` | function |
| `Moss_RendererBackendSupportsFonts` | `moss.renderer.backend_supports_fonts(...)` | `moss.renderer.backend_supports_fonts(backend: GpuBackendType) -> bool` | function |
| `Moss_RendererDrawSprite` | `Renderer.draw_sprite(...)` | `Renderer.draw_sprite(self, desc: SpriteDesc) -> None` | function |
| `Moss_RendererCreateSpriteBatch` | `Renderer.create_sprite_batch(...)` | `Renderer.create_sprite_batch(self, capacity: int) -> SpriteBatch` | function |
| `Moss_RendererDestroySpriteBatch` | `Renderer.destroy_sprite_batch(...)` | `Renderer.destroy_sprite_batch(self, batch: SpriteBatch) -> None` | function |
| `Moss_SpriteBatchClear` | `SpriteBatch.clear(...)` | `SpriteBatch.clear(self) -> None` | function |
| `Moss_SpriteBatchAdd` | `SpriteBatch.add(...)` | `SpriteBatch.add(self, desc: SpriteDesc) -> int` | function |
| `Moss_RendererDrawSpriteBatch` | `Renderer.draw_sprite_batch(...)` | `Renderer.draw_sprite_batch(self, batch: SpriteBatch) -> None` | function |
| `Moss_RendererCreateDebugDrawList` | `Renderer.create_debug_draw_list(...)` | `Renderer.create_debug_draw_list(self, capacity: int) -> DebugDrawList` | function |
| `Moss_RendererDestroyDebugDrawList` | `Renderer.destroy_debug_draw_list(...)` | `Renderer.destroy_debug_draw_list(self, list: DebugDrawList) -> None` | function |
| `Moss_DebugDrawListClear` | `DebugDrawList.clear(...)` | `DebugDrawList.clear(self) -> None` | function |
| `Moss_DebugDrawLine` | `DebugDrawList.debug_draw_line(...)` | `DebugDrawList.debug_draw_line(self, desc: DebugLineDesc) -> int` | function |
| `Moss_DebugDrawBox` | `DebugDrawList.debug_draw_box(...)` | `DebugDrawList.debug_draw_box(self, desc: DebugBoxDesc) -> int` | function |
| `Moss_DebugDrawSphere` | `DebugDrawList.debug_draw_sphere(...)` | `DebugDrawList.debug_draw_sphere(self, desc: DebugSphereDesc) -> int` | function |
| `Moss_DebugDrawNavmesh` | `DebugDrawList.debug_draw_navmesh(...)` | `DebugDrawList.debug_draw_navmesh(self, desc: DebugNavmeshDesc) -> int` | function |
| `Moss_DebugDrawPhysicsContact` | `DebugDrawList.debug_draw_physics_contact(...)` | `DebugDrawList.debug_draw_physics_contact(self, desc: DebugPhysicsContactDesc) -> int` | function |
| `Moss_RendererDrawDebugList` | `Renderer.draw_debug_list(...)` | `Renderer.draw_debug_list(self, list: DebugDrawList) -> None` | function |
| `Moss_CameraCreate2D` | `moss.renderer.camera_create_dimension2_d(...)` | `moss.renderer.camera_create_dimension2_d(desc: CameraDIMENSION2DDesc) -> Camera` | function |
| `Moss_CameraCreate3D` | `moss.renderer.camera_create_dimension3_d(...)` | `moss.renderer.camera_create_dimension3_d(desc: CameraDIMENSION3DDesc) -> Camera` | function |
| `Moss_CameraCreateEditor` | `moss.renderer.camera_create_editor(...)` | `moss.renderer.camera_create_editor(desc: EditorCameraDesc) -> Camera` | function |
| `Moss_CameraDestroy` | `Camera.close(...)` | `Camera.close(self) -> None` | function |
| `Moss_RendererSetCamera` | `Renderer.set_camera(...)` | `Renderer.set_camera(self, camera: Camera) -> None` | function |
| `Moss_RendererCreateShadowMap` | `Renderer.create_shadow_map(...)` | `Renderer.create_shadow_map(self, desc: ShadowMapDesc) -> ShadowMap` | function |
| `Moss_RendererDestroyShadowMap` | `Renderer.destroy_shadow_map(...)` | `Renderer.destroy_shadow_map(self, shadow_map: ShadowMap) -> None` | function |
| `Moss_RendererBeginShadowPass` | `Renderer.begin_shadow_pass(...)` | `Renderer.begin_shadow_pass(self, shadow_map: ShadowMap) -> None` | function |
| `Moss_RendererEndShadowPass` | `Renderer.end_shadow_pass(...)` | `Renderer.end_shadow_pass(self, shadow_map: ShadowMap) -> None` | function |
| `Moss_MeshDraw` | `Mesh.draw(...)` | `Mesh.draw(self) -> None` | function |
| `Moss_ModelDraw` | `Model.draw(...)` | `Model.draw(self) -> None` | function |
| `Moss_MeshRemove` | `Mesh.close(...)` | `Mesh.close(self) -> None` | function |
| `Moss_ModelRemove` | `Model.close(...)` | `Model.close(self) -> None` | function |
| `Moss_ResourceSetCreate` | `Renderer.create_resource_set(...)` | `Renderer.create_resource_set(self, desc: ResourceSetLayoutDesc) -> ResourceSet` | function |
| `Moss_ResourceSetDestroy` | `ResourceSet.close(...)` | `ResourceSet.close(self) -> None` | function |
| `Moss_FramebufferCreate` | `Renderer.create_framebuffer(...)` | `Renderer.create_framebuffer(self, desc: FramebufferDesc) -> Framebuffer` | function |
| `Moss_FramebufferDestroy` | `Framebuffer.close(...)` | `Framebuffer.close(self) -> None` | function |
| `Moss_RendererSetUpscaler` | `Renderer.set_upscaler(...)` | `Renderer.set_upscaler(self, upscaler: Upscaler) -> None` | function |
| `Moss_RendererSetUpscalerDesc` | `Renderer.set_upscaler_desc(...)` | `Renderer.set_upscaler_desc(self, desc: RendererUpscalerDesc) -> None` | function |
| `Moss_GPUGetPhysicalDeviceName` | `moss.renderer.get_physical_device_name(...)` | `moss.renderer.get_physical_device_name() -> str \| None` | function |
| `Moss_GPUAPIGetName` | `moss.renderer.gpuapi_get_name(...)` | `moss.renderer.gpuapi_get_name() -> str \| None` | function |
| `Moss_GPUAPIGetVersion` | `moss.renderer.gpuapi_get_version(...)` | `moss.renderer.gpuapi_get_version() -> str \| None` | function |
| `Moss_RendererDrawLine2D` | `Renderer.draw_line_dimension2_d(...)` | `Renderer.draw_line_dimension2_d(self, from: Vec2, to: Vec2, color: Color, thickness: float) -> None` | function |
| `Moss_RendererDrawLines2D` | `Renderer.draw_lines_dimension2_d(...)` | `Renderer.draw_lines_dimension2_d(self, positions: TArray<Vec2>, color: Color, thickness: float) -> None` | function |
| `Moss_RendererDrawBox2D` | `Renderer.draw_box_dimension2_d(...)` | `Renderer.draw_box_dimension2_d(self, center: Sequence[Vec2], rotation: float, color: Color) -> None` | function |
| `Moss_RendererDrawCircle2D` | `Renderer.draw_circle_dimension2_d(...)` | `Renderer.draw_circle_dimension2_d(self, center: Vec2, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawCylinder2D` | `Renderer.draw_cylinder_dimension2_d(...)` | `Renderer.draw_cylinder_dimension2_d(self, matrix: RMat44, half_height: float, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawTriangle2D` | `Renderer.draw_triangle_dimension2_d(...)` | `Renderer.draw_triangle_dimension2_d(self, p0: Vec2, p1: Vec2, p2: Vec2, color: Color) -> None` | function |
| `Moss_RendererDrawArrow2D` | `Renderer.draw_arrow_dimension2_d(...)` | `Renderer.draw_arrow_dimension2_d(self, from: Vec2, to: Vec2, color: Color, head_size: float) -> None` | function |
| `Moss_RendererDrawMarker2D` | `Renderer.draw_marker_dimension2_d(...)` | `Renderer.draw_marker_dimension2_d(self, position: Vec2, color: Color, size: float) -> None` | function |
| `Moss_RendererDrawBone2D` | `Renderer.draw_bone_dimension2_d(...)` | `Renderer.draw_bone_dimension2_d(self, transform: RMat44, length: float, color: Color) -> None` | function |
| `Moss_RendererDrawCoordinateSystem2D` | `Renderer.draw_coordinate_system_dimension2_d(...)` | `Renderer.draw_coordinate_system_dimension2_d(self, matrix: RMat44, size: float) -> None` | function |
| `Moss_RendererDrawCapsule2D` | `Renderer.draw_capsule_dimension2_d(...)` | `Renderer.draw_capsule_dimension2_d(self, matrix: RMat44, half_height_of_cylinder: float, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawConvex2D` | `Renderer.draw_convex_dimension2_d(...)` | `Renderer.draw_convex_dimension2_d(self, points: Sequence[Vec2], color: Color) -> None` | function |
| `Moss_RendererDrawConvexHull2D` | `Renderer.draw_convex_hull_dimension2_d(...)` | `Renderer.draw_convex_hull_dimension2_d(self, points: Sequence[Vec2], color: Color) -> None` | function |
| `Moss_RendererDrawPolygonShape2D` | `Renderer.draw_polygon_shape_dimension2_d(...)` | `Renderer.draw_polygon_shape_dimension2_d(self, points: Sequence[Vec2], color: Color) -> None` | function |
| `Moss_RendererDrawGizmo2D` | `Renderer.draw_gizmo_dimension2_d(...)` | `Renderer.draw_gizmo_dimension2_d(self, transform: RMat44, size: float) -> None` | function |
| `Moss_RendererDrawLineBatch2D` | `Renderer.draw_line_batch_dimension2_d(...)` | `Renderer.draw_line_batch_dimension2_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawBoxBatch2D` | `Renderer.draw_box_batch_dimension2_d(...)` | `Renderer.draw_box_batch_dimension2_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawCircleBatch2D` | `Renderer.draw_circle_batch_dimension2_d(...)` | `Renderer.draw_circle_batch_dimension2_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawArrowBatch2D` | `Renderer.draw_arrow_batch_dimension2_d(...)` | `Renderer.draw_arrow_batch_dimension2_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawMarkerBatch2D` | `Renderer.draw_marker_batch_dimension2_d(...)` | `Renderer.draw_marker_batch_dimension2_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawBoxInstanced2D` | `Renderer.draw_box_instanced_dimension2_d(...)` | `Renderer.draw_box_instanced_dimension2_d(self, size: Vec2, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawCircleInstanced2D` | `Renderer.draw_circle_instanced_dimension2_d(...)` | `Renderer.draw_circle_instanced_dimension2_d(self, radius: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawCapsuleInstanced2D` | `Renderer.draw_capsule_instanced_dimension2_d(...)` | `Renderer.draw_capsule_instanced_dimension2_d(self, half_height_of_cylinder: float, radius: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawMarkerInstanced2D` | `Renderer.draw_marker_instanced_dimension2_d(...)` | `Renderer.draw_marker_instanced_dimension2_d(self, size: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawGizmoInstanced2D` | `Renderer.draw_gizmo_instanced_dimension2_d(...)` | `Renderer.draw_gizmo_instanced_dimension2_d(self, size: float, transform: RMat44, count: int) -> None` | function |
| `Moss_RendererDrawLine3D` | `Renderer.draw_line_dimension3_d(...)` | `Renderer.draw_line_dimension3_d(self, from: Vec3, to: Vec3, color: Color, thickness: float) -> None` | function |
| `Moss_RendererDrawLines3D` | `Renderer.draw_lines_dimension3_d(...)` | `Renderer.draw_lines_dimension3_d(self, positions: TArray<Vec3>, color: Color, thickness: float) -> None` | function |
| `Moss_RendererDrawBox3D` | `Renderer.draw_box_dimension3_d(...)` | `Renderer.draw_box_dimension3_d(self, center: Sequence[Vec3], rotation: RMat44, color: Color) -> None` | function |
| `Moss_RendererDrawSphere3D` | `Renderer.draw_sphere_dimension3_d(...)` | `Renderer.draw_sphere_dimension3_d(self, center: Vec3, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawCylinder3D` | `Renderer.draw_cylinder_dimension3_d(...)` | `Renderer.draw_cylinder_dimension3_d(self, base: Vec3, top: Vec3, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawCone3D` | `Renderer.draw_cone_dimension3_d(...)` | `Renderer.draw_cone_dimension3_d(self, base: Vec3, tip: Vec3, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawTriangle3D` | `Renderer.draw_triangle_dimension3_d(...)` | `Renderer.draw_triangle_dimension3_d(self, p0: Vec3, p1: Vec3, p2: Vec3, color: Color) -> None` | function |
| `Moss_RendererDrawArrow3D` | `Renderer.draw_arrow_dimension3_d(...)` | `Renderer.draw_arrow_dimension3_d(self, from: Vec3, to: Vec3, color: Color, head_size: float) -> None` | function |
| `Moss_RendererDrawMarker3D` | `Renderer.draw_marker_dimension3_d(...)` | `Renderer.draw_marker_dimension3_d(self, position: Vec3, color: Color, size: float) -> None` | function |
| `Moss_RendererDrawPlane3D` | `Renderer.draw_plane_dimension3_d(...)` | `Renderer.draw_plane_dimension3_d(self, center: Vec3, normal: Sequence[Vec3], color: Color) -> None` | function |
| `Moss_RendererDrawBone3D` | `Renderer.draw_bone_dimension3_d(...)` | `Renderer.draw_bone_dimension3_d(self, transform: RMat44, length: float, color: Color) -> None` | function |
| `Moss_RendererDrawCoordinateSystem3D` | `Renderer.draw_coordinate_system_dimension3_d(...)` | `Renderer.draw_coordinate_system_dimension3_d(self, matrix: RMat44, size: float) -> None` | function |
| `Moss_RendererDrawCapsule3D` | `Renderer.draw_capsule_dimension3_d(...)` | `Renderer.draw_capsule_dimension3_d(self, matrix: RMat44, half_height_of_cylinder: float, radius: float, color: Color) -> None` | function |
| `Moss_RendererDrawConvex3D` | `Renderer.draw_convex_dimension3_d(...)` | `Renderer.draw_convex_dimension3_d(self, points: Sequence[Vec3], color: Color) -> None` | function |
| `Moss_RendererDrawConvexHull3D` | `Renderer.draw_convex_hull_dimension3_d(...)` | `Renderer.draw_convex_hull_dimension3_d(self, points: Sequence[Vec3], color: Color) -> None` | function |
| `Moss_RendererDrawPolygonShape3D` | `Renderer.draw_polygon_shape_dimension3_d(...)` | `Renderer.draw_polygon_shape_dimension3_d(self, points: Sequence[Vec3], color: Color) -> None` | function |
| `Moss_RendererDrawGizmo3D` | `Renderer.draw_gizmo_dimension3_d(...)` | `Renderer.draw_gizmo_dimension3_d(self, transform: RMat44, size: float) -> None` | function |
| `Moss_RendererDrawLineBatch3D` | `Renderer.draw_line_batch_dimension3_d(...)` | `Renderer.draw_line_batch_dimension3_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawBoxBatch3D` | `Renderer.draw_box_batch_dimension3_d(...)` | `Renderer.draw_box_batch_dimension3_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawSphereBatch3D` | `Renderer.draw_sphere_batch_dimension3_d(...)` | `Renderer.draw_sphere_batch_dimension3_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawArrowBatch3D` | `Renderer.draw_arrow_batch_dimension3_d(...)` | `Renderer.draw_arrow_batch_dimension3_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawMarkerBatch3D` | `Renderer.draw_marker_batch_dimension3_d(...)` | `Renderer.draw_marker_batch_dimension3_d(self, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawBoxInstanced3D` | `Renderer.draw_box_instanced_dimension3_d(...)` | `Renderer.draw_box_instanced_dimension3_d(self, size: Vec3, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawSphereInstanced3D` | `Renderer.draw_sphere_instanced_dimension3_d(...)` | `Renderer.draw_sphere_instanced_dimension3_d(self, radius: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawCapsuleInstanced3D` | `Renderer.draw_capsule_instanced_dimension3_d(...)` | `Renderer.draw_capsule_instanced_dimension3_d(self, half_height_of_cylinder: float, radius: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawCylinderInstanced3D` | `Renderer.draw_cylinder_instanced_dimension3_d(...)` | `Renderer.draw_cylinder_instanced_dimension3_d(self, half_height: float, radius: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawConeInstanced3D` | `Renderer.draw_cone_instanced_dimension3_d(...)` | `Renderer.draw_cone_instanced_dimension3_d(self, height: float, radius: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawMarkerInstanced3D` | `Renderer.draw_marker_instanced_dimension3_d(...)` | `Renderer.draw_marker_instanced_dimension3_d(self, size: float, transform: RMat44, colour: Color, count: int) -> None` | function |
| `Moss_RendererDrawGizmoInstanced3D` | `Renderer.draw_gizmo_instanced_dimension3_d(...)` | `Renderer.draw_gizmo_instanced_dimension3_d(self, size: float, transform: RMat44, count: int) -> None` | function |
| `Moss_VulkanGetDevice` | `Renderer.get_device(...)` | `Renderer.get_device(self) -> VkDevice` | function |
| `Moss_VulkanGetDescriptorPool` | `Renderer.get_descriptor_pool(...)` | `Renderer.get_descriptor_pool(self) -> VkDescriptorPool` | function |
| `Moss_VulkanGetDescriptorSetLayoutTexture` | `Renderer.get_descriptor_set_layout_texture(...)` | `Renderer.get_descriptor_set_layout_texture(self) -> VkDescriptorSetLayout` | function |
| `Moss_VulkanGetTextureSamplerRepeat` | `Renderer.get_texture_sampler_repeat(...)` | `Renderer.get_texture_sampler_repeat(self) -> VkSampler` | function |
| `Moss_VulkanGetTextureSamplerShadow` | `Renderer.get_texture_sampler_shadow(...)` | `Renderer.get_texture_sampler_shadow(self) -> VkSampler` | function |
| `Moss_VulkanGetRenderPassShadow` | `Renderer.get_render_pass_shadow(...)` | `Renderer.get_render_pass_shadow(self) -> VkRenderPass` | function |
| `Moss_VulkanGetRenderPass` | `Renderer.get_render_pass(...)` | `Renderer.get_render_pass(self) -> VkRenderPass` | function |
| `Moss_VulkanGetPipelineLayout` | `Renderer.get_pipeline_layout(...)` | `Renderer.get_pipeline_layout(self) -> VkPipelineLayout` | function |
| `Moss_VulkanGetCommandBuffer` | `Renderer.get_command_buffer(...)` | `Renderer.get_command_buffer(self) -> VkCommandBuffer` | function |
| `Moss_VulkanStartTempCommandBuffer` | `Renderer.start_temp_command_buffer(...)` | `Renderer.start_temp_command_buffer(self) -> VkCommandBuffer` | function |
| `Moss_VulkanEndTempCommandBuffer` | `Renderer.end_temp_command_buffer(...)` | `Renderer.end_temp_command_buffer(self, command_buffer: VkCommandBuffer) -> None` | function |
| `Moss_VulkanAllocateMemory` | `Renderer.allocate_memory(...)` | `Renderer.allocate_memory(self, size: VkDeviceSize, memory_type_bits: int, properties: VkMemoryPropertyFlags, out_memory: VkDeviceMemory) -> None` | function |
| `Moss_VulkanFreeMemory` | `Renderer.free_memory(...)` | `Renderer.free_memory(self, memory: VkDeviceMemory, size: VkDeviceSize) -> None` | function |
| `Moss_VulkanCreateBuffer` | `Renderer.create_buffer(...)` | `Renderer.create_buffer(self, size: VkDeviceSize, usage: VkBufferUsageFlags, properties: VkMemoryPropertyFlags, out_buffer: BufferVK) -> None` | function |
| `Moss_VulkanCopyBuffer` | `Renderer.copy_buffer(...)` | `Renderer.copy_buffer(self, src: VkBuffer, dst: VkBuffer, size: VkDeviceSize) -> None` | function |
| `Moss_VulkanCreateDeviceLocalBuffer` | `Renderer.create_device_local_buffer(...)` | `Renderer.create_device_local_buffer(self, data: memoryview, size: VkDeviceSize, usage: VkBufferUsageFlags, out_buffer: BufferVK) -> None` | function |
| `Moss_VulkanFreeBuffer` | `Renderer.free_buffer(...)` | `Renderer.free_buffer(self, buffer: BufferVK) -> None` | function |
| `Moss_VulkanCreateConstantBuffer` | `Renderer.create_constant_buffer(...)` | `Renderer.create_constant_buffer(self, buffer_size: VkDeviceSize) -> ConstantBufferVK` | function |
| `Moss_VulkanCreateImage` | `Renderer.create_image(...)` | `Renderer.create_image(self, width: int, height: int, format: VkFormat, tiling: VkImageTiling, usage: VkImageUsageFlags, properties: VkMemoryPropertyFlags, out_image: VkImage, out_memory: VkDeviceMemory) -> None` | function |
| `Moss_VulkanDestroyImage` | `Renderer.destroy_image(...)` | `Renderer.destroy_image(self, image: VkImage, memory: VkDeviceMemory) -> None` | function |
| `Moss_VulkanCreateImageView` | `Renderer.create_image_view(...)` | `Renderer.create_image_view(self, image: VkImage, format: VkFormat, aspect_flags: VkImageAspectFlags) -> VkImageView` | function |
| `Moss_VulkanFindDepthFormat` | `Renderer.find_depth_format(...)` | `Renderer.find_depth_format(self) -> VkFormat` | function |
| `Moss_Vulkan_EncodeSPIRV` | `moss.renderer.encode_spirv(...)` | `moss.renderer.encode_spirv(path: str) -> None` | function |
| `Moss_Vulkan_DecodeSPIRV` | `moss.renderer.decode_spirv(...)` | `moss.renderer.decode_spirv(path: str) -> None` | function |
| `Moss_DX12GetDevice` | `Renderer.dx12_get_device(...)` | `Renderer.dx12_get_device(self) -> IdDIMENSION3D1DIMENSION2Device` | function |
| `Moss_DX12GetRootSignature` | `Renderer.dx12_get_root_signature(...)` | `Renderer.dx12_get_root_signature(self) -> IdDIMENSION3D12RootSignature` | function |
| `Moss_DX12GetCommandList` | `Renderer.dx12_get_command_list(...)` | `Renderer.dx12_get_command_list(self) -> IdDIMENSION3D12GraphicsCommandList` | function |
| `Moss_DX12GetUploadQueue` | `Renderer.dx12_get_upload_queue(...)` | `Renderer.dx12_get_upload_queue(self) -> CommandQueueDX12` | function |
| `Moss_DX12GetDSVHeap` | `Renderer.dx12_get_dsv_heap(...)` | `Renderer.dx12_get_dsv_heap(self) -> DescriptorHeapDX12` | function |
| `Moss_DX12GetSRVHeap` | `Renderer.dx12_get_srv_heap(...)` | `Renderer.dx12_get_srv_heap(self) -> DescriptorHeapDX12` | function |
| `Moss_DX_CompileHLSL` | `moss.renderer.compile_hlsl(...)` | `moss.renderer.compile_hlsl(input_path: str, output_path: str, entry_point: str, target: str) -> bool` | function |
| `Moss_DX_EnableDebugLayer` | `moss.renderer.enable_debug_layer(...)` | `moss.renderer.enable_debug_layer(enable: bool) -> None` | function |
| `Moss_DX_GetShaderModel` | `moss.renderer.get_shader_model(...)` | `moss.renderer.get_shader_model() -> int` | function |
| `Moss_MetalGetView` | `Renderer.get_view(...)` | `Renderer.get_view(self) -> MTKView` | function |
| `Moss_MetalGetDevice` | `Renderer.get_device(...)` | `Renderer.get_device(self) -> memoryview` | function |
| `Moss_MetalGetRenderEncoder` | `Renderer.get_render_encoder(...)` | `Renderer.get_render_encoder(self) -> memoryview` | function |
| `Moss_Metal_CompileMSL` | `moss.renderer.compile_msl(...)` | `moss.renderer.compile_msl(input_path: str, output_path: str) -> bool` | function |
| `Moss_Metal_SupportsFamily` | `moss.renderer.supports_family(...)` | `moss.renderer.supports_family(family: int) -> bool` | function |

### `Moss_stdinc.h` → `moss.core`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Allocate` | `moss.core.allocator.allocate` | `moss.core.allocator.allocate: Callable[..., object]` | callback |
| `Reallocate` | `moss.core.allocator.reallocate` | `moss.core.allocator.reallocate: Callable[..., object]` | callback |
| `Free` | `moss.core.allocator.free` | `moss.core.allocator.free: Callable[..., object]` | callback |
| `AlignedAllocate` | `moss.core.allocator.aligned_allocate` | `moss.core.allocator.aligned_allocate: Callable[..., object]` | callback |
| `AlignedFree` | `moss.core.allocator.aligned_free` | `moss.core.allocator.aligned_free: Callable[..., object]` | callback |
| `RegisterDefaultAllocator` | `moss.core.register_default_allocator(...)` | `moss.core.register_default_allocator() -> None` | function |
| `Allocate` | `moss.core.allocate(...)` | `moss.core.allocate(in_size: int) -> memoryview` | function |
| `Reallocate` | `moss.core.reallocate(...)` | `moss.core.reallocate(in_block: memoryview, in_old_size: int, in_new_size: int) -> memoryview` | function |
| `Free` | `moss.core.free(...)` | `moss.core.free(in_block: memoryview) -> None` | function |
| `AlignedAllocate` | `moss.core.aligned_allocate(...)` | `moss.core.aligned_allocate(in_size: int, in_alignment: int) -> memoryview` | function |
| `AlignedFree` | `moss.core.aligned_free(...)` | `moss.core.aligned_free(in_block: memoryview) -> None` | function |
| `Moss_rand` | `moss.core.rand(...)` | `moss.core.rand(n: Int32) -> Int32` | function |
| `Moss_randf` | `moss.core.randf(...)` | `moss.core.randf() -> float` | function |
| `Moss_randi` | `moss.core.randi(...)` | `moss.core.randi() -> int` | function |
| `Moss_srand` | `moss.core.srand(...)` | `moss.core.srand(seed: Int64) -> None` | function |
| `Moss_randi_range` | `moss.core.randi_range(...)` | `moss.core.randi_range(from: Int32, to: Int32) -> Int32` | function |
| `Moss_randf_range` | `moss.core.randf_range(...)` | `moss.core.randf_range(from: float, to: float) -> float` | function |
| `Moss_rand_gaussian` | `moss.core.rand_gaussian(...)` | `moss.core.rand_gaussian(mean: float, stddev: float) -> float` | function |
| `Moss_rand_unit` | `moss.core.rand_unit(...)` | `moss.core.rand_unit() -> float` | function |
| `Moss_GetTicks` | `moss.core.get_ticks(...)` | `moss.core.get_ticks() -> Time` | function |
| `Moss_GetSeconds` | `moss.core.get_seconds(...)` | `moss.core.get_seconds(time: Time) -> float` | function |
| `Moss_GetMilliseconds` | `moss.core.get_milliseconds(...)` | `moss.core.get_milliseconds(ticks: Time) -> float` | function |
| `Moss_GetMillisecondsAndReset` | `moss.core.get_milliseconds_and_reset(...)` | `moss.core.get_milliseconds_and_reset(ticks: Time) -> float` | function |
| `Moss_Yield` | `moss.core.yield(...)` | `moss.core.yield() -> None` | function |
| `Moss_Delay` | `moss.core.delay(...)` | `moss.core.delay(delay: float) -> float` | function |
| `Moss_LocalTime` | `moss.core.local_time(...)` | `moss.core.local_time() -> str \| None` | function |
| `Moss_TimeStamp` | `moss.core.time_stamp(...)` | `moss.core.time_stamp() -> str \| None` | function |
| `Moss_TimeNow` | `moss.core.time_now(...)` | `moss.core.time_now() -> str \| None` | function |
| `Moss_CTimeNow` | `moss.core.c_time_now(...)` | `moss.core.c_time_now() -> str \| None` | function |
| `Moss_TimeStampET` | `moss.core.time_stamp_et(...)` | `moss.core.time_stamp_et() -> str \| None` | function |
| `Moss_TimeStampCT` | `moss.core.time_stamp_ct(...)` | `moss.core.time_stamp_ct() -> str \| None` | function |
| `Moss_TimeStampMT` | `moss.core.time_stamp_mt(...)` | `moss.core.time_stamp_mt() -> str \| None` | function |
| `Moss_TimeStampPT` | `moss.core.time_stamp_pt(...)` | `moss.core.time_stamp_pt() -> str \| None` | function |
| `Moss_FormatTime` | `moss.core.format_time(...)` | `moss.core.format_time(fmt: str) -> str \| None` | function |

### `Moss_XR.h` → `moss.xr`

| Native symbol (traceability) | Public API projection | Target-language signature | Kind |
|---|---|---|---|
| `Moss_XR_Initialize` | `moss.xr.initialize(...)` | `moss.xr.initialize(self) -> bool` | function |
| `Moss_XR_Shutdown` | `moss.xr.shutdown(...)` | `moss.xr.shutdown() -> None` | function |
| `Moss_XR_GetCapabilities` | `moss.xr.get_capabilities(...)` | `moss.xr.get_capabilities() -> XrCapabilities` | function |
| `Moss_XR_EnableValidationLayers` | `moss.xr.enable_validation_layers(...)` | `moss.xr.enable_validation_layers() -> bool` | function |
| `Moss_XR_CreateSession` | `moss.xr.create_session(...)` | `moss.xr.create_session() -> XrSession` | function |
| `Moss_XR_DestroySession` | `XrSession.close(...)` | `XrSession.close(self) -> None` | function |
| `Moss_XR_GetSessionState` | `XrSession.get_state(...)` | `XrSession.get_state(self) -> XrSessionState` | function |
| `Moss_XR_BeginFrame` | `XrSession.begin_frame(...)` | `XrSession.begin_frame(self) -> bool` | function |
| `Moss_XR_EndFrame` | `XrSession.end_frame(...)` | `XrSession.end_frame(self) -> None` | function |
| `Moss_XR_GetPredictedDisplayTime` | `XrSession.get_predicted_display_time(...)` | `XrSession.get_predicted_display_time(self) -> XrTime` | function |
| `Moss_XR_GetDeltaSeconds` | `moss.xr.get_delta_seconds(...)` | `moss.xr.get_delta_seconds() -> float` | function |
| `Moss_XR_HandleEvents` | `XrSession.handle_events(...)` | `XrSession.handle_events(self) -> None` | function |
| `Moss_XR_GetViewCount` | `XrSession.get_view_count(...)` | `XrSession.get_view_count(self) -> int` | function |
| `Moss_XR_GetView` | `XrSession.get_view(...)` | `XrSession.get_view(self, index: int, out_view: XrView) -> bool` | function |
| `Moss_XR_CreateActionSet` | `moss.xr.create_action_set(...)` | `moss.xr.create_action_set(name: str) -> XrActionSet` | function |
| `Moss_XR_DestroyActionSet` | `XrActionSet.close(...)` | `XrActionSet.close(self) -> None` | function |
| `Moss_XR_CreateAction` | `XrActionSet.create_action(...)` | `XrActionSet.create_action(self, name: str, type: XrActionType) -> XrAction` | function |
| `Moss_XR_DestroyAction` | `XrAction.close(...)` | `XrAction.close(self) -> None` | function |
| `Moss_XR_AttachActionSet` | `XrActionSet.attach(...)` | `XrActionSet.attach(self, session: XrSession) -> None` | function |
| `Moss_XR_SyncActions` | `XrSession.sync_actions(...)` | `XrSession.sync_actions(self) -> None` | function |
| `Moss_XR_GetActionBoolean` | `XrAction.get_boolean(...)` | `XrAction.get_boolean(self) -> bool` | function |
| `Moss_XR_GetActionFloat` | `XrAction.get_float(...)` | `XrAction.get_float(self) -> float` | function |
| `Moss_XR_GetActionPose` | `XrAction.get_pose(...)` | `XrAction.get_pose(self, out_pose: XrPose) -> bool` | function |
| `Moss_XR_StopHaptic` | `XrAction.stop_haptic(...)` | `XrAction.stop_haptic(self) -> None` | function |
| `Moss_XR_CreateHand` | `moss.xr.create_hand(...)` | `moss.xr.create_hand() -> XrHandModifier` | function |
| `Moss_XR_GetHandJointPose` | `XrHandModifier.get_hand_joint_pose(...)` | `XrHandModifier.get_hand_joint_pose(self, joint: XrHandJoint, out_pose: XrPose) -> bool` | function |
| `Moss_XR_DestroyHand` | `XrHandModifier.destroy_hand(...)` | `XrHandModifier.destroy_hand(self) -> None` | function |
| `Moss_XR_GetHandPose` | `XrHandModifier.get_hand_pose(...)` | `XrHandModifier.get_hand_pose(self, out_pose: XrPose) -> bool` | function |
| `Moss_XR_CreateBody` | `moss.xr.create_body(...)` | `moss.xr.create_body() -> XrBodyModifier` | function |
| `Moss_XR_GetBodyJointPose` | `XrBodyModifier.get_body_joint_pose(...)` | `XrBodyModifier.get_body_joint_pose(self, joint: XrBodyJoint, out_pose: XrPose) -> bool` | function |
| `Moss_XR_DestroyBody` | `XrBodyModifier.destroy_body(...)` | `XrBodyModifier.destroy_body(self) -> None` | function |
| `Moss_XR_CreateFace` | `moss.xr.create_face(...)` | `moss.xr.create_face() -> XrFaceModifier` | function |
| `Moss_XR_DestroyFace` | `XrFaceModifier.destroy_face(...)` | `XrFaceModifier.destroy_face(self) -> None` | function |
| `Moss_XR_UpdateBodyTracking` | `XrSession.update_body_tracking(...)` | `XrSession.update_body_tracking(self) -> None` | function |
| `Moss_XR_UpdateHandTracking` | `XrSession.update_hand_tracking(...)` | `XrSession.update_hand_tracking(self) -> None` | function |
| `Moss_XR_EnablePassthrough` | `moss.xr.enable_passthrough(...)` | `moss.xr.enable_passthrough(enable: bool) -> bool` | function |
| `Moss_XR_EnableFoveatedRendering` | `moss.xr.enable_foveated_rendering(...)` | `moss.xr.enable_foveated_rendering(enable: bool) -> bool` | function |
| `Moss_XR_GetBackendName` | `moss.xr.get_backend_name(...)` | `moss.xr.get_backend_name() -> str \| None` | function |
| `Moss_XR_GetBackendVersion` | `moss.xr.get_backend_version(...)` | `moss.xr.get_backend_version() -> int` | function |
| `Moss_XR_BeginDebugLabel` | `moss.xr.begin_debug_label(...)` | `moss.xr.begin_debug_label(label: str) -> None` | function |
| `Moss_XR_EndDebugLabel` | `moss.xr.end_debug_label(...)` | `moss.xr.end_debug_label() -> None` | function |


## 8. Native header and feature coverage

The public binding must cover the complete Moss header tree, not only the exported function index. Many Core, Geometry, Physics, Renderer, Resources, Skeleton, TriangleSplitter and Variants APIs are class members, templates, inline methods, or declarations not represented by the top-level free-function catalogue.

## Complete header inventory

Each link opens the corresponding source file in `include/Moss/`. The rightmost count shows declarations beginning with `MOSS_API` in that header; zero does not mean the header has no public API (it may expose class members, templates, inline methods, typedefs, or preprocessor declarations).

### Core (40 headers)

Foundational C++ utilities.

| Header | Export-marked declarations |
|---|---:|
| [`Core/BinaryHeap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/BinaryHeap.h) | 0 |
| [`Core/ByteBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/ByteBuffer.h) | 0 |
| [`Core/FPControlWord.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPControlWord.h) | 0 |
| [`Core/FPException.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPException.h) | 0 |
| [`Core/FPFlushDenormals.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FPFlushDenormals.h) | 0 |
| [`Core/Factory.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Factory.h) | 0 |
| [`Core/FixedSizeFreeList.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FixedSizeFreeList.h) | 0 |
| [`Core/FreeBlockAlloc.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FreeBlockAlloc.h) | 0 |
| [`Core/HashCombine.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/HashCombine.h) | 0 |
| [`Core/HashTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/HashTable.h) | 0 |
| [`Core/InsertionSort.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/InsertionSort.h) | 0 |
| [`Core/JobSystem.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystem.h) | 0 |
| [`Core/JobSystemSingleThreaded.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemSingleThreaded.h) | 0 |
| [`Core/JobSystemThreadPool.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemThreadPool.h) | 0 |
| [`Core/JobSystemWithBarrier.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystemWithBarrier.h) | 0 |
| [`Core/LinearCurve.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LinearCurve.h) | 0 |
| [`Core/LockFreeHashMap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LockFreeHashMap.h) | 0 |
| [`Core/Memory.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Memory.h) | 11 |
| [`Core/Mutex.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Mutex.h) | 0 |
| [`Core/MutexArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/MutexArray.h) | 0 |
| [`Core/NonCopyable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/NonCopyable.h) | 0 |
| [`Core/Profiler.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Profiler.h) | 4 |
| [`Core/QuickSort.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/QuickSort.h) | 0 |
| [`Core/RTTI.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/RTTI.h) | 0 |
| [`Core/Reference.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Reference.h) | 0 |
| [`Core/Result.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Result.h) | 0 |
| [`Core/STLAlignedAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLAlignedAllocator.h) | 0 |
| [`Core/STLAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLAllocator.h) | 0 |
| [`Core/STLLocalAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLLocalAllocator.h) | 0 |
| [`Core/STLTempAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/STLTempAllocator.h) | 0 |
| [`Core/ScopeExit.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/ScopeExit.h) | 0 |
| [`Core/Semaphore.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Semaphore.h) | 0 |
| [`Core/StreamIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamIn.h) | 0 |
| [`Core/StreamOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamOut.h) | 0 |
| [`Core/StreamUtils.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamUtils.h) | 0 |
| [`Core/StreamWrapper.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StreamWrapper.h) | 0 |
| [`Core/StridedPtr.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StridedPtr.h) | 0 |
| [`Core/StringTools.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/StringTools.h) | 6 |
| [`Core/TempAllocator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/TempAllocator.h) | 0 |
| [`Core/TickCounter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/TickCounter.h) | 0 |

### Geometry (9 headers)

Geometry algorithms and primitives.

| Header | Export-marked declarations |
|---|---:|
| [`Geometry/ClipPoly.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ClipPoly.h) | 0 |
| [`Geometry/ClosestPoint.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ClosestPoint.h) | 0 |
| [`Geometry/ConvexHullBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ConvexHullBuilder.h) | 0 |
| [`Geometry/ConvexSupport.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/ConvexSupport.h) | 0 |
| [`Geometry/EPAConvexHullBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/EPAConvexHullBuilder.h) | 0 |
| [`Geometry/EPAPenetrationDepth.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/EPAPenetrationDepth.h) | 0 |
| [`Geometry/GJKClosestPoint.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/GJKClosestPoint.h) | 0 |
| [`Geometry/RayCylinder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/RayCylinder.h) | 0 |
| [`Geometry/Triangle.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Geometry/Triangle.h) | 0 |

### Physics (69 headers)

Simulation, body, shape, collision, constraint and query implementation headers.

| Header | Export-marked declarations |
|---|---:|
| [`Physics/AABBTree/AABBTreeBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/AABBTreeBuilder.h) | 0 |
| [`Physics/AABBTree/AABBTreeToBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/AABBTreeToBuffer.h) | 0 |
| [`Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat.h) | 0 |
| [`Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat2D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/NodeCodec/NodeCodecQuadTreeHalfFloat2D.h) | 0 |
| [`Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags.h) | 0 |
| [`Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags2D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/AABBTree/TriangleCodec/TriangleCodecIndexed8BitPackSOA4Flags2D.h) | 0 |
| [`Physics/Body/Body.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/Body.h) | 1 |
| [`Physics/Body/BodyID.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/BodyID.h) | 0 |
| [`Physics/Character/Character.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Character/Character.h) | 0 |
| [`Physics/Cloth/Cloth.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Cloth/Cloth.h) | 0 |
| [`Physics/Collision/ActiveEdges.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ActiveEdges.h) | 0 |
| [`Physics/Collision/BroadPhase/BroadPhase.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/BroadPhase.h) | 0 |
| [`Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/BroadPhaseLayerInterfaceMask.h) | 0 |
| [`Physics/Collision/BroadPhase/QuadTree.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/BroadPhase/QuadTree.h) | 0 |
| [`Physics/Collision/CastConvexVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastConvexVsTriangles.h) | 0 |
| [`Physics/Collision/CastResult.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastResult.h) | 0 |
| [`Physics/Collision/CastSphereVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CastSphereVsTriangles.h) | 0 |
| [`Physics/Collision/CollideConvexVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideConvexVsTriangles.h) | 0 |
| [`Physics/Collision/CollidePointResult.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollidePointResult.h) | 0 |
| [`Physics/Collision/CollideShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideShape.h) | 0 |
| [`Physics/Collision/CollideShapeVsShapePerLeaf.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideShapeVsShapePerLeaf.h) | 0 |
| [`Physics/Collision/CollideSoftBodyVertexIterator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSoftBodyVertexIterator.h) | 0 |
| [`Physics/Collision/CollideSoftBodyVerticesVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSoftBodyVerticesVsTriangles.h) | 0 |
| [`Physics/Collision/CollideSphereVsTriangles.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollideSphereVsTriangles.h) | 0 |
| [`Physics/Collision/CollisionCollector.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionCollector.h) | 0 |
| [`Physics/Collision/CollisionCollectorImpl.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionCollectorImpl.h) | 0 |
| [`Physics/Collision/CollisionDispatch.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionDispatch.h) | 0 |
| [`Physics/Collision/CollisionGroup.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/CollisionGroup.h) | 0 |
| [`Physics/Collision/ContactListener.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ContactListener.h) | 0 |
| [`Physics/Collision/EstimateCollisionResponse.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/EstimateCollisionResponse.h) | 1 |
| [`Physics/Collision/GroupFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/GroupFilter.h) | 0 |
| [`Physics/Collision/GroupFilterTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/GroupFilterTable.h) | 0 |
| [`Physics/Collision/InternalEdgeRemovingCollector.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/InternalEdgeRemovingCollector.h) | 0 |
| [`Physics/Collision/ManifoldBetweenTwoFaces.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ManifoldBetweenTwoFaces.h) | 2 |
| [`Physics/Collision/NarrowPhaseQuery.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/NarrowPhaseQuery.h) | 0 |
| [`Physics/Collision/NarrowPhaseStats.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/NarrowPhaseStats.h) | 0 |
| [`Physics/Collision/ObjectLayer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayer.h) | 0 |
| [`Physics/Collision/ObjectLayerPairFilterMask.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayerPairFilterMask.h) | 0 |
| [`Physics/Collision/ObjectLayerPairFilterTable.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ObjectLayerPairFilterTable.h) | 0 |
| [`Physics/Collision/PhysicsMaterial.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/PhysicsMaterial.h) | 0 |
| [`Physics/Collision/PhysicsMaterialSimple.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/PhysicsMaterialSimple.h) | 0 |
| [`Physics/Collision/RayCast.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/RayCast.h) | 0 |
| [`Physics/Collision/Shape/CompoundShapeVisitors.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/CompoundShapeVisitors.h) | 0 |
| [`Physics/Collision/Shape/ConvexHullShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/ConvexHullShape.h) | 0 |
| [`Physics/Collision/Shape/GetTrianglesContext.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/GetTrianglesContext.h) | 0 |
| [`Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/PolyhedronSubmergedVolumeCalculator.h) | 0 |
| [`Physics/Collision/Shape/Shapes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/Shapes.h) | 0 |
| [`Physics/Collision/Shape/SubShapeID.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/SubShapeID.h) | 0 |
| [`Physics/Collision/Shape/SubShapeIDPair.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/Shape/SubShapeIDPair.h) | 0 |
| [`Physics/Collision/ShapeCast.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ShapeCast.h) | 0 |
| [`Physics/Collision/ShapeFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/ShapeFilter.h) | 0 |
| [`Physics/Collision/SimShapeFilter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SimShapeFilter.h) | 0 |
| [`Physics/Collision/SimShapeFilterWrapper.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SimShapeFilterWrapper.h) | 0 |
| [`Physics/Collision/SortReverseAndStore.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/SortReverseAndStore.h) | 0 |
| [`Physics/Collision/TransformedShape.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Collision/TransformedShape.h) | 0 |
| [`Physics/Constraints/ConstraintManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Constraints/ConstraintManager.h) | 0 |
| [`Physics/Constraints/Constraints.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Constraints/Constraints.h) | 0 |
| [`Physics/Hair/Hair.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Hair/Hair.h) | 0 |
| [`Physics/IslandBuilder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/IslandBuilder.h) | 0 |
| [`Physics/LargeIslandSplitter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/LargeIslandSplitter.h) | 0 |
| [`Physics/PhysicsLock.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsLock.h) | 0 |
| [`Physics/PhysicsScene.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsScene.h) | 0 |
| [`Physics/PhysicsSystem.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsSystem.h) | 0 |
| [`Physics/PhysicsUpdateContext.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/PhysicsUpdateContext.h) | 0 |
| [`Physics/Ragdoll/Ragdoll.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Ragdoll/Ragdoll.h) | 0 |
| [`Physics/SoftBody/Softbody.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/SoftBody/Softbody.h) | 0 |
| [`Physics/StateRecorder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/StateRecorder.h) | 0 |
| [`Physics/StateRecorderImpl.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/StateRecorderImpl.h) | 0 |
| [`Physics/Vehicle/Vehicle.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Vehicle/Vehicle.h) | 0 |

### Renderer (65 headers)

Renderer resources and graphics backend headers.

| Header | Export-marked declarations |
|---|---:|
| [`Renderer/DX12/CommandQueueDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/CommandQueueDX12.h) | 0 |
| [`Renderer/DX12/ConstantBufferDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/ConstantBufferDX12.h) | 0 |
| [`Renderer/DX12/DescriptorHeapDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/DescriptorHeapDX12.h) | 0 |
| [`Renderer/DX12/LightingDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/LightingDX12.h) | 0 |
| [`Renderer/DX12/PipelineStateDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/PipelineStateDX12.h) | 0 |
| [`Renderer/DX12/RenderInstancesDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RenderInstancesDX12.h) | 0 |
| [`Renderer/DX12/RenderPrimitiveDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RenderPrimitiveDX12.h) | 0 |
| [`Renderer/DX12/RendererDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/RendererDX12.h) | 0 |
| [`Renderer/DX12/ShaderDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/ShaderDX12.h) | 0 |
| [`Renderer/DX12/TextureDX12.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DX12/TextureDX12.h) | 0 |
| [`Renderer/DebugRenderer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRenderer.h) | 0 |
| [`Renderer/DebugRendererPlayback.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererPlayback.h) | 0 |
| [`Renderer/DebugRendererRecorder.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererRecorder.h) | 0 |
| [`Renderer/DebugRendererSimple.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/DebugRendererSimple.h) | 0 |
| [`Renderer/FogVolume.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/FogVolume.h) | 0 |
| [`Renderer/Font.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Font.h) | 0 |
| [`Renderer/GL/FontGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/FontGL.h) | 0 |
| [`Renderer/GL/LightingGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/LightingGL.h) | 0 |
| [`Renderer/GL/MeshGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/MeshGL.h) | 0 |
| [`Renderer/GL/PipelineStateGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/PipelineStateGL.h) | 0 |
| [`Renderer/GL/PostProcess.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/PostProcess.h) | 0 |
| [`Renderer/GL/ReflectionProbe.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/ReflectionProbe.h) | 0 |
| [`Renderer/GL/Renderer_GL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/Renderer_GL.h) | 0 |
| [`Renderer/GL/ShaderGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/ShaderGL.h) | 0 |
| [`Renderer/GL/StorageBufferGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/StorageBufferGL.h) | 0 |
| [`Renderer/GL/SubViewportGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/SubViewportGL.h) | 0 |
| [`Renderer/GL/SurfaceGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/SurfaceGL.h) | 0 |
| [`Renderer/GL/TextureGL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/TextureGL.h) | 0 |
| [`Renderer/GL/glad.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/GL/glad.h) | 0 |
| [`Renderer/MTL/PipelineStateMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/PipelineStateMTL.h) | 0 |
| [`Renderer/MTL/RenderInstancesMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RenderInstancesMTL.h) | 0 |
| [`Renderer/MTL/RenderPrimitiveMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RenderPrimitiveMTL.h) | 0 |
| [`Renderer/MTL/RendererMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/RendererMTL.h) | 0 |
| [`Renderer/MTL/ShaderMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/ShaderMTL.h) | 0 |
| [`Renderer/MTL/TextureMTL.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/MTL/TextureMTL.h) | 0 |
| [`Renderer/Managers/DecalManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/DecalManager.h) | 0 |
| [`Renderer/Managers/FontManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/FontManager.h) | 0 |
| [`Renderer/Managers/LightingManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/LightingManager.h) | 0 |
| [`Renderer/Managers/MeshManager3D.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/MeshManager3D.h) | 0 |
| [`Renderer/Managers/ParticleEffectManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/ParticleEffectManager.h) | 0 |
| [`Renderer/Managers/SurfaceManager.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Managers/SurfaceManager.h) | 0 |
| [`Renderer/Material2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Material2.h) | 0 |
| [`Renderer/Material3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Material3.h) | 0 |
| [`Renderer/Mesh.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Mesh.h) | 0 |
| [`Renderer/Model.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Model.h) | 0 |
| [`Renderer/PipelineState.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/PipelineState.h) | 0 |
| [`Renderer/Renderer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Renderer.h) | 0 |
| [`Renderer/Shader.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Shader.h) | 0 |
| [`Renderer/SkyBox.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/SkyBox.h) | 0 |
| [`Renderer/StorageBuffer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/StorageBuffer.h) | 0 |
| [`Renderer/SubViewport.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/SubViewport.h) | 0 |
| [`Renderer/Surface.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Surface.h) | 0 |
| [`Renderer/Texture.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/Texture.h) | 0 |
| [`Renderer/VK/ConstantBufferVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/ConstantBufferVK.h) | 0 |
| [`Renderer/VK/LightingManagerVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/LightingManagerVK.h) | 0 |
| [`Renderer/VK/MeshVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/MeshVK.h) | 0 |
| [`Renderer/VK/PipelineStateVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/PipelineStateVK.h) | 0 |
| [`Renderer/VK/RenderInstancesVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RenderInstancesVK.h) | 0 |
| [`Renderer/VK/RenderPrimitiveVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RenderPrimitiveVK.h) | 0 |
| [`Renderer/VK/RendererVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/RendererVK.h) | 0 |
| [`Renderer/VK/Renderer_VK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/Renderer_VK.h) | 4 |
| [`Renderer/VK/ShaderVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/ShaderVK.h) | 0 |
| [`Renderer/VK/StorageBufferVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/StorageBufferVK.h) | 0 |
| [`Renderer/VK/SubViewportVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/SubViewportVK.h) | 0 |
| [`Renderer/VK/TextureVK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Renderer/VK/TextureVK.h) | 0 |

### Resources (21 headers)

File/resource and serialization headers.

| Header | Export-marked declarations |
|---|---:|
| [`Resources/BezierCurve.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/BezierCurve.h) | 0 |
| [`Resources/Config.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Config.h) | 0 |
| [`Resources/Gradient.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Gradient.h) | 0 |
| [`Resources/Json.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Json.h) | 0 |
| [`Resources/ObjectStream/GetPrimitiveTypeOfType.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/GetPrimitiveTypeOfType.h) | 0 |
| [`Resources/ObjectStream/ObjectStream.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStream.h) | 4 |
| [`Resources/ObjectStream/ObjectStreamBinaryIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamBinaryIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamBinaryOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamBinaryOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTextIn.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTextIn.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTextOut.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTextOut.h) | 0 |
| [`Resources/ObjectStream/ObjectStreamTypes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/ObjectStreamTypes.h) | 0 |
| [`Resources/ObjectStream/SerializableAttribute.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttribute.h) | 0 |
| [`Resources/ObjectStream/SerializableAttributeEnum.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttributeEnum.h) | 0 |
| [`Resources/ObjectStream/SerializableAttributeTyped.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableAttributeTyped.h) | 0 |
| [`Resources/ObjectStream/SerializableObject.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/SerializableObject.h) | 0 |
| [`Resources/ObjectStream/TypeDeclarations.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/ObjectStream/TypeDeclarations.h) | 0 |
| [`Resources/PCK.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/PCK.h) | 0 |
| [`Resources/Timer.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Timer.h) | 0 |
| [`Resources/Tween.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Resources/Tween.h) | 0 |

### Skeleton (1 headers)

Skeleton and animation declarations.

| Header | Export-marked declarations |
|---|---:|
| [`Skeleton/Skeleton.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Skeleton/Skeleton.h) | 0 |

### TriangleSplitter (1 headers)

Triangle splitting utilities.

| Header | Export-marked declarations |
|---|---:|
| [`TriangleSplitter/TriangleSplitter.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/TriangleSplitter/TriangleSplitter.h) | 0 |

### Variants (53 headers)

Math, geometry and container value types.

| Header | Export-marked declarations |
|---|---:|
| [`Variants/AABB2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/AABB2.h) | 0 |
| [`Variants/AABB3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/AABB3.h) | 0 |
| [`Variants/Basis.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Basis.h) | 0 |
| [`Variants/Color.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Color.h) | 0 |
| [`Variants/Math/EigenValueSymmetric.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/EigenValueSymmetric.h) | 0 |
| [`Variants/Math/GaussianElimination.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/GaussianElimination.h) | 0 |
| [`Variants/Math/HalfFloat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/HalfFloat.h) | 0 |
| [`Variants/Math/MathTypes.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/MathTypes.h) | 0 |
| [`Variants/Math/Real.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/Real.h) | 0 |
| [`Variants/Math/Trigonometry.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Math/Trigonometry.h) | 0 |
| [`Variants/Matrix/DMat44.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat44.h) | 0 |
| [`Variants/Matrix/DMat4x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat4x3.h) | 0 |
| [`Variants/Matrix/DynMatrix.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DynMatrix.h) | 0 |
| [`Variants/Matrix/Mat22.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat22.h) | 0 |
| [`Variants/Matrix/Mat2x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat2x3.h) | 0 |
| [`Variants/Matrix/Mat2x4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat2x4.h) | 0 |
| [`Variants/Matrix/Mat33.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat33.h) | 0 |
| [`Variants/Matrix/Mat3x2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat3x2.h) | 0 |
| [`Variants/Matrix/Mat3x4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat3x4.h) | 0 |
| [`Variants/Matrix/Mat44.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat44.h) | 0 |
| [`Variants/Matrix/Mat4x2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat4x2.h) | 0 |
| [`Variants/Matrix/Mat4x3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat4x3.h) | 0 |
| [`Variants/Matrix/TMatrix.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/TMatrix.h) | 0 |
| [`Variants/OBB2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/OBB2.h) | 0 |
| [`Variants/OBB3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/OBB3.h) | 0 |
| [`Variants/Plane.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Plane.h) | 0 |
| [`Variants/Quat.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Quat.h) | 0 |
| [`Variants/Rect.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Rect.h) | 0 |
| [`Variants/TArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TArray.h) | 0 |
| [`Variants/TMap.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TMap.h) | 0 |
| [`Variants/TSet.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TSet.h) | 0 |
| [`Variants/TStaticArray.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/TStaticArray.h) | 0 |
| [`Variants/Vector/BVec16.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/BVec16.h) | 0 |
| [`Variants/Vector/DVec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec2.h) | 0 |
| [`Variants/Vector/DVec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec3.h) | 0 |
| [`Variants/Vector/DVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec4.h) | 0 |
| [`Variants/Vector/Double2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double2.h) | 0 |
| [`Variants/Vector/Double3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double3.h) | 0 |
| [`Variants/Vector/Double4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Double4.h) | 0 |
| [`Variants/Vector/Float2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float2.h) | 0 |
| [`Variants/Vector/Float3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float3.h) | 0 |
| [`Variants/Vector/Float4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Float4.h) | 0 |
| [`Variants/Vector/Int2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int2.h) | 0 |
| [`Variants/Vector/Int3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int3.h) | 0 |
| [`Variants/Vector/Int4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Int4.h) | 0 |
| [`Variants/Vector/TVec.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/TVec.h) | 0 |
| [`Variants/Vector/UVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/UVec4.h) | 0 |
| [`Variants/Vector/Vec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec2.h) | 1 |
| [`Variants/Vector/Vec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec3.h) | 1 |
| [`Variants/Vector/Vec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec4.h) | 0 |
| [`Variants/Vector/iVec2.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec2.h) | 1 |
| [`Variants/Vector/iVec3.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec3.h) | 1 |
| [`Variants/Vector/iVec4.h`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec4.h) | 1 |

### Top-level subsystem headers (11 headers)

| Header | Export-marked declarations |
|---|---:|
| [`Moss.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss.h)` | 0 |
| [`Moss_Audio.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Audio.h)` | 113 |
| [`Moss_GPU.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_GPU.h)` | 241 |
| [`Moss_GUI.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_GUI.h)` | 73 |
| [`Moss_Navigation.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Navigation.h)` | 0 |
| [`Moss_Network.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Network.h)` | 146 |
| [`Moss_Physics.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Physics.h)` | 39 |
| [`Moss_Platform.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Platform.h)` | 181 |
| [`Moss_Renderer.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_Renderer.h)` | 195 |
| [`Moss_XR.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_XR.h)` | 41 |
| [`Moss_stdinc.h](https://github.com/TxbiG/Moss/blob/main/include/Moss/Moss_stdinc.h)` | 34 |

## Inline implementation header inventory (19 files)

These files provide inline/template definitions or companion implementation details. They are included here because the public API surface cannot be understood from declarations alone when these files contain required definitions.

| Inline header |
|---|
| [`Core/FixedSizeFreeList.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/FixedSizeFreeList.inl) |
| [`Core/JobSystem.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/JobSystem.inl) |
| [`Core/LockFreeHashMap.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/LockFreeHashMap.inl) |
| [`Core/Profiler.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Core/Profiler.inl) |
| [`Physics/Body/Body.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Physics/Body/Body.inl) |
| [`Variants/Matrix/DMat44.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/DMat44.inl) |
| [`Variants/Matrix/Mat44.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Matrix/Mat44.inl) |
| [`Variants/Quat.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Quat.inl) |
| [`Variants/Vector/BVec16.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/BVec16.inl) |
| [`Variants/Vector/DVec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec2.inl) |
| [`Variants/Vector/DVec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec3.inl) |
| [`Variants/Vector/DVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/DVec4.inl) |
| [`Variants/Vector/UVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/UVec4.inl) |
| [`Variants/Vector/Vec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec2.inl) |
| [`Variants/Vector/Vec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec3.inl) |
| [`Variants/Vector/Vec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/Vec4.inl) |
| [`Variants/Vector/iVec2.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec2.inl) |
| [`Variants/Vector/iVec3.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec3.inl) |
| [`Variants/Vector/iVec4.inl`](https://github.com/TxbiG/Moss/blob/main/include/Moss/Variants/Vector/iVec4.inl) |

## 9. Native integration details to preserve

This is a language-level API specification, not a mechanical ABI mirror. When implementing the projection, keep these source-level distinctions intact:

- Convert pointer-plus-count inputs into a single slice/span/list/array/table/sequence; do not expose a redundant count parameter when the collection already carries its length.
- Convert output pointer pairs (for example, mouse coordinates) into a point/tuple/value object. Do not expose C-style `int*` output arguments.
- Resource factories return owned language objects and translate native allocation/backend failure into the language's normal error model. Destroy/terminate functions become deterministic resource disposal (`Dispose`, `Drop`, `close`, or context-manager release), not ordinary public `destroy_*` calls.
- GUI calls should be scoped to a `GuiContext` instance in every managed binding. They must not silently depend on mutable global “current context” state when the public API can retain an explicit context.
- Vulkan handles, callback pointers, allocator callbacks and backend-specific opaque handles stay behind advanced interop APIs unless the user explicitly opts into native interop. They must not be confused with Moss-owned managed resources.
- Where the native declaration lacks information required by the intended safe API (for example a pointer/count pair, output values, or a resource owner), resolve the native declaration or binding-side contract before implementing that projection; do not guess buffer length or object lifetime.

## 10. Build and distribution contract

- Ship matching native binaries and binding artifacts for each supported OS/architecture combination.
- Keep ABI declarations internal; public API users should not need to know calling conventions, struct packing, pointer widths or native library search paths.
- Make backend and optional dependency feature flags visible through capability queries or build metadata.
- Test a clean build, import/load, initialization/shutdown, object lifetime, callback detach, error translation and resource ownership in CI for every supported platform.
- Keep the generated language API index in sync with the native CMoss export catalogue whenever public native declarations change.

## 11. Compatibility and versioning

Treat the binding's public API as a separately versioned language library. A native API addition does not become a public binding API until the type mapping, ownership, failure semantics and example are specified. Avoid leaking raw `Moss_*` function names into application code. Deprecate public methods using the target language's normal annotation mechanism and provide a migration note in the API Reference itself when a breaking change is unavoidable.

---

**Canonical authority:** Moss C++/CMoss headers and the CMoss API Reference supplied for this documentation set. The native symbol column preserves traceability; the public API column is the intended language-specific design.
