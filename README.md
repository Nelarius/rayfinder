# README

An experimental pathtracer, implemented using WebGPU via the [Dawn](https://dawn.googlesource.com/dawn) renderer.

## Build

A C++20 compiler is required. On macOS, fetching a newer compiler via homebrew was required:

```sh
$ brew install llvm
$ export CXX=/opt/homebrew/opt/llvm@17/bin/clang++ 
$ export C=/opt/homebrew/opt/llvm@17/bin/clang 
$ cmake -B build-debug -S . -DCMAKE_BUILD_TYPE=Debug
$ cmake --build build-debug --target pt -- -j 32
```

## Run

### `tests`

To run the tests, the working directory has to be in the build folder, as some sample assets are copied there and used for tests.

```sh
$ (cd build-debug && ./tests)
```
