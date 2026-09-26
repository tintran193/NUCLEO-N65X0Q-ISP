@echo off
setlocal EnableDelayedExpansion
title NUCLEO_N65X0Q_ISP -- Build and Flash

:: ---------------------------------------------------------------
:: NUCLEO_N65X0Q_ISP -- Build and Flash (Windows)
::
:: Two modes:
::   dev    (default) Load Appli straight into internal SRAM over SWD and
::          run it. Fully reversible (a power cycle erases it). Requires
::          the board's BOOT1 jumper set to Dev Boot mode (BOOT1=1, BOOT0=0).
::   flash  Build a signed FSBL + a padded Appli image and write both to
::          external NOR flash so the board boots on its own from then on.
::          Also requires BOOT1=1 while flashing -- SWD needs it to talk to
::          the target at all. AFTER flashing, set BOOT1=0 (Flash Boot mode)
::          and power-cycle: FSBL copies Appli from flash into SRAM and
::          jumps to it on every boot, no debugger needed.
::
:: Usage: build_and_flash.bat [Debug|Release] [dev|flash] [COM3]
::
:: The COM port argument is optional and order-independent (recognized by
:: its "COM<n>" shape, e.g. "build_and_flash.bat COM3" also works with
:: Debug/dev defaulted). When given, a serial monitor (115200 8N1, the baud
:: rate LPUART1 is configured for in main.c) opens on it once the
:: build/flash step succeeds, so you see the board's boot log immediately.
::
:: This FSBL uses ST's generic STM32_ExtMem_Manager "LRUN" boot sequence
:: (see FSBL/Core/Inc/stm32_extmem_conf.h): BOOT_Application() memcpy's a
:: FIXED number of bytes (EXTMEM_LRUN_SOURCE_SIZE) from flash to RAM, no
:: signature or embedded size involved. So the Appli image just needs a
:: 1KB zero-padding header in front of it (skipped, never executed -- NOT
:: the same 4-byte size-prefixed container the sibling Face_Detection
:: project uses), and Appli itself is not run through the ST signing tool
:: (only FSBL is, since only FSBL is loaded directly by the BootROM).
:: ---------------------------------------------------------------

:: ---------------------------------------------------------------
:: Tool paths (edit if installed in a non-default location)
:: ---------------------------------------------------------------
set PROG_PATH=C:\Program Files\STMicroelectronics\STM32Cube\STM32CubeProgrammer\bin
set PROGRAMMER="%PROG_PATH%\STM32_Programmer_CLI.exe"
set SIGNER="%PROG_PATH%\STM32_SigningTool_CLI.exe"
set EXT_LOADER=%PROG_PATH%\ExternalLoader\MX25UM51245G_STM32N6570-NUCLEO.stldr

:: ---------------------------------------------------------------
:: Flash layout -- MUST match FSBL\Core\Inc\stm32_extmem_conf.h
:: (EXTMEM_LRUN_SOURCE_ADDRESS/_SIZE, EXTMEM_HEADER_OFFSET) and both
:: linker scripts (FSBL: STM32N657X0HXQ_AXISRAM2_fsbl.ld ROM ORIGIN;
:: Appli: STM32N657X0HXQ_LRUN.ld RAM ORIGIN).
:: ---------------------------------------------------------------
set FSBL_LOAD_ADDR=0x34180400
set FSBL_FLASH_ADDR=0x70000000
set APPLI_FLASH_ADDR=0x70100000
set APPLI_HEADER_SIZE=1024
set APPLI_MAX_SIZE=262144

:: ---------------------------------------------------------------
:: Arguments -- order-independent: a "COM<n>" arg is the serial port,
:: "Debug"/"Release" is the build type, "dev"/"flash" is the mode. Unset
:: ones default to Debug/dev.
:: ---------------------------------------------------------------
set BUILD_TYPE=
set MODE=
set SERIAL_PORT=
for %%A in (%*) do (call :ClassifyArg "%%~A" || goto END)
if "%BUILD_TYPE%"=="" set BUILD_TYPE=Debug
if "%MODE%"=="" set MODE=dev
goto ArgsParsed

:ClassifyArg
set "ARG=%~1"
echo %ARG%| findstr /R /I "^COM[0-9][0-9]*$" >nul
if not errorlevel 1 (
    set "SERIAL_PORT=%ARG%"
    exit /b 0
)
if /I "%ARG%"=="Debug"   (set "BUILD_TYPE=Debug"   & exit /b 0)
if /I "%ARG%"=="Release" (set "BUILD_TYPE=Release" & exit /b 0)
if /I "%ARG%"=="dev"     (set "MODE=dev"           & exit /b 0)
if /I "%ARG%"=="flash"   (set "MODE=flash"         & exit /b 0)
echo [ERROR] Unrecognized argument: %ARG%
echo         Usage: %~n0 [Debug^|Release] [dev^|flash] [COM3]
exit /b 1

:ArgsParsed

set APPLI_ELF=Appli\build\NUCLEO_N65X0Q_ISP_Appli.elf
set FSBL_ELF=FSBL\build\NUCLEO_N65X0Q_ISP_FSBL.elf
set APPLI_BIN=Appli\build\Appli.bin
set FSBL_BIN=FSBL\build\FSBL.bin
set FSBL_SIGNED=FSBL\build\FSBL_sign.bin
set APPLI_IMAGE=Appli\build\Appli_image.bin
set APPLI_HEADER=Appli\build\Appli_header.bin

echo.
echo ========================================================
echo  NUCLEO_N65X0Q_ISP -- Build and Flash
echo ========================================================
echo.
echo  Board   : NUCLEO-N657X0-Q
echo  Config  : %BUILD_TYPE%
echo  Mode    : %MODE%
if "%SERIAL_PORT%"=="" (
    echo  Serial  : ^(none -- pass COM3 etc. to auto-open one^)
) else (
    echo  Serial  : %SERIAL_PORT%
)
echo.

where arm-none-eabi-gcc >nul 2>nul
if errorlevel 1 (
    echo [ERROR] arm-none-eabi-gcc not found in PATH.
    goto END
)
if not exist %PROGRAMMER% (
    echo [ERROR] STM32_Programmer_CLI not found at: %PROGRAMMER%
    goto END
)
if "%MODE%"=="flash" if not exist %SIGNER% (
    echo [ERROR] STM32_SigningTool_CLI not found at: %SIGNER%
    goto END
)
if "%MODE%"=="flash" if not exist "%EXT_LOADER%" (
    echo [ERROR] External loader not found: %EXT_LOADER%
    goto END
)

:: ---------------------------------------------------------------
:: Step 1 -- Configure and build (Appli + FSBL via top-level preset)
:: ---------------------------------------------------------------
echo [1] Configuring with CMake preset "%BUILD_TYPE%"...
cmake --preset %BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] CMake configure failed.
    goto END
)

echo [1] Building...
cmake --build --preset %BUILD_TYPE%
if errorlevel 1 (
    echo [ERROR] Build failed.
    goto END
)

if not exist "%APPLI_ELF%" (
    echo [ERROR] Appli ELF not found: %APPLI_ELF%
    goto END
)
if not exist "%FSBL_ELF%" (
    echo [ERROR] FSBL ELF not found: %FSBL_ELF%
    goto END
)

if "%MODE%"=="dev" (
    :: -------------------------------------------------------------
    :: Dev mode -- load Appli into internal SRAM over SWD and run
    :: -------------------------------------------------------------
    echo [2] Connecting and loading Appli into RAM: %APPLI_ELF%
    %PROGRAMMER% -c port=SWD mode=UR -d "%APPLI_ELF%" -run
    if errorlevel 1 (
        echo [ERROR] Flashing failed.
        goto END
    )

    echo.
    echo ========================================================
    echo  Done! Appli is running from RAM on NUCLEO-N657X0-Q.
    echo  ^(Dev Mode: power-cycling the board erases it. Use
    echo   "%~n0 %BUILD_TYPE% flash" for a persistent boot.^)
    echo ========================================================
    call :OpenSerialMonitor
    goto END
)

:: ---------------------------------------------------------------
:: Flash mode from here on
:: ---------------------------------------------------------------

:: Step 2 -- Raw binaries
echo [2] Extracting raw binaries...
arm-none-eabi-objcopy -O binary "%FSBL_ELF%" "%FSBL_BIN%"
arm-none-eabi-objcopy -O binary "%APPLI_ELF%" "%APPLI_BIN%"

for %%A in ("%APPLI_BIN%") do set APPLI_SIZE=%%~zA
echo     Appli.bin size: %APPLI_SIZE% bytes
set /a APPLI_TOTAL=%APPLI_SIZE%+%APPLI_HEADER_SIZE%
if %APPLI_TOTAL% GTR %APPLI_MAX_SIZE% (
    echo [ERROR] Appli.bin ^(%APPLI_SIZE% bytes^) + header ^(%APPLI_HEADER_SIZE%^) exceeds
    echo         EXTMEM_LRUN_SOURCE_SIZE ^(%APPLI_MAX_SIZE% bytes^). FSBL would truncate
    echo         it on every boot. Bump EXTMEM_LRUN_SOURCE_SIZE in
    echo         FSBL\Core\Inc\stm32_extmem_conf.h, rebuild FSBL, and retry.
    goto END
)

