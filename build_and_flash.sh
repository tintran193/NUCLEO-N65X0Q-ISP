#!/usr/bin/env bash
#
# NUCLEO_N65X0Q_ISP -- Build and Flash (Linux)
#
# Two modes:
#   dev    (default) Load Appli straight into internal SRAM over SWD and run
#          it. Fully reversible (a power cycle erases it). Requires the
#          board's BOOT1 jumper set to Dev Boot mode (BOOT1=1, BOOT0=0).
#   flash  Build a signed FSBL + a padded Appli image and write both to
#          external NOR flash so the board boots on its own from then on.
#          Also requires BOOT1=1 (Dev Boot mode) while flashing -- SWD needs
#          it to talk to the target at all. AFTER flashing, set BOOT1=0
#          (Flash Boot mode) and power-cycle: FSBL copies Appli from flash
#          into SRAM and jumps to it on every boot, no debugger needed.
#
# Usage: ./build_and_flash.sh [Debug|Release] [dev|flash] [/dev/ttyACM0]
#
# The serial port argument is optional and order-independent (recognized by
# its "/dev/..." shape, e.g. "./build_and_flash.sh /dev/ttyACM0" also works
# with Debug/dev defaulted). When given, a serial monitor (115200 8N1, the
# baud rate LPUART1 is configured for in main.c) is opened on it once the
# build/flash step succeeds, so you see the board's boot log immediately.
#
# --- How "flash" mode differs from a hand-rolled FSBL (e.g. the sibling
# STM32N6_Face_Detection project) -----------------------------------------
# This FSBL uses ST's generic STM32_ExtMem_Manager "LRUN" boot sequence
# (Middlewares/ST/STM32_ExtMem_Manager/boot/stm32_boot_lrun.c) instead of a
# custom bootloader. BOOT_Application() does a raw memcpy of a FIXED number
# of bytes (EXTMEM_LRUN_SOURCE_SIZE, see FSBL/Core/Inc/stm32_extmem_conf.h)
# from flash offset EXTMEM_LRUN_SOURCE_ADDRESS to RAM at
# EXTMEM_LRUN_DESTINATION_ADDRESS, then jumps to
# EXTMEM_LRUN_DESTINATION_ADDRESS + EXTMEM_HEADER_OFFSET. There is no
# signature or embedded size to validate -- so:
#   - Appli's image needs a EXTMEM_HEADER_OFFSET (0x400 = 1KB) padding
#     block in front of it (content doesn't matter, it's never executed;
#     this script just zero-fills it) so Appli's vector table ends up at
#     the address Appli's own linker script assumes (RAM ORIGIN 0x34000400
#     in STM32N657X0HXQ_LRUN.ld = destination 0x34000000 + 0x400).
#   - Appli itself is NOT signed with STM32_SigningTool_CLI (only FSBL is
#     -- it alone is loaded by the immutable BootROM, which does enforce a
#     signed header).
#   - EXTMEM_LRUN_SOURCE_SIZE must be >= the real Appli.bin size or FSBL
#     truncates it on every boot. This script warns if Appli.bin doesn't
#     fit; see the comment in stm32_extmem_conf.h for how to bump it.
#
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# ---------------------------------------------------------------
# Tool paths (override via env if installed elsewhere)
# ---------------------------------------------------------------
PROG_PATH="${STM32_PRG_PATH:-$HOME/STMicroelectronics/STM32Cube/STM32CubeProgrammer/bin}"
PROGRAMMER="$PROG_PATH/STM32_Programmer_CLI"
SIGNER="$PROG_PATH/STM32_SigningTool_CLI"
EXT_LOADER="$PROG_PATH/ExternalLoader/MX25UM51245G_STM32N6570-NUCLEO.stldr"

# ---------------------------------------------------------------
# Flash layout -- MUST match FSBL/Core/Inc/stm32_extmem_conf.h
# (EXTMEM_LRUN_SOURCE_ADDRESS/_SIZE, EXTMEM_HEADER_OFFSET) and both
# linker scripts (FSBL: STM32N657X0HXQ_AXISRAM2_fsbl.ld ROM ORIGIN;
# Appli: STM32N657X0HXQ_LRUN.ld RAM ORIGIN).
# ---------------------------------------------------------------
FSBL_LOAD_ADDR=0x34180400     # FSBL's own SRAM execution address (AXISRAM2)
FSBL_FLASH_ADDR=0x70000000    # where BootROM looks for FSBL in external flash
APPLI_FLASH_ADDR=0x70100000   # 0x70000000 + EXTMEM_LRUN_SOURCE_ADDRESS
APPLI_HEADER_SIZE=1024        # EXTMEM_HEADER_OFFSET -- skipped, never executed
APPLI_MAX_SIZE=262144         # EXTMEM_LRUN_SOURCE_SIZE (0x40000)

