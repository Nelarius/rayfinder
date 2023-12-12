# README

An experimental pathtracer, implemented using WebGPU via the [Dawn](https://dawn.googlesource.com/dawn) renderer.

## References

- [WGSL offset computer](https://webgpufundamentals.org/webgpu/lessons/resources/wgsl-offset-computer.html)
  - Great tool for checking correct memory layout
- _Physically Based Rendering, fourth edition_
  - Bounding volume hierarchy
  - Inverse transform sampling method
- [Ray Tracing: The Rest of Your Life](https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html)
  - Cosine-weighted hemisphere sampling method
- [scratchapixel/moller-trumbore-ray-triangle-intersection](https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html)
- _The Reference Path Tracer, Ray Tracing Gems II_ and associated code sample [boksajak/referencePT](https://github.com/boksajak/referencePT/)
  - Rng initialization
- [Crash Course in BRDF implementation](https://boksajak.github.io/files/CrashCourseBRDF.pdf)
  - BRDF interface
  - Lambertian BRDF, `sampleLambertian`, `evalLambertian` 
- [Building an Orthonormal Basis, Revisited](https://www.jcgt.org/published/0006/01/01/paper-lowres.pdf)
  - Robust orthonormal axis, implemented in the `pixarOnb` function
- [ACES Filmic Tone Mapping Curve](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/)
  - filmic tonemapping function

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

For validating that the bounding volume hierarchy (BVH) and it's intersection tests are computed correctly. This executable loads the specified glTF file, builds a BVH, and produces an image where each pixel is colored by the number of nodes visited for the pixel's primary ray. Running the executable products the test image `bvh-visualizer.png`.

```sh
$ ./build-release/bvh-visualizer assets/Duck.glb
```

### `hw-skymodel-demo`

Running the `hw-skymodel-demo` target generates a test image `hw-skymodel-demo.png`.

### `tests`

To run the tests, the working directory has to be in the build folder. The tests depend on assets which are copied to the build folder.

```sh
$ (cd build-debug && ./tests)
```

### `textractor`

For validating that textures are loaded correctly. Loads the specified glTF model and its base color textures, and dumps the textures into `.png` files.

```sh
$ ./build-release/textractor assets/Sponza.png
```