:: Step 3 -- Sign FSBL (BootROM requires a signed header to load it at all)
for /f "tokens=4" %%A in ('arm-none-eabi-readelf -h "%FSBL_ELF%" ^| findstr /C:"Entry point address"') do set FSBL_ENTRY=%%A
if "%FSBL_ENTRY%"=="" (
    echo [ERROR] Could not read FSBL entry point from %FSBL_ELF%
    goto END
)
echo [3] Signing FSBL ^(load=%FSBL_LOAD_ADDR% entry=%FSBL_ENTRY%^)...
:: Remove any previous output first -- the signing tool asks "replace this
:: file? (y/n)" otherwise, and with no console attached that prompt can
:: hang instead of failing.
if exist "%FSBL_SIGNED%" del /f /q "%FSBL_SIGNED%"
%SIGNER% -bin "%FSBL_BIN%" -nk -t fsbl -la %FSBL_LOAD_ADDR% -ep %FSBL_ENTRY% -hv 2.3 -align -o "%FSBL_SIGNED%"
if errorlevel 1 (
    echo [ERROR] FSBL signing failed.
    goto END
)

:: Step 4 -- Build the Appli flash image: [1KB zero padding][Appli.bin].
:: FSBL's BOOT_Application() does a raw, fixed-size memcpy from flash to
:: RAM with no signature or size check -- it just skips EXTMEM_HEADER_OFFSET
:: (1KB) bytes before jumping. Appli is NOT run through the ST signing tool.
echo [4] Building Appli flash image: %APPLI_IMAGE%
powershell -NoProfile -Command ^
    "[IO.File]::WriteAllBytes('%APPLI_HEADER%', (New-Object byte[] %APPLI_HEADER_SIZE%))"
if errorlevel 1 (
    echo [ERROR] Failed to build header padding.
    goto END
)
copy /b "%APPLI_HEADER%" + "%APPLI_BIN%" "%APPLI_IMAGE%" >nul
if errorlevel 1 (
    echo [ERROR] Failed to build Appli flash image.
    goto END
)

:: Step 5 -- Write both to external NOR flash (board must be in Dev Boot
:: mode, BOOT1=1, for SWD to reach it at all)
echo [5] Flashing FSBL to %FSBL_FLASH_ADDR%: %FSBL_SIGNED%
%PROGRAMMER% -c port=SWD mode=UR -el "%EXT_LOADER%" -w "%FSBL_SIGNED%" %FSBL_FLASH_ADDR%
if errorlevel 1 (
    echo [ERROR] FSBL flash failed.
    goto END
)

echo [5] Flashing Appli image to %APPLI_FLASH_ADDR%: %APPLI_IMAGE%
%PROGRAMMER% -c port=SWD mode=UR -el "%EXT_LOADER%" -w "%APPLI_IMAGE%" %APPLI_FLASH_ADDR%
if errorlevel 1 (
    echo [ERROR] Appli flash failed.
    goto END
)

echo.
echo ========================================================
echo  Done! FSBL + Appli written to external flash.
echo.
echo  To boot on its own: set BOOT1=0 ^(Flash Boot mode, BOOT0
echo  stays 0^) and power-cycle the board. No debugger needed.
echo ========================================================
echo.
if not "%SERIAL_PORT%"=="" (
    echo  Opening the serial monitor now -- flip BOOT1 to 0 and
    echo  power-cycle the board to see it boot.
    echo.
)
call :OpenSerialMonitor

:END
endlocal
pause
goto :eof

:: ---------------------------------------------------------------
:: Open a serial monitor on %SERIAL_PORT% (115200 8N1, no flow control --
:: matches hlpuart1.Init in Appli\Core\Src\main.c). Uses PowerShell's
:: SerialPort class so no extra tool (PuTTY/TeraTerm/...) is required.
:: ---------------------------------------------------------------
:OpenSerialMonitor
if "%SERIAL_PORT%"=="" exit /b 0
echo [6] Opening serial monitor on %SERIAL_PORT% (115200 8N1, Ctrl+C to stop)...
powershell -NoProfile -Command ^
    "$p = New-Object System.IO.Ports.SerialPort('%SERIAL_PORT%', 115200, 'None', 8, 'One');" ^
    "$p.ReadTimeout = 500;" ^
    "try { $p.Open() } catch { Write-Host \"[ERROR] Could not open %SERIAL_PORT%: $_\"; exit 1 };" ^
    "while ($true) { try { Write-Host $p.ReadLine() } catch [System.TimeoutException] {} }"
exit /b 0
