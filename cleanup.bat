@echo off

:: Env variables and "current directory" we change will reset after script is over
setlocal

:: Change directory to Batch file location. %0 = batch file location. "~dp" = Parameter extensions to extract drive and path
:: https://stackoverflow.com/questions/17063947/get-current-batchfile-directory
cd /d %~dp0

:: Change directory to the build folder
mkdir .\build
pushd .\build

DEL /Q /F /S "*.pdb"
DEL /Q /F /S "*.asm"
DEL /Q /F /S "*.i"
DEL /Q /F /S "*.obj"
DEL /Q /F /S "*.bin"
DEL /Q /F /S "*.cso"
DEL /Q /F /S "*.ilk"

popd