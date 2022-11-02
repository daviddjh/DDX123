@echo off

mkdir C:\dev\DDX123\DDX123_Lib\build
pushd C:\dev\DDX123\DDX123_Lib\build

set flags=/Fed_dx12 /MDd /Yc"pch.h" /FAs /EHsc /c
set code=..\code\d_dx12.cpp
set includes=/I"C:\dev\DDX123\DDX123_Lib\code" /I"C:\dev\d_common" /I"C:\dev\DirectX\DirectXTK12\Inc"
set link_libs= d3d12.lib dxgi.lib dxguid.lib D3Dcompiler.lib kernel32.lib user32.lib gdi32.lib winspool.lib shell32.lib ole32.lib oleaut32.lib uuid.lib comdlg32.lib advapi32.lib odbc32.lib odbccp32.lib C:\dev\DirectX\DirectXTK12\Bin\Desktop_2019_Win10\x64\Debug\DirectXTK12.lib

:: Compile App
if "%1" == "-d" (

    :: Compile
    cl /Zi /D_DEBUG %flags% %code% %includes%

    :: Link to static lib
    lib -out:d_dx12.lib d_dx12.obj

) else if "%1" == "-rds" (

    :: Compile
    cl /Zi %flags% %code% %includes%

    :: Link to static lib
    lib -out:d_dx12.lib d_dx12.obj

) else (

    :: Compile
    cl %flags% %code% %includes%

    :: Link to static lib
    lib -out:d_dx12.lib d_dx12.obj

)
popd