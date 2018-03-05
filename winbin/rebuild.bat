@echo off
setlocal EnableDelayedExpansion

if not exist chenard.sln (
    echo ERROR: Cannot find chenard.sln
    exit /b 1
)

if not defined VCINSTALLDIR (
    call "C:\Program Files (x86)\Microsoft Visual Studio 14.0\VC\vcvarsall.bat" x86_amd64
    echo Build environment set for Visual Studio 2015.
)

if not exist logs (
    mkdir logs
    if not exist logs (
        echo ERROR: cannot create 'logs' directory.
        exit /b 1
    )
    echo Created 'logs' directory.
)

for %%p in (Win32 x64) do (
    for %%c in (Debug Release) do (
        echo Rebuilding Chenard %%p %%c
        > logs\build_%%p_%%c.log msbuild chenard.sln /t:rebuild /p:Platform=%%p /p:Configuration=%%c
        if errorlevel 1 (
            echo *** BUILD ERROR ***
            exit /b 1
        )
    )
)

echo Successful rebuild of all Chenard executables.
exit /b 0