@echo off
cls

color 01
echo /*
echo  * Copyright (c) 2006 - 2025, RT - Thread Development Team
echo  *
echo  * SPDX - License - Identifier: Apache - 2.0
echo  *
echo  * Change Logs:
echo  * Date           Author       Notes
echo  * 2025/04/29     Wangshun     first version
echo  */
echo.
color 07

setlocal enabledelayedexpansion

set CONFIG_FILE=qemu_config.txt
set CPU_CONFIG_FILE=cpu_config.txt

if exist %CONFIG_FILE% (
    for /f "usebackq delims=" %%a in (%CONFIG_FILE%) do set "QEMU_DIR=%%a"
    echo Current QEMU directory: !QEMU_DIR!
) else (
    echo This is the first time running. Please set the QEMU directory.
    set /p "USER_INPUT=Enter the QEMU directory: "
    if defined USER_INPUT (
        if not "!USER_INPUT!"=="" (
            set "QEMU_DIR=!USER_INPUT!"
            echo !QEMU_DIR! > %CONFIG_FILE%
        ) else (
            echo No valid path provided. Using default.
            set "QEMU_DIR=E:\XuanTie Core\6.QEMU"
        )
    ) else (
        echo No input received. Using default.
        set "QEMU_DIR=E:\XuanTie Core\6.QEMU"
    )
)

if exist %CPU_CONFIG_FILE% (
    for /f "usebackq delims=" %%a in (%CPU_CONFIG_FILE%) do set "CPU_PARAM=%%a"
) else (
    set "CPU_PARAM=c908v"
)

set "QEMU_PATH=%QEMU_DIR%\qemu-system-riscv64.exe"
set "ELF_PATH=%CD%\rtthread.elf"

set /p "USER_INPUT=Enter new QEMU directory (press Enter to use default): "
if defined USER_INPUT (
    if not "!USER_INPUT!"=="" (
        set "QEMU_DIR=!USER_INPUT!"
        echo !QEMU_DIR! > %CONFIG_FILE%
    )
)
set "QEMU_PATH=%QEMU_DIR%\qemu-system-riscv64.exe"

echo.
echo Current CPU parameter: !CPU_PARAM!
set /p "CPU_INPUT=Enter new -cpu parameter (press Enter to use default): "
if defined CPU_INPUT (
    if not "!CPU_INPUT!"=="" (
        set "CPU_PARAM=!CPU_INPUT!"
        echo !CPU_PARAM! > %CPU_CONFIG_FILE%
    )
)

echo.
 "%QEMU_PATH%" --version

echo.
"%QEMU_PATH%" -machine xiaohui -kernel "%ELF_PATH%" -nographic -cpu %CPU_PARAM%

if %ERRORLEVEL% NEQ 0 (
    echo Error: QEMU failed to run. Please check the configuration or paths.
    pause
    exit /b %ERRORLEVEL%
)

echo QEMU terminated.
pause
endlocal