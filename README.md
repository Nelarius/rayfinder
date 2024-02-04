# rayfinder

An interactive experimental pathtracer, implemented using WebGPU via the [Dawn](https://dawn.googlesource.com/dawn) renderer.

## Showcase

![sponza-1](/img/sponza-1.png)

![sponza-2](/img/sponza-2.png)

## References

- [WGSL offset computer](https://webgpufundamentals.org/webgpu/lessons/resources/wgsl-offset-computer.html)
  - Great tool for checking correct memory layout
- _Physically Based Rendering, fourth edition_
  - Bounding volume hierarchy
  - Inverse transform sampling method
- [Ray Tracing: The Rest of Your Life](https://raytracing.github.io/books/RayTracingTheRestOfYourLife.html)
  - Cosine-weighted hemisphere sampling method
- [scratchapixel/moller-trumbore-ray-triangle-intersection](https://www.scratchapixel.com/lessons/3d-basic-rendering/ray-tracing-rendering-a-triangle/moller-trumbore-ray-triangle-intersection.html)
- _The Reference Path Tracer_, _Ray Tracing Gems II_ and associated code sample [boksajak/referencePT](https://github.com/boksajak/referencePT/)
  - Rng initialization
  - Next event estimation (direct light sampling) method
- [Crash Course in BRDF implementation](https://boksajak.github.io/files/CrashCourseBRDF.pdf)
  - BRDF interface
  - Lambertian BRDF, `sampleLambertian`, `evalLambertian` 
- [Building an Orthonormal Basis, Revisited](https://www.jcgt.org/published/0006/01/01/paper-lowres.pdf)
  - Robust orthonormal axis, implemented in the `pixarOnb` function
- _A Fast and Robust Method for Avoiding Self-Intersection_, _Ray Tracing Gems_
  - Method for the `offsetRay` function, for preventing ray self-intersections.
- _Sampling Transformations Zoo_, _Ray Tracing Gems_
  - The `rngNextInCone` method and source
- [ACES Filmic Tone Mapping Curve](https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/)
  - filmic tonemapping function
- [Simple Analytic Approximations to the CIE XYZ Color Matching Functions](https://jcgt.org/published/0002/02/01/)
  - C code for the CIE XYZ color matching functions' multi-lobe Gaussian fit
- [RGB / XYZ conversion matrices](http://www.brucelindbloom.com/index.html?Eqn_RGB_XYZ_Matrix.html)
  - XYZ to sRGB conversion matrix
- [Sky Dome Appearance Project](https://cgg.mff.cuni.cz/projects/SkylightModelling/)
  - The original source of the sky and solar disk model
  - The following [Rust implementation](https://github.com/phoekz/hw-skymodel) serves as the basis of the much simplified C code used in this project.

## Build

A C++20 compiler is required.

```sh
$ cmake -B build-debug -S . -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build-debug --target pt -- -j 32
```

## Run

### `pt` and `pt-format-tool`

The `pt` executable contains the path tracer. You run it by providing it with input file in the `pt` file format. `.pt` files are generated from a glTF file using the `pt-format-tool` executable.

```sh
# Running the path tracer requires input data generated from a gltf file.
$ ./build-release/pt-format-tool assets/Sponza.glb
# Run the path tracer with the resulting .pt file.
$ ./build-release/pt assets/Sponza.pt
```

### `bvh-visualizer`

For validating that the bounding volume hierarchy (BVH) and it's intersection tests are computed correctly. This executable loads the specified glTF file, builds a BVH, and produces an image where each pixel is colored by the number of nodes visited for the pixel's primary ray. Running the executable produces the test image `bvh-visualizer.png`.

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
$ ./build-release/textractor assets/Sponza.glb
```
