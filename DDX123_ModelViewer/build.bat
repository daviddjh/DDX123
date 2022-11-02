@echo off

:: call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Enterprise\VC\Auxiliary\Build\vcvarsall.bat" x64

:: Change directory to the build folder
mkdir C:\dev\DDX123\DDX123_ModelViewer\build
pushd C:\dev\DDX123\DDX123_ModelViewer\build

:: If program is running in the debugger, then stop it so the compiler can write to the .exe
:: remedybg.exe stop-debugging

:: Compile Shaders
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\FXC.exe" /Od /Zi /T vs_5_1 /Fo VertexShader.cso ..\code\VertexShader.hlsl
"C:\Program Files (x86)\Windows Kits\10\bin\10.0.19041.0\x64\FXC.exe" /Od /Zi /T ps_5_1 /Fo PixelShader.cso  ..\code\PixelShader.hlsl

set flags=/FeDDX123 /MDd /Yc"pch.h" /FAs /EHsc /D_UNICODE
set code=..\code\main.cpp
set includes=/I"C:\dev\DDX123\DDX123_Lib\code" /I"C:\dev\d_common" /I"C:\dev\DirectX\DirectXTK12\Inc"
set link_libs= d3d12.lib dxgi.lib dxguid.lib D3Dcompiler.lib kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib odbc32.lib odbccp32.lib C:\dev\DirectX\DirectXTK12\Bin\Desktop_2019_Win10\x64\Debug\DirectXTK12.lib C:\dev\DDX123\DDX123_Lib\build\d_dx12.lib

:: Compile App
if "%1" == "-d" (

    :: Compile d_dx12 lib
    C:\dev\DDX123\DDX123_Lib\build.bat -d

    :: Compile this
    cl /Zi /D_DEBUG %flags% %code% %includes% %link_libs%

) else if "%1" == "-rds" (

    echo Release with debug symbols!

    :: Compile d_dx12 lib
    C:\dev\DDX123\DDX123_Lib\build.bat -rds

    :: Compile this
    cl %flags% %code% %includes% %link_libs%

) else (

    :: Compile d_dx12 lib
    C:\dev\DDX123\DDX123_Lib\build.bat

    :: Compile this
    cl %flags% %code% %includes% %link_libs%

)
popd