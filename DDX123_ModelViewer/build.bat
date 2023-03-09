@echo off

:: call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Change directory to the build folder
mkdir C:\dev\DDX123\DDX123_ModelViewer\build
pushd C:\dev\DDX123\DDX123_ModelViewer\build

:: If program is running in the debugger, then stop it so the compiler can write to the .exe
:: remedybg.exe stop-debugging

:: Compile Shaders
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\FXC.exe" /Od /Zi /T vs_5_1 /enable_unbounded_descriptor_tables /Fo VertexShader.cso ..\code\VertexShader.hlsl
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\FXC.exe" /Od /Zi /T ps_5_1 /enable_unbounded_descriptor_tables /Fo PixelShader.cso  ..\code\PixelShader.hlsl
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\FXC.exe" /Od /Zi /T vs_5_1 /enable_unbounded_descriptor_tables /Fo PBRVertexShader.cso ..\code\PBRVertexShader.hlsl
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.22621.0\x64\FXC.exe" /Od /Zi /T ps_5_1 /enable_unbounded_descriptor_tables /Fo PBRPixelShader.cso  ..\code\PBRPixelShader.hlsl

:: Old flags: /Yc"pch.h"
set flags= /FeDDX123 /FAs /EHsc
set code=..\code\main.cpp ..\code\model.cpp ..\..\third_party\imgui\imgui.cpp ..\..\third_party\imgui\imgui_draw.cpp ..\..\third_party\imgui\imgui_demo.cpp ..\..\third_party\imgui\imgui_tables.cpp ..\..\third_party\imgui\imgui_widgets.cpp ..\..\third_party\imgui\backends\imgui_impl_dx12.cpp ..\..\third_party\imgui\backends\imgui_impl_win32.cpp
set includes=/I"C:\dev\DDX123\third_party\DirectXTex\DirectXTex" /I"C:\dev\DDX123\DDX123_Lib\code" /I"C:\dev\d_core" /I"C:\dev\DirectX\DirectXTK12\Inc" /I"C:\dev\DDX123\third_party\tinygltf" /I"C:\dev\DDX123\third_party\imgui" /I"C:\dev\DDX123\third_party\imgui\backends"
set link_libs= Winmm.lib d3d12.lib dxgi.lib dxguid.lib D3Dcompiler.lib kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib odbc32.lib odbccp32.lib C:\dev\DDX123\DDX123_Lib\build\d_dx12.lib

:: Compile App
if "%1" == "-d" (

    :: Compile d_dx12 lib
    C:\dev\DDX123\DDX123_Lib\build.bat -d

    :: Compile this
    cl /Zi /DDEBUG /D_DEBUG /MDd %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Debug\DirectXTex.lib 

    popd

) else if "%1" == "-rds" (

    :: Compile d_dx12 lib
    C:\dev\DDX123\DDX123_Lib\build.bat -rds

    :: Compile this
    cl /MD /Zi %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Profile\DirectXTex.lib

    popd

) else (

    :: Compile d_dx12 lib
    C:\dev\DDX123\DDX123_Lib\build.bat

    :: Compile this
    cl /MD %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Release\DirectXTex.lib

    popd
)