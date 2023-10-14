# DDX123

Toy renderer I use to learn about graphics programming.

To Build:
- Ensure git submodules are downloaded. `git submodule update --init`
- Ensure you're running in a Visual Studio Developer Command Prompt
- Build DirectXTex: `build.bat --DirectXTex2022` or `build.bat --DirectXTex2019` depending on your Visual Studio build tools version. Only needs to be done once.
- Build DDX123: `build.bat`
  - `build.bat -o`   - Optimized with cl flag: /O2
  - `build.bat -d`   - Debug build (Default)
  - `build.bat -ods` - Release build with debug symbols

## Lowered CPU frame time from 146µs to 77µs - 9/28/23

This started by wanting to remove stdlib `map` calls during runtime.
I ended up moving most of it to compile time. I also removed std::string from my binding system. Also added comiler options for speed. Shaved off 69µs on average.


![CPU Frame time](pics/CPU_FRAME_TIME.png) 

## Deferred Rendering in [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) Scene - 9/28/23 

![Deferred Rendering in Sponza Scene](pics/Deferred_rendering.png)

## Shadow Maps in [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) Scene - 4/5/23 

![Shadow Maps in Sponza Scene](pics/shadow_mapping_2.png)

## Physically Based Rendering in [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) Scene - 3/10/23 

![Physically Based Rendering in Sponza Scene](pics/PBR_Sponza.png)

## Phong Shading / Normal Mapping in [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) Scene - 2/10/23 

![Mipmapping with Sponza Scene](pics/phong_sponza.png)

## Mipmapping [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) Scene - 1/30/23 

![Mipmapping with Sponza Scene](pics/mipmapped_sponza.png)

## Loading [Sponza](https://github.com/KhronosGroup/glTF-Sample-Models/tree/master/2.0/Sponza) Scene - 1/25/23 

![Sponza Scene](pics/loading_sponza.png)
