@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat" x64 -vcvars_ver=14.16 >nul
if errorlevel 1 exit /b 1
if not exist bin mkdir bin
if not exist build mkdir build
cl /nologo /EHsc /std:c++17 /utf-8 /W4 /Isrc /Febin\AxisFacadeRegression.exe /Fobuild\ src\PlanRules.cpp src\GroupEndpoints.cpp src\WallTopology.cpp tests\AxisFacadeRegression.cpp
if errorlevel 1 exit /b 1
bin\AxisFacadeRegression.exe inputs\frozen-scheme-v2.tsv inputs\user-axis-exclusions-v1.tsv
