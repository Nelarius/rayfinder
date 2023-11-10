# `.maxStorageBufferBindingSize`

_Written: 2023-11-10_

_Context: when doing texture lookup on the GPU for the first time, it was observed that a single texture buffer can exceed Dawn's internal storage buffer size limit._

Dawn's internal storage buffer limit is 256 MiB, or `1 << 28` bytes.

If your `wgpuQueueWriteBuffer` command exceeds this, a validation error will occur with the printed output revealing this limit. If you try to memory map a buffer over this limit, you will silently get a null pointer from the `*GetMappedRange` functions. The memory-mapped range is now `assert`ed in the `GpuBuffer` constructor.

It seems like, regardless of what you set as required limits you pass to `wgpuAdapterRequestDevice`, no errors occur. And requesting a larger `maxStorageBufferBindingSize` does nothing -- validation will still occur if the size exceeds `1 << 28` bytes.

## Dealing with `Sponza.glb`'s textures

Stored as `Rgbaf32` all in one buffer, Sponza's textures greatly exceeded the limit (384 MiB). It was necessary to change the internal pixel layout to `Rgbau8` to be able to even render the scene.
