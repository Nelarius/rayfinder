# Using `CMakePreset.json` to build with x64 architexture on windows 

_Written: 2024-04-06_

Visual Studio Code's CMake Tools extension seems to build using `-A Win32` by default. This can be overriden using CMake presets. Presets can be enabled by adding `"cmake.useCMakePresets": "always"` to the local `settings.json`.

Now CMake Tools will prompt for a preset if one doesn't already exist. The following snippet is a working example of a preset:

```json
{
    "version": 2,
    "configurePresets": [
        {
            "name": "x64",
            "displayName": "Visual Studio Community 2022 - amd64",
            "description": "Using compilers for Visual Studio 17 2022 (x64 architecture)",
            "generator": "Visual Studio 17 2022",
            "toolset": "host=x64",
            "architecture": "x64",
            "binaryDir": "${sourceDir}/build/${presetName}",
            "cacheVariables": {
                "CMAKE_INSTALL_PREFIX": "${sourceDir}/install/${presetName}",
                "CMAKE_C_COMPILER": "cl.exe",
                "CMAKE_CXX_COMPILER": "cl.exe"
            }
        }
    ],
    "buildPresets": [
        {
            "name": "x64-debug",
            "displayName": "amd64 - Debug",
            "configurePreset": "x64",
            "configuration": "Debug"
        },
        {
            "name": "x64-release",
            "displayName": "amd64 - Release",
            "configurePreset": "x64",
            "configuration": "Release"
        }
    ]
}
```
