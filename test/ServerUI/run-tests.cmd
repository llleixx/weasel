@echo off
setlocal
rem Run from an x64 or x86 Visual Studio developer command prompt.
if not defined BOOST_ROOT (
  echo Set BOOST_ROOT to the Boost installation used to build Weasel.
  exit /b 1
)
if not defined VSCMD_ARG_TGT_ARCH (
  echo Run this script from a Visual Studio developer command prompt.
  exit /b 1
)
set "platform=x64"
if "%VSCMD_ARG_TGT_ARCH%"=="x86" set "platform=Win32"
pushd "%~dp0..\.."
set "out=msbuild\ServerUITests\%platform%"
if not exist "%out%" mkdir "%out%"
msbuild WeaselIPCServer\WeaselIPCServer.vcxproj /m /p:Configuration=Release /p:Platform=%platform% /p:SolutionDir="%CD%\\" /p:WindowsTargetPlatformVersion=10.0 /v:minimal
if errorlevel 1 goto fail
msbuild WeaselIPC\WeaselIPC.vcxproj /m /p:Configuration=Release /p:Platform=%platform% /p:SolutionDir="%CD%\\" /p:WindowsTargetPlatformVersion=10.0 /v:minimal
if errorlevel 1 goto fail
msbuild WeaselUI\WeaselUI.vcxproj /m /p:Configuration=Release /p:Platform=%platform% /p:SolutionDir="%CD%\\" /p:WindowsTargetPlatformVersion=10.0 /v:minimal
if errorlevel 1 goto fail
set "libs=msbuild\Release\%platform%\WeaselUI.lib msbuild\Release\%platform%\WeaselIPC.lib msbuild\Release\%platform%\WeaselIPCServer.lib Usp10.lib d2d1.lib dwrite.lib d3d11.lib dxgi.lib dcomp.lib shcore.lib user32.lib gdi32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib windowscodecs.lib comctl32.lib"
for %%T in (CommandDispatchTest NotificationTest StateTest ResponseSnapshotTest PositionTest NativePresentationTest) do (
  cl /nologo /std:c++17 /EHsc /O2 /Zi /MT /utf-8 /DUNICODE /D_UNICODE /Iinclude /I"%BOOST_ROOT%" /IWeaselIPCServer "test\ServerUI\%%T.cpp" /Fe:"%out%\%%T.exe" /Fo:"%out%\%%T.obj" /Fd:"%out%\%%T.pdb" /link /LTCG /DEBUG /MANIFEST:EMBED /MANIFESTUAC:"level='asInvoker' uiAccess='false'" /LIBPATH:"%BOOST_ROOT%\stage\lib" %libs%
  if errorlevel 1 goto fail
  powershell -NoProfile -ExecutionPolicy Bypass -File test\ServerUI\Run-Test.ps1 "%out%\%%T.exe"
  if errorlevel 1 goto fail
)
popd
exit /b 0
:fail
popd
exit /b 1
