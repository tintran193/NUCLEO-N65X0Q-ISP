# Worklog — NUCLEO-N65X0Q-ISP

Chronological record of this project's CMake conversion + bring-up debugging, so a future
session can pick up context without re-deriving it. Newest entry on top.

---

## 2026-09-26 — CMake conversion, build/flash pipeline, camera bring-up debugging

### Done

1. **Camera_README.md typo/bug pass** — fixed doc/code-mismatch issues (struct field names
   `handler`→`handle`, `data`→`value`, a few Vietnamese typos, a Mermaid label typo). Not the
   main work of this session, done first as a smaller request.

2. **Converted the project from STM32CubeIDE (Eclipse/Makefile) to CMake**, mirroring the
   sibling `../STM32N6_Face_Detection` project's structure:
   - Top-level `CMakeLists.txt` / `mx-generated.cmake` (ExternalProject_Add for Appli + FSBL),
     `gcc-arm-none-eabi.cmake` (copied verbatim, generic toolchain file), `CMakePresets.json`.
   - `Appli/CMakeLists.txt` + `Appli/mx-generated.cmake`, `FSBL/CMakeLists.txt` +
     `FSBL/mx-generated.cmake` — source/include/define lists reconstructed from the original
     `Appli/Debug/**/subdir.mk` / `FSBL/Debug/**/subdir.mk` (the real GCC command lines
     CubeIDE had generated), not guessed.
   - **Important discovery**: every `subdir.mk` in the Appli project references a
     `stm32-mw-usb-device` middleware and an `ISP_SRC/stm32-mw-isp` folder (~60 source files
     total) that **do not exist anywhere in this checkout** — leftover CubeIDE "linked
     resource" folders pointing at the original author's machine
     (`D:/STM32N6_WS/...`, `D:/stm32-mw-isp/...`). These were left out of the CMake build.
     Nothing is actually missing functionally: `main.c` never calls `MX_USB_DEVICE_Init()`
     or anything from `usbd_core.c`.
   - `ISP_MW/isp/Src/isp_tool_com.c` *does* exist locally, but `#include`s
     `usbd_cdc_if.h`/`usb_device.h` — same missing-middleware problem. Its calls are all
     guarded by `#ifdef ISP_MW_TUNING_TOOL_SUPPORT` in `isp_core.c`, never defined here, so
     it's dead code. Excluded from the CMake build rather than stubbing two missing headers.
   - Verified: both `Debug` and `Release` presets build clean for Appli + FSBL
     (`cmake --preset X && cmake --build --preset X`).
   - Added `.gitignore` (build/ dirs, CMake cache files — copied pattern from
     `STM32N6_Face_Detection/.gitignore`).

3. **Wrote `build_and_flash.sh` / `build_and_flash.bat`** (`dev` = SWD RAM-load, `flash` =
   write to external NOR via FSBL). **This FSBL uses a different boot mechanism than
   `STM32N6_Face_Detection`'s hand-rolled one** — ST's generic
   `Middlewares/ST/STM32_ExtMem_Manager/boot/stm32_boot_lrun.c` "LRUN" sequence:
   - `BOOT_Application()` does a **fixed-size, unsigned** `memcpy` from flash offset
     `EXTMEM_LRUN_SOURCE_ADDRESS` (`0x00100000`, i.e. absolute flash addr `0x70100000`) to RAM
     at `EXTMEM_LRUN_DESTINATION_ADDRESS` (`0x34000000`), then jumps to
     `destination + EXTMEM_HEADER_OFFSET` (`0x400`). No signature, no embedded size — the
     first 1KB of the image is just skipped padding.
   - So Appli's flash image is `[1KB zero padding][Appli.bin]`, **not** signed with
     `STM32_SigningTool_CLI` (only FSBL is — it alone is BootROM-loaded and needs the signed
     header). This is a different container format than Face_Detection's 4-byte
     size-prefixed one — don't copy that script blindly for this project.
   - **Bug found & fixed**: `EXTMEM_LRUN_SOURCE_SIZE` (`FSBL/Core/Inc/stm32_extmem_conf.h`)
     was `0x10000` (64KB) — but Appli's Debug build is already ~114KB (RAM "Used Size" from
     the linker, which includes .bss; the `.bin` extracted via `objcopy` is ~70KB). FSBL would
     have silently truncated Appli on every real flash boot. Bumped to `0x40000` (256KB).
   - Scripts accept an optional serial port arg (`/dev/ttyACM0` or `COM3`), order-independent
     with the existing `[Debug|Release] [dev|flash]` args — opens a serial monitor (115200
     8N1, matches `hlpuart1.Init`) once the build/flash step succeeds. `.sh` tries
     picocom→screen→stty+cat fallback; `.bat` uses PowerShell's `SerialPort` class.
   - Fixed a real bug in the flash-mode script itself: `STM32_SigningTool_CLI` prompts
     "replace this file? (y/n)" if the output already exists, which with no TTY attached
     spins forever instead of failing. Scripts now `rm -f`/`del` the old signed output first.

