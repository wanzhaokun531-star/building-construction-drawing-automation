@echo off
setlocal
pushd "%~dp0"
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.16 >nul
if errorlevel 1 exit /b 1
if not exist bin mkdir bin
if not exist build mkdir build
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /WX /Isrc /Febin\ApronGeometryTests.exe /Fobuild\ src\ApronGeometry.cpp tests\ApronGeometryTests.cpp
if errorlevel 1 exit /b 1
bin\ApronGeometryTests.exe
set "apron_test_exit=%errorlevel%"
popd
exit /b %apron_test_exit%
