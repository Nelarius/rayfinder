# Accelerating builds with `ccache`

_Written: 2024-03-21_

Use `ccache` to cache the object files and massively accelerate rebuilding Dawn from source.

## macOS

**Install**

```sh
$ brew install ccache
```

**Configure**

```sh
# Set unlimited file number cap
$ ccache -F 0
# Set 128 GB cache size
$ ccache -M 128GB
```

**VsCode**

From the command pallette, select "CMake: Edit User-Local CMake Kits" to edit the local kits. Add the following entry to the list:

```json
  {
    "name": "ccache",
    "compilers": {
      "C": "/opt/homebrew/opt/ccache/libexec/clang",
      "CXX": "/opt/homebrew/opt/ccache/libexec/clang++"
    },
    "isTrusted": true
  }
```

**CLI**

Build the project by setting CMake's `CMAKE_<LANG>_COMPILER_LAUNCHER` variable before building like usual:

```sh
$ export CMAKE_C_COMPILER_LAUNCHER=ccache
$ export CMAKE_CXX_COMPILER_LAUNCHER=ccache
$ cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug
```
