# README

An experimental pathtracer, implemented using WebGPU via the [Dawn](https://dawn.googlesource.com/dawn) renderer.

## References

- [WGSL offset computer](https://webgpufundamentals.org/webgpu/lessons/resources/wgsl-offset-computer.html)
  - Great tool for checking correct memory layout
- _Physically Based Rendering, fourth edition_
  - Bounding volume hierarchy
- [scratchapixel/moller-trumbore-ray-triangle-intersection](https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html)
- _The Reference Path Tracer, Ray Tracing Gems II_ and associated code sample [boksajak/referencePT](https://github.com/boksajak/referencePT/)
  - Rng initialization

## Build

A C++20 compiler is required. On macOS, fetching a newer compiler via homebrew was required:

```sh
$ brew install llvm
$ export CXX=/opt/homebrew/opt/llvm@17/bin/clang++ 
$ export CC=/opt/homebrew/opt/llvm@17/bin/clang 
$ cmake -B build-debug -S . -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build-debug --target pt -- -j 32
```

## Run

### `bvh-visualizer`

This executable loads a glTF file, builds a bounding volume hierarchy (BVH), and produces an image where each pixel is colored by the number of nodes visited for the pixel's primary ray. The image is printed in the PPM format to stdout.

```sh
$ (cd build-release && ./bvh-visualizer) > duck.ppm
```

### `tests`

To run the tests, the working directory has to be in the build folder, as some sample assets are copied there and used for tests.

```sh
$ (cd build-debug && ./tests)
```
