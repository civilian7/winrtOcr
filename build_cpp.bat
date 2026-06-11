@echo off
rem Build sc_ocr.dll (x64) and sc_ocr32.dll (x86)
rem MSVC + C++/WinRT, static CRT (/MT) - no runtime dependency
setlocal

set VCVARSALL="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
set CLFLAGS=/nologo /std:c++20 /EHsc /O2 /MT /W4 /utf-8 /DUNICODE /D_UNICODE
set LINKLIBS=WindowsApp.lib shcore.lib

cd /d "%~dp0"
if not exist bin mkdir bin
if not exist cpp\obj64 mkdir cpp\obj64
if not exist cpp\obj32 mkdir cpp\obj32

rem ---- x64 ----
setlocal
call %VCVARSALL% x64 >nul
if errorlevel 1 (
  echo [ERROR] vcvarsall x64 failed
  exit /b 1
)
cl %CLFLAGS% /Focpp\obj64\ cpp\sc_ocr.cpp /link /DLL /DEF:cpp\sc_ocr.def /IMPLIB:cpp\obj64\sc_ocr.lib /OUT:bin\sc_ocr.dll %LINKLIBS%
if errorlevel 1 (
  echo [ERROR] sc_ocr.dll x64 build failed
  exit /b 1
)
endlocal

rem ---- x86 ----
setlocal
call %VCVARSALL% x86 >nul
if errorlevel 1 (
  echo [ERROR] vcvarsall x86 failed
  exit /b 1
)
cl %CLFLAGS% /Focpp\obj32\ cpp\sc_ocr.cpp /link /DLL /DEF:cpp\sc_ocr.def /IMPLIB:cpp\obj32\sc_ocr32.lib /OUT:bin\sc_ocr32.dll %LINKLIBS%
if errorlevel 1 (
  echo [ERROR] sc_ocr32.dll x86 build failed
  exit /b 1
)
endlocal

echo [OK] sc_ocr.dll (x64) + sc_ocr32.dll (x86) build succeeded
endlocal
