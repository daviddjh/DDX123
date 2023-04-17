@echo off

mkdir C:\dev\DDX123\DDX123_Lib\build
pushd C:\dev\DDX123\DDX123_Lib\build

set flags=/Fed_dx12 /FAs /EHsc /c
set code=..\code\d_dx12.cpp
set includes=/I"C:\dev\DDX123\third_party\DirectXTex\DirectXTex" /I"C:\dev\DDX123\DDX123_Lib\code" /I"C:\dev\d_core" /I"C:\dev\DirectX\DirectXTK12\Inc"

:: Compile App
if "%1" == "-d" (

    :: Compile
    cl /Zi /D_DEBUG /MDd %flags% %code% %includes%

    :: Link to static lib
    lib -out:d_dx12.lib d_dx12.obj

) else if "%1" == "-rds" (

    :: Compile
    cl /Zi /MD %flags% %code% %includes%

    :: Link to static lib
    lib -out:d_dx12.lib d_dx12.obj

) else (

    :: Compile
    cl /MD %flags% %code% %includes%

    :: Link to static lib
    lib -out:d_dx12.lib d_dx12.obj

)
popd