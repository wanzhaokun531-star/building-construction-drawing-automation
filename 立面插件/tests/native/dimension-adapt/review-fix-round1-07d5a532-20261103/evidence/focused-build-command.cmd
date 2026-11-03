@echo off
call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" -vcvars_ver=14.16
if errorlevel 1 exit /b %errorlevel%
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe" ElevationDimensionAdaptCases.vcxproj /t:Rebuild /p:Configuration=Release /p:Platform=x64 /p:VCToolsVersion=14.16.27023 /m:1 /nologo /verbosity:normal
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MD /W4 /WX /utf-8 DimensionTopologyTests.cpp /Fe"..\out\DimensionTopologyTests.exe"
if errorlevel 1 exit /b %errorlevel%
"..\out\DimensionTopologyTests.exe"
if errorlevel 1 exit /b %errorlevel%
cl /nologo /std:c++17 /EHsc /MD /W4 /WX /utf-8 RejectionAcceptanceTests.cpp /Fe"..\out\RejectionAcceptanceTests.exe"
if errorlevel 1 exit /b %errorlevel%
"..\out\RejectionAcceptanceTests.exe"
exit /b %errorlevel%

