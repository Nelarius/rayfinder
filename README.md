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

For validating that the bounding volume hierarchy (BVH) and it's intersection tests are computed correctly. This executable loads the specified glTF file, builds a BVH, and produces an image where each pixel is colored by the number of nodes visited for the pixel's primary ray. The image is printed in the PPM format to stdout.

```sh
$ ./build-release/bvh-visualizer assets/Duck.glb > duck.ppm
```

### `tests`

To run the tests, the working directory has to be in the build folder. The tests depend on assets which are copied to the build folder.

```sh
$ (cd build-debug && ./tests)
```

### `textractor`

For validating that textures are loaded correctly. Loads the specified glTF model and its base color textures, and dumps the textures into `.ppm` files.

```sh
$ ./build-release/textractor assets/Sponza.ppm
```
