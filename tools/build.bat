@echo off
rem Usage: tools\build.bat [target]   (ARRecomp, ARLauncher, ARShaderTool; everything when omitted)
rem Configures out\build on first use: Ninja, clang-cl, RelWithDebInfo.
rem LLVM_DIR names a portable LLVM. Without it, ..\_tools\llvm next to this project is used if it's there, otherwise clang-cl from the PATH.
set "PATH=C:\Program Files (x86)\Microsoft Visual Studio\Installer;%PATH%"
set "VSROOT="
rem Visual Studio 2022 if it's there, as the project is built and tested with it (mixing compilers breaks an existing build); otherwise the newest.
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
for /f "usebackq delims=" %%i in (`vswhere.exe -latest -products * -version "[17.0,18.0)" -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT echo Visual Studio with its C++ tools wasn't found.& exit /b 1
call "%VSROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul || exit /b 1
if not defined LLVM_DIR if exist "%~dp0..\..\_tools\llvm\bin\clang-cl.exe" set "LLVM_DIR=%~dp0..\..\_tools\llvm"
if defined LLVM_DIR set "PATH=%LLVM_DIR%\bin;%PATH%"
cd /d "%~dp0.."
if not exist out\build\build.ninja cmake -S . -B out\build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_C_COMPILER=clang-cl -DCMAKE_CXX_COMPILER=clang-cl || exit /b 1
if "%1"=="" (cmake --build out\build -- -k 0) else (cmake --build out\build --target %1 -- -k 0)