# ---------------------------------------------------------------
# Arguments -- order-independent: a "/dev/..." arg is the serial port,
# "Debug"/"Release" is the build type, "dev"/"flash" is the mode. Unset
# ones default to Debug/dev.
# ---------------------------------------------------------------
usage() {
    echo "[ERROR] Usage: $0 [Debug|Release] [dev|flash] [/dev/ttyACM0]" >&2
    exit 1
}

BUILD_TYPE=""
MODE=""
SERIAL_PORT=""
for arg in "$@"; do
    case "$arg" in
        /dev/*) SERIAL_PORT="$arg" ;;
        Debug|Release) BUILD_TYPE="$arg" ;;
        dev|flash) MODE="$arg" ;;
        *) usage ;;
    esac
done
BUILD_TYPE="${BUILD_TYPE:-Debug}"
MODE="${MODE:-dev}"

APPLI_ELF="Appli/build/NUCLEO_N65X0Q_ISP_Appli.elf"
FSBL_ELF="FSBL/build/NUCLEO_N65X0Q_ISP_FSBL.elf"
APPLI_BIN="Appli/build/Appli.bin"
FSBL_BIN="FSBL/build/FSBL.bin"
FSBL_SIGNED="FSBL/build/FSBL_sign.bin"
APPLI_IMAGE="Appli/build/Appli_image.bin"

echo
echo "========================================================"
echo " NUCLEO_N65X0Q_ISP -- Build and Flash"
echo "========================================================"
echo
echo " Board   : NUCLEO-N657X0-Q"
echo " Config  : $BUILD_TYPE"
echo " Mode    : $MODE"
echo " Serial  : ${SERIAL_PORT:-(none -- pass /dev/ttyACM0 etc. to auto-open one)}"
echo

# ---------------------------------------------------------------
# Open a serial monitor on $SERIAL_PORT (115200 8N1, no flow control --
# matches hlpuart1.Init in Appli/Core/Src/main.c). Tries picocom, then
# screen, then falls back to a read-only stty+cat viewer that needs
# neither installed.
# ---------------------------------------------------------------
open_serial_monitor() {
    if [[ -z "$SERIAL_PORT" ]]; then
        return 0
    fi
    if [[ ! -e "$SERIAL_PORT" ]]; then
        echo "[WARN] $SERIAL_PORT does not exist -- skipping serial monitor." >&2
        return 0
    fi
    echo
    echo "[6] Opening serial monitor on $SERIAL_PORT (115200 8N1)..."
    if command -v picocom >/dev/null 2>&1; then
        echo "     (picocom -- exit with Ctrl-A then Ctrl-X)"
        picocom -b 115200 -d 8 -p n -y n "$SERIAL_PORT"
    elif command -v screen >/dev/null 2>&1; then
        echo "     (screen -- exit with Ctrl-A then k, then y)"
        screen "$SERIAL_PORT" 115200
    else
        echo "     (no picocom/screen found -- read-only viewer, Ctrl-C to stop)"
        stty -F "$SERIAL_PORT" 115200 cs8 -cstopb -parenb raw -echo
        cat "$SERIAL_PORT"
    fi
}

if ! command -v arm-none-eabi-gcc >/dev/null 2>&1; then
    echo "[ERROR] arm-none-eabi-gcc not found in PATH." >&2
    exit 1
fi
if [[ ! -x "$PROGRAMMER" ]]; then
    echo "[ERROR] STM32_Programmer_CLI not found at: $PROGRAMMER" >&2
    echo "        Override with STM32_PRG_PATH=<bin dir> $0" >&2
    exit 1
fi
if [[ "$MODE" == "flash" && ! -x "$SIGNER" ]]; then
    echo "[ERROR] STM32_SigningTool_CLI not found at: $SIGNER" >&2
    exit 1
fi
if [[ "$MODE" == "flash" && ! -f "$EXT_LOADER" ]]; then
    echo "[ERROR] External loader not found: $EXT_LOADER" >&2
    exit 1
fi

# ---------------------------------------------------------------
# Step 1 -- Configure and build (Appli + FSBL via top-level preset)
# ---------------------------------------------------------------
echo "[1] Configuring with CMake preset \"$BUILD_TYPE\"..."
cmake --preset "$BUILD_TYPE"

echo "[1] Building..."
cmake --build --preset "$BUILD_TYPE"

if [[ ! -f "$APPLI_ELF" ]]; then
    echo "[ERROR] Appli ELF not found: $APPLI_ELF" >&2
    exit 1
fi
if [[ ! -f "$FSBL_ELF" ]]; then
    echo "[ERROR] FSBL ELF not found: $FSBL_ELF" >&2
    exit 1
fi

if [[ "$MODE" == "dev" ]]; then
    # -------------------------------------------------------------
    # Dev mode -- load Appli into internal SRAM over SWD and run
    # -------------------------------------------------------------
    echo "[2] Connecting and loading Appli into RAM: $APPLI_ELF"
    "$PROGRAMMER" -c port=SWD mode=UR -d "$APPLI_ELF" -run

    echo
    echo "========================================================"
    echo " Done! Appli is running from RAM on NUCLEO-N657X0-Q."
    echo " (Dev Mode: power-cycling the board erases it. Use"
    echo "  '$0 $BUILD_TYPE flash' for a persistent boot.)"
    echo "========================================================"
    open_serial_monitor
    exit 0
fi

# ---------------------------------------------------------------
# Flash mode from here on
# ---------------------------------------------------------------

# Step 2 -- Raw binaries
echo "[2] Extracting raw binaries..."
arm-none-eabi-objcopy -O binary "$FSBL_ELF" "$FSBL_BIN"
arm-none-eabi-objcopy -O binary "$APPLI_ELF" "$APPLI_BIN"

APPLI_SIZE=$(stat -c%s "$APPLI_BIN" 2>/dev/null || stat -f%z "$APPLI_BIN")
echo "    Appli.bin size: $APPLI_SIZE bytes"
if (( APPLI_SIZE + APPLI_HEADER_SIZE > APPLI_MAX_SIZE )); then
    echo "[ERROR] Appli.bin ($APPLI_SIZE bytes) + header ($APPLI_HEADER_SIZE) exceeds" >&2
    echo "        EXTMEM_LRUN_SOURCE_SIZE ($APPLI_MAX_SIZE bytes). FSBL would truncate" >&2
    echo "        it on every boot. Bump EXTMEM_LRUN_SOURCE_SIZE in" >&2
    echo "        FSBL/Core/Inc/stm32_extmem_conf.h, rebuild FSBL, and retry." >&2
    exit 1
fi

# Step 3 -- Sign FSBL (BootROM requires a signed header to load it at all)
FSBL_ENTRY=$(arm-none-eabi-readelf -h "$FSBL_ELF" | awk '/Entry point address/ {print $NF}')
if [[ -z "$FSBL_ENTRY" ]]; then
    echo "[ERROR] Could not read FSBL entry point from $FSBL_ELF" >&2
    exit 1
fi
echo "[3] Signing FSBL (load=$FSBL_LOAD_ADDR entry=$FSBL_ENTRY)..."
# Remove any previous output first -- the signing tool asks "replace this
# file? (y/n)" otherwise, and with no TTY attached that prompt just loops
# forever instead of failing.
rm -f "$FSBL_SIGNED"
"$SIGNER" -bin "$FSBL_BIN" -nk -t fsbl -la "$FSBL_LOAD_ADDR" -ep "$FSBL_ENTRY" \
    -hv 2.3 -align -o "$FSBL_SIGNED"

# Step 4 -- Build the Appli flash image: [1KB zero padding][Appli.bin].
# FSBL's BOOT_Application() (stm32_boot_lrun.c) does a raw, fixed-size
# memcpy from flash to RAM with no signature or size check -- it just
# skips EXTMEM_HEADER_OFFSET (1KB) bytes before jumping. That padding is
# never executed, so its content doesn't matter; Appli is NOT run through
# STM32_SigningTool_CLI.
echo "[4] Building Appli flash image: $APPLI_IMAGE"
head -c "$APPLI_HEADER_SIZE" /dev/zero > "$APPLI_IMAGE"
cat "$APPLI_BIN" >> "$APPLI_IMAGE"

# Step 5 -- Write both to external NOR flash (board must be in Dev Boot mode,
# BOOT1=1, for SWD to reach it at all)
echo "[5] Flashing FSBL to $FSBL_FLASH_ADDR: $FSBL_SIGNED"
"$PROGRAMMER" -c port=SWD mode=UR -el "$EXT_LOADER" -w "$FSBL_SIGNED" "$FSBL_FLASH_ADDR"

echo "[5] Flashing Appli image to $APPLI_FLASH_ADDR: $APPLI_IMAGE"
"$PROGRAMMER" -c port=SWD mode=UR -el "$EXT_LOADER" -w "$APPLI_IMAGE" "$APPLI_FLASH_ADDR"

echo
echo "========================================================"
echo " Done! FSBL + Appli written to external flash."
echo
echo " To boot on its own: set BOOT1=0 (Flash Boot mode, BOOT0"
echo " stays 0) and power-cycle the board. No debugger needed."
echo "========================================================"
if [[ -n "$SERIAL_PORT" ]]; then
    echo
    echo " Opening the serial monitor now -- flip BOOT1 to 0 and"
    echo " power-cycle the board to see it boot."
fi
open_serial_monitor
