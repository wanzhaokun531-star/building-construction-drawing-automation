@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.16 >nul
if errorlevel 1 exit /b 1
cl /nologo /EHsc /std:c++17 /W4 /I"src" /Fe:"diagnostics\bin\public_text_policy_smoke.exe" /Fo:"diagnostics\obj\public_text_policy_smoke.obj" "..\tests\public_text_policy_smoke.cpp"
if errorlevel 1 exit /b 1
"diagnostics\bin\public_text_policy_smoke.exe"