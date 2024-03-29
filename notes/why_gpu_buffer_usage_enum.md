# Why `GpuBufferUsage` enum and not others?

_Written: 2024-03-29_

This [commit](https://github.com/Nelarius/rayfinder/commit/70c89b91c590da381599c7da761d1228138d73b2) introduced an enum class wrapper for the `WGPUBufferUsage` enum. Why did this particular WebGPU enum deserve a C++ wrapper and not others? After all, the philosophy so far has been to have RAII wrappers which interact directly with WebGPU constructs, and not other abstractions.

The short answer is that the enum contains additional values that the WebGPU equivalent does not and thus offers additional functionality.

`GpuBufferUsage` is a superset of `WGPUBufferUsage`. It contains the additional `GpuBufferUsage::ReadOnlyStorage` value that `WGPUBufferUsage` does not. We can thus discriminate between the two usages when setting the `WGPUBufferBindingType` in `GpuBuffer::BindGroupLayoutEntry`. This is what enables the use of of read-only storage buffers.