4. **Camera/DCMIPP bugs actually described in Camera_README.md's "Lỗi" sections** — analyzed
   with no hardware available yet at the time:
   - **PIPE1 (RAW10→RGB via ISP) overrun**: `MX_DCMIPP_Init()` configured PIPE1's packer,
     downsize, VC etc. but never called `HAL_DCMIPP_SetIPPlugConfig()` for its AXI
     write-client (`DCMIPP_CLIENT2`) — left at hardware-reset defaults (effectively no
     internal FIFO depth, no outstanding-transaction headroom). Cross-checked against ST's
     own reference (`x-cube-n6-ai-face-landmarks/Src/app_cam.c`) and a pre-existing local
     `DCMIPP_CSI_FIXES_GUIDE.md`, both of which explicitly configure IPPlug for the active
     capture pipe's client. **Fix applied** in `MX_DCMIPP_Init()` (search `USER CODE BEGIN
     DCMIPP_Init 2`): full FIFO pool (`DPREGStart=0x000`, `DPREGEnd=0x3FF`) since PIPE0 is
     never started in this app and can't contend for it, burst 128B, 16 outstanding
     transactions, max WLRU priority.
   - The PIPE0 RAW8/BT_900 "DCMIPP ERROR" and PIPE0 RAW10/BT_450 overrun cases documented in
     the README are just illustrations of CSI-PHY-bitrate-vs-actual-sensor-clock mismatches;
     current code already uses the matched value (`PHY_BT_220`, `imx219.c` `0x030D=0x1C`) —
     nothing to fix there.

5. **Added debug instrumentation** (per user request, to make future log reads self-evident
   without re-deriving bit positions each time):
   - `MX_DCMIPP_Init()`: prints back `IPC2R1/IPC2R2/IPC2R3` right after
     `HAL_DCMIPP_SetIPPlugConfig()`, confirming the IPPlug fix actually landed in hardware.
   - After frame capture: decodes `P1SR` (`OVRF`/`LSTFRM`/`LSTLINE` bits) and `CMSR2`
     (`P1OVRF`) explicitly instead of raw hex, plus a one-line `PIPE1 OVERRUN: yes/no` verdict.
   - `Camera_CheckFrameBuffer()`: `Non-zero bytes` now also shows fraction/percentage, plus a
     `FRAME COMPLETE: YES / NO (truncated)` verdict line.
   - Added `I2C_ScanBus()` — full 0x00–0x7F `i2cdetect`-style grid scan of I2C2, run right
     after the camera power/reset GPIO sequence and before the IMX219 ID read. Prints every
     address (not just hits) plus an explicit `PRESENT`/`NOT FOUND` verdict for the IMX219's
     expected address (`0x20`).

6. **Flashed to real hardware and debugged a full camera-comms failure** (ST-Link/board
   physically connected by the user; USB connection itself was flaky a few times — unrelated
   ST-Link/cable issue, not a code problem). Three real, distinct bugs found and fixed along
   the way, root-caused by comparing byte-for-byte against `../Camera_N6_AI_Test`, a sibling
   project confirmed working on this exact same board/camera:

   a. **IPPlug fix (see #4)** — applied first, before any hardware was available to test.

   b. **RIF GPIO security blocking I2C2's pins.** `SystemIsolation_Config()` (in `main.c`)
      marked `GPIOB_PIN_10`/`GPIO_PIN_11` as `GPIO_PIN_SEC` — but those pins double as
      `I2C2_SCL`/`I2C2_SDA` on this package. This whole block (11 GPIO pins,
      `RIF_MASTER_INDEX_ETH1`) was leftover CubeMX codegen for Ethernet, which this project
      has zero driver code for. Result: I2C2 ACK'd nothing at any address at all (confirmed
      via the new `I2C_ScanBus()` — a full empty grid), even though `HAL_I2C_Init()` reported
      success (RIF silently drops disallowed writes rather than faulting, so the peripheral
      *looked* configured in software while never actually working). **Fix**: removed the
      entire `ETH1`/GPIO-security block; added `RIF_RISC_PERIPH_INDEX_CSI` (present in the
      working reference's `Security_Config()`, missing from ours) alongside the existing
      `RIF_RISC_PERIPH_INDEX_DCMIPP`. Mirrors the reference project's `Security_Config()`
      structure exactly now (RIMC for DCMIPP only, RISC slave attrs for CSI + DCMIPP).

   c. **The actual root cause — `HAL_GPIO_Init()` was never called for the camera's
      reset/enable pins.** `MX_GPIO_Init()` only enables GPIO port clocks (E/B/A — not even
      O); it never configures any pin's mode. Every `HAL_GPIO_WritePin()` targeting
      `CAM_NRST` (GPIOO_5) and `EN_MODULE` (GPIOA_0) had been writing to pins that were never
      initialized as outputs (GPIOO's clock wasn't even enabled). This had gone unnoticed
      because a fresh SWD RAM-load leaves the chip at clean reset defaults, under which this
      board's external pull-up apparently happened to keep `CAM_NRST` high anyway — but it
      broke the moment Appli started booting via FSBL (new in this session): FSBL
      reconfigures many GPIOs for its external XSPI flash interface before jumping to Appli,
      and nothing in Appli's code ever reclaimed these two pins afterward. **Fix**: added
      explicit `__HAL_RCC_GPIOO_CLK_ENABLE()` + `HAL_GPIO_Init()` (mode `OUTPUT_PP`) for both
      `GPIOO_PIN_5` and `GPIOA_PIN_0` right before they're first written, matching
      `Camera_N6_AI_Test`'s pattern. **This was the fix that got the camera working** — after
      it, `I2C_ScanBus()` found the IMX219 at `0x20`, ID read `0x0219`, full 640x480 RAW10
      config succeeded, DCMIPP capture + IMX219 streaming both started successfully.

   Also reordered the GPIO sequence (reset before EN_MODULE, not after) to match the
   reference project, though in hindsight (c) was almost certainly the real fix and the
   ordering change alone would not have been sufficient.

7. **Found the AWB/AE warm-up loop (`while(frame_count < 60)`, right after
   `IMX219: streaming started`) could hang forever with zero output.** `frame_count` only
   increments from `HAL_DCMIPP_PIPE_FrameEventCallback`, which needs a real DCMIPP
   frame-complete interrupt on PIPE1 — if that interrupt never fires even once (CSI never
   locks / no valid frame ever completes), the loop just spins silently, no timeout, no print.
   Confirmed via hardware: after fix 6c got the camera responding on I2C, a real test run hung
   here for 5+ minutes with literally nothing printed after `IMX219: streaming started`.
   **Fix**: loop now prints `frame_count`/`CSI_IRQ`/`DCMIPP_IRQ`/`SOT_L0`/`SOT_L1` counters
   every ~1s, and bails out after 10s with a full diagnostic dump (those counters plus
   `CSI->SR0/SR1`, `DCMIPP->P1SR/CMSR2`) instead of hanging indefinitely. This doesn't fix
   the underlying "no frame arrives" problem (unknown cause yet) — it just makes it
   observable instead of a silent hang.

8. **Root-caused the frame-hang from step 7 with real hardware data.** Fresh log showed the
   warmup loop timing out at 10s with `frame_count=0`, `CSI_IRQ=0`, `DCMIPP_IRQ=0`,
   `SOT_L0=0`, `SOT_L1=0` the entire time, and `CSI->SR1 = 0x64100000`. Decoded against
   `stm32n657xx.h` bit definitions: bits 20/26/29/30 = `ULPNDL0F`/`ULPNDL1F`/`ULPNACTF`/
   `ULPNCLF` — i.e. the CSI D-PHY receiver (both data lanes *and* the clock lane) is sitting
   in Ultra-Low-Power/idle state the whole time, never even attempting HS synchronization.
   No error flags either (no `ESOTDL0F` etc.) — this isn't a failed reception, it's a
   receiver that's never even trying, consistent with it having no functioning reference
   clock at all.
   - Checked `MX_DCMIPP_Init()` and the whole file: **RCC for DCMIPP/CSI peripheral clocks
     is never configured anywhere.** `MX_DCMIPP_Init()` only touches DCMIPP/CSI *peripheral*
     config registers (pipe config, PHY bitrate class, etc.), never `RCC_PERIPHCLK_DCMIPP` /
     `RCC_PERIPHCLK_CSI`.
   - `Camera_N6_AI_Test` has a dedicated `MX_DCMIPP_ClockConfig()` called *before*
     `MX_DCMIPP_Init()` that routes `RCC_PERIPHCLK_DCMIPP` through IC17 (PLL1/4) and
     `RCC_PERIPHCLK_CSI` through IC18 (PLL1/60) — noticed this difference early in the
     session (see the very first comparison against this reference project) but dismissed it
     at the time since the live symptom then was I2C-related, not CSI-related. Revisited once
     I2C was fixed and the blocker moved to "no CSI activity at all".
   - **Fix**: added `MX_DCMIPP_ClockConfig()` (copied from the reference, both PLL1 trees
     confirmed identical via FSBL diff — see 6b's reasoning pattern) called right before
     `MX_DCMIPP_Init()` in `main()`.

9. **This alone changed nothing when tested on hardware** — identical `CSI_SR0/SR1`, identical
   hang, byte-for-byte. But the log still showed `IPPlug CLIENT2:` (printed inside
   `MX_DCMIPP_Init()`, which runs *after* `MX_DCMIPP_ClockConfig()`), proving the new RCC call
   returned `HAL_OK` rather than silently failing. So something *after* it was undoing it.
   Found it: `HAL_DCMIPP_Init()` (called from `MX_DCMIPP_Init()`) invokes
   `HAL_DCMIPP_MspInit()` (`stm32n6xx_hal_msp.c`), which still had CubeMX's **default**
   `RCC_PeriphCLKInitTypeDef` block — `RCC_PERIPHCLK_DCMIPP|RCC_PERIPHCLK_CSI`, DCMIPP from
   `PCLK5`, CSI's IC18 from **PLL4**/1 (not PLL1) — running *after* `MX_DCMIPP_ClockConfig()`
   and silently overwriting it, since it's the same `RCC_PERIPHCLK_DCMIPP`/`RCC_PERIPHCLK_CSI`
   selection touched twice with the last write winning. Checked the reference project's own
   `HAL_DCMIPP_MspInit()`: **it has this exact block deliberately removed**, with a comment
   explaining clock source selection is done once, in `MX_DCMIPP_ClockConfig()`, and MSP init
   should only enable/reset/NVIC-config. **Fix**: removed the same block from our
   `HAL_DCMIPP_MspInit()`, added a comment (with regen warning, since this is CubeMX-generated
   code outside `USER CODE` markers and will come back on a future "Generate Code" unless
   re-deleted). **Also not yet confirmed on hardware as of this worklog entry** — build is
   clean, not yet flashed/tested.

10. **Flashed step 9's fix and got a full, correct capture.** `[warmup] frame_count=30
    CSI_IRQ=32 DCMIPP_IRQ=60 SOT_L0=0 SOT_L1=0` → counters now moving, `BGP OK` printed,
    `DCMIPP snapshot OK`, `FRAME RECEIVED`. **This is the fix that actually got the camera
    producing frames.** (`SOT_L0`/`SOT_L1` staying at 0 while `CSI_IRQ`/`DCMIPP_IRQ` move is
    fine -- those two counters are only incremented by a specific bit pattern in `CSI->SR1`
    in `CSI_IRQHandler`, unrelated to overall capture success; not investigated further since
    everything downstream worked.)
    - **Original PIPE1-overrun bug (4/6a) — CONFIRMED FIXED**: `P1SR = 0x00020007` (`OVRF=0`),
      `CMSR2` (`P1OVRF=0`), `PIPE1 OVERRUN: no`, `Non-zero bytes = 614400 / 614400 (100%)`,
      `FRAME COMPLETE: YES`. Full frame, no truncation, no overrun. This is the bug the whole
      investigation (starting from Camera_README.md's "Lỗi" section) set out to fix.
      Camera_README.md's PIPE1 section updated to reflect this (was marked "chưa kiểm chứng").
    - Two small things noticed in this same log, both fixed:
      - `printf("ok\n")` in the idle loop (`main()`, very end) used `\n` alone where every
        other printf in the file uses `\r\n` — caused a visible "staircase" indentation
        artifact on the terminal (no carriage return between iterations). Changed to `\r\n`.
      - `DCMIPP data counter = 0` printed despite a fully successful capture — turned out to
        be an **ST HAL driver limitation**, not a bug in this project:
        `HAL_DCMIPP_PIPE_GetDataCounter()` (`stm32n6xx_hal_dcmipp.c`) ignores its own `Pipe`
        argument and always reads `P0DCCNTR` (Pipe0's counter). Pipe0 is never started in
        this app, so that register is always 0 regardless of what Pipe1 actually did. Added a
        comment at the call site in `main.c` explaining this so it isn't mistaken for a real
        problem again; `Camera_CheckFrameBuffer()`'s non-zero-byte count/checksum remains the
        trustworthy signal for whether a capture actually succeeded.

### Current status (end of session)

**Camera fully working end-to-end, confirmed on real hardware**: boots via FSBL → external
flash → Appli, IMX219 initializes and streams, PIPE1 (RAW10 → ISP → RGB-ish output, per the
current pixel packer config) captures a complete, correct 614400-byte frame with no overrun.
The original bug from Camera_README.md's PIPE1 section is fixed and confirmed. All work items
from this session are done; nothing is mid-flight or unverified as of this entry.

Six real, distinct bugs were found and fixed to get here (see the numbered log above for
full detail): (4/6a) missing PIPE1 IPPlug config, (6b) RIF GPIO security blocking I2C2's
pins, (6c) missing `HAL_GPIO_Init()` for the camera reset/enable pins, (8) missing DCMIPP/CSI
RCC clock source config, (9) that same clock config being silently overwritten by a leftover
CubeMX-generated block in `HAL_DCMIPP_MspInit()`, plus (10) two minor logging/HAL-quirk
items. All were root-caused primarily by comparing byte-for-byte against `../Camera_N6_AI_Test`,
a sibling project confirmed working on this exact same board/camera -- that comparison
methodology (diff the two `main.c`/`stm32n6xx_hal_msp.c` files section by section rather than
guessing from first principles) is what actually cracked each one; worth reaching for first
if a similar "works there, not here" situation comes up again.

### Next steps / open items for the next session

1. **Nothing is currently broken or blocking.** If picking this project back up, start by
   reading this file top to bottom for context rather than re-deriving it.
2. The RAW8/PHY_BT_900 and RAW10/PHY_BT_450-mismatch scenarios documented earlier in
   Camera_README.md (before the PIPE1 section) were never touched/fixed this session -- they're
   illustrations of a mismatched config, not live bugs, and the current code already uses the
   matched value. Nothing to do there unless someone wants to re-verify that documented
   behavior for its own sake.
3. Nothing in this session touched `imx219.c`/`imx219_port.c`/`imx219_reg.c` — the I2C driver
   itself was never the problem, don't re-litigate it. Also, the AWB/AE color output itself
   (the captured frame's actual visual correctness/color balance) was never evaluated --
   only that a full, non-overrun frame reaches RAM. If someone reports the image looks wrong
   colorwise, that's new territory (`ISP_MW/isp/Src/isp_algo.c`, `evision` AWB/AE libs),
   not covered by anything fixed this session.
4. If a *Release* build is ever flashed, re-check `Appli.bin` size against
   `EXTMEM_LRUN_SOURCE_SIZE` (`build_and_flash.sh`/`.bat` already assert this and fail loudly
   if it doesn't fit, so this should be self-guarding, but worth knowing why if it ever trips).
5. `.bat` (Windows) script was written and reviewed carefully but **never actually run on
   Windows** — only `.sh` has been exercised end-to-end on real hardware. If a Windows user
   hits an issue with it, start there.
6. `SOT_L0`/`SOT_L1` counters (added in step 5's diagnostics) stayed at 0 even in the fully
   successful run in step 10 -- never investigated why, since it didn't block anything. Only
   worth chasing if it turns out to matter for something later; low priority.
