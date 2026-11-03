@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.16
if errorlevel 1 exit /b %errorlevel%
cl /nologo /EHsc /std:c++17 /W4 /WX /utf-8 /Fe:acceptance.exe /Fo:acceptance.obj "../../tests/native/unitconfig/PrintedCommandAcceptanceTests.cpp"
if errorlevel 1 exit /b %errorlevel%
acceptance.exe
exit /b %errorlevel%
