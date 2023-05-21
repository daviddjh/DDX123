@echo off

:: Change directory to the build folder
mkdir C:\dev\DDX123\build
pushd C:\dev\DDX123\build

:: If program is running in the debugger, then stop it so the compiler can write to the .exe
:: remedybg.exe stop-debugging

set flags= /FeDDX123 /FAs /EHsc
set code=..\code\main.cpp ..\third_party\imgui\imgui.cpp ..\third_party\imgui\imgui_draw.cpp ..\third_party\imgui\imgui_demo.cpp ..\third_party\imgui\imgui_tables.cpp ..\third_party\imgui\imgui_widgets.cpp ..\third_party\imgui\backends\imgui_impl_dx12.cpp ..\third_party\imgui\backends\imgui_impl_win32.cpp
set includes=/I"C:\dev\DDX123\third_party\DirectXTex\DirectXTex" /I"C:\dev\DDX123\code\d_core" /I"C:\dev\DDX123\code\d_dx12" /I"C:\dev\DirectX\DirectXTK12\Inc" /I"C:\dev\DDX123\third_party\tinygltf" /I"C:\dev\DDX123\third_party\imgui" /I"C:\dev\DDX123\third_party\imgui\backends"
set link_libs= Winmm.lib d3d12.lib dxgi.lib dxguid.lib dxcompiler.lib kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib odbc32.lib odbccp32.lib

:: Compile App
if "%1" == "-d" (

    :: Compile this
    cl /Zi /DDEBUG /D_DEBUG /MDd %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Debug\DirectXTex.lib 

    popd

) else if "%1" == "-rds" (

    :: Compile this
    cl /MD /Zi %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Profile\DirectXTex.lib

    popd

) else if "%1" == "-o" (

    :: Compile this
    cl /MD /O2 %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Profile\DirectXTex.lib

    popd

) else (

    :: Compile this
    cl /MD %flags% %code% %includes% %link_libs% C:\dev\DDX123\third_party\DirectXTex\DirectXTex\Bin\Desktop_2019_Win10\x64\Release\DirectXTex.lib

    popd
)