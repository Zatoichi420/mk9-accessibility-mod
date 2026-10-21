@echo off
rem Builds the 32-bit dinput8.dll proxy and test_speech.exe into native\mk9hook\out.
setlocal
set VCVARS=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat
if not exist "%VCVARS%" set VCVARS=C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvarsall.bat
call "%VCVARS%" amd64_x86 >nul || exit /b 1
cd /d "%~dp0"
if not exist out mkdir out
set MH=third_party\minhook
set LIBS=user32.lib kernel32.lib ole32.lib oleaut32.lib
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 /DWIN32_LEAN_AND_MEAN /I %MH%\include ^
  src\dllmain.cpp src\announce.cpp src\input.cpp src\speech.cpp %MH%\src\hook.c %MH%\src\buffer.c %MH%\src\trampoline.c %MH%\src\hde\hde32.c ^
  /LD /Fo"out\\" /Fe"out\dinput8.dll" /link /DEF:src\exports.def %LIBS% || exit /b 1
cl /nologo /O2 /MT /EHsc /std:c++17 /W3 src\test_speech.cpp src\speech.cpp ^
  /Fo"out\\" /Fe"out\test_speech.exe" /link %LIBS% || exit /b 1
copy /y third_party\nvda\nvdaControllerClient.dll out\ >nul
echo Built out\dinput8.dll and out\test_speech.exe
