@echo off

:: Env variables and "current directory" we change will reset after script is over
setlocal

:: Change directory to Batch file location. %0 = batch file location. "~dp" = Parameter extensions to extract drive and path
:: https://stackoverflow.com/questions/17063947/get-current-batchfile-directory
cd /d %~dp0

:: Change directory to the build folder
mkdir .\build
pushd .\build

:: If program is running in the debugger, then stop it so the compiler can write to the .exe
:: remedybg.exe stop-debugging

set flags= /FeDDX123 /FAs /EHsc
set code=..\code\main.cpp ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_demo.cpp ..\third_party\imgui\imgui_tables.cpp ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\backends\imgui_impl_dx12.cpp ..\third_party\imgui\backends\imgui_impl_win32.cpp
set includes=/I"..\third_party\DirectXTex\DirectXTex" /I"..\code\d_core" /I"..\code\d_dx12" /I"..\third_party\DirectXTK12\Inc" /I"..\third_party\tinygltf" /I"..\third_party\imgui" /I"..\third_party\imgui\backends" /I"..\third_party\dxc\inc"
set link_libs= Winmm.lib d3d12.lib dxgi.lib dxguid.lib ole32.lib oleaut32.lib Advapi32.lib ..\third_party\dxc\lib\x64\dxcompiler.lib

:: Compile our app, or DirectXTex
if "%1" == "-d" (

    :: Compile this
    cl /Zi /DDEBUG /D_DEBUG /MDd %flags% %code% %includes% %link_libs% ..\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Debug\DirectXTex.lib 

    popd

) else if "%1" == "-ods" (

    :: Compile this
    cl /MD /Zi /O2 /DPCOUNTER /fp:fast %flags% %code% %includes% %link_libs%  ..\third_party\Superluminal\lib\x64\PerformanceAPI_MD.lib ..\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Profile\DirectXTex.lib /link /OPT:REF,ICF 

    popd

) else if "%1" == "-o" (

    :: Compile this
    cl /MD /O2 /fp:fast %flags% %code% %includes% %link_libs% ..\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Release\DirectXTex.lib /link /OPT:REF,ICF

    popd

) else if "%1" == "--DirectXTex2022" (

    msbuild ..\third_party\DirectXTex\DirectXTex_Desktop_2022.sln /p:Configuration="Release" /p:Platform="x64"
    msbuild ..\third_party\DirectXTex\DirectXTex_Desktop_2022.sln /p:Configuration="Debug" /p:Platform="x64"
    msbuild ..\third_party\DirectXTex\DirectXTex_Desktop_2022.sln /p:Configuration="Profile" /p:Platform="x64"
    
    popd

) else if "%1" == "--DirectXTex2019" (

    msbuild ..\third_party\DirectXTex\DirectXTex_Desktop_2019.sln /p:Configuration="Release" /p:Platform="x64"
    msbuild ..\third_party\DirectXTex\DirectXTex_Desktop_2019.sln /p:Configuration="Debug" /p:Platform="x64"
    msbuild ..\third_party\DirectXTex\DirectXTex_Desktop_2019.sln /p:Configuration="Profile" /p:Platform="x64"

    popd

) else (

    :: Compile this
    cl /Zi /DDEBUG /D_DEBUG /MDd %flags% %code% %includes% %link_libs% ..\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Debug\DirectXTex.lib 

    popd
)