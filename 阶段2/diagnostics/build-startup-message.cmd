@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.16 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /FeOwnedStartupMessageRead.exe /FoOwnedStartupMessageRead.obj OwnedStartupMessageRead.cpp user32.lib
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /FeOwnedKnownUpdateErrorClose.exe /FoOwnedKnownUpdateErrorClose.obj OwnedKnownUpdateErrorClose.cpp user32.lib
