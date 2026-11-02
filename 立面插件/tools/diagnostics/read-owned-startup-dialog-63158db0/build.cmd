@echo off
call "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/VC/Auxiliary/Build/vcvars64.bat" -vcvars_ver=14.16 >nul
if errorlevel 1 exit /b %errorlevel%
cl.exe /nologo /std:c++17 /EHsc /MT /O2 /W4 /utf-8 /DUNICODE /D_UNICODE ReadOwnedStartupDialog.cpp /Fe"ReadOwnedStartupDialog.exe" /Fo"ReadOwnedStartupDialog.obj" /link user32.lib /SUBSYSTEM:CONSOLE /INCREMENTAL:NO
exit /b %errorlevel%
