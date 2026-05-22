# Feature Gaps

## Metal renderer parity gaps

- Geometry shader fallback rendering is not implemented. The renderer can recognize the normal-line visualizer pattern used by existing `.geom` samples, but it still reports that the Metal fallback renderer is missing. Arbitrary geometry shaders remain unsupported because Metal has no geometry shader stage.
- Vulkan ray shader parity is not implemented. Metal can build acceleration structures for `build_as` models and can run native `.metal` ray kernels, but `.rgen`, `.rmiss`, `.rchit`, shader groups, shader binding tables, miss/hit groups, and procedural ray groups are not translated to Metal.
- Metal ray tracing is still device and OS gated. Scenes that use the native Metal ray path require macOS 11 or newer and a device where `supportsRaytracing` is true.
- General descriptor parity is incomplete for Metal raster passes. Supported bindings are currently the default pass UBO, named sampled surfaces, and the set 2 model material resources. Extra UBOs, storage buffers, storage images, texel buffers, input attachments, and arbitrary descriptor arrays are rejected.
- HDR or wide-color presentation is not implemented for the Metal swap layer; presentation is currently `BGRA8Unorm`.
