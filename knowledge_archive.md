# Knowledge Archive — STM32N6 Camera (CSI/DCMIPP) Bring-up

This document explains, from first principles, how the camera pipeline on the STM32N6
(NUCLEO-N65X0Q-ISP board + IMX219 sensor) actually works, and walks through every bug found
and fixed while bringing it up. It's written for someone who has never touched MIPI CSI-2 or
this chip before — if you know basic C and "camera modules send pixels somehow," you should be
able to follow all of it. §§1-8 cover the raw sensor→RAM capture path (I2C, CSI-2/DCMIPP, RIF,
boot chain). §9 covers what happens *after* a frame is in RAM — the USB UVC streaming pipeline
(JPEG encoding, USBX). §10 covers the closed-source auto-exposure (AEC) library and the
multi-session debugging saga to stop it flickering.

For the terse, chronological engineering log (what was tried, in what order, with what
hardware evidence), see [WORKLOG.md](WORKLOG.md). This document is the "why does any of this
work at all" companion to that log.

---

## 1. The big picture: how does a pixel get from the sensor to RAM?

```
IMX219 sensor  --MIPI CSI-2 (2 lanes)-->  CSI D-PHY  -->  DCMIPP  -->  RAM (frame buffer)
   (I2C control)                         (receiver)      (pipeline)
```

Three physically separate things have to work, in order, for a single frame to arrive:

1. **I2C control path** — a slow, boring 2-wire bus (SCL/SDA) used only to *configure* the
   sensor (resolution, exposure, "start streaming now") and read back its identity register.
   No pixel data ever goes over I2C.
2. **CSI-2 D-PHY receiver** — a totally separate, high-speed differential signaling block
   (2 data lanes + 1 clock lane on this board) that receives the actual pixel bytes from the
   sensor. This is "MIPI" — a different physical layer, different clocks, different silicon
   block than I2C.
3. **DCMIPP (Digital Camera Interface Pixel Pipeline)** — an on-chip hardware pipeline that
   takes the raw bytes the D-PHY receiver hands it, understands the CSI-2 packet format
   (frame start/end markers, per-line data), optionally reformats/color-converts the pixels,
   and DMAs the result into RAM over the AXI bus.

The critical insight for debugging this kind of system: **each of these three has its own
clock, its own power/reset sequencing, and its own way of silently doing nothing if
misconfigured.** A `HAL_xxx_Init()` call returning `HAL_OK` only means "the register write
succeeded" — it says nothing about whether the physical thing behind that register is actually
receiving/sending real signals. This distinction is the root cause of almost every bug in this
project's bring-up (see §4).

---

## 2. I2C control path

Standard STM32 I2C peripheral (`I2C2` here), talking to the IMX219 at 7-bit address `0x10`
(`0x20` in the 8-bit form HAL functions want). Two things have to be true before I2C will work
at all, independent of anything CSI/DCMIPP related:

- **The GPIO pins must be configured as the I2C alternate function.** `HAL_I2C_Init()` only
  configures the *I2C peripheral's own registers* — it does **not** touch GPIO pin modes. If
  nobody calls `HAL_GPIO_Init()` on the SCL/SDA pins with `GPIO_MODE_AF_OD` first, the pins
  stay in whatever mode they were left in (input, or some other peripheral's alternate
  function) and I2C transactions on the bus go nowhere, even though `HAL_I2C_Init()` reports
  success.
- **The camera's own power/reset pins must be driven correctly.** The IMX219 module has a
  `CAM_NRST` (active-low reset) and `EN_MODULE` (power enable) pin, both ordinary GPIOs. If the
  sensor is held in reset or never powered up, it won't ACK on I2C no matter how correctly the
  bus itself is configured — from the MCU's point of view this looks *identical* to a GPIO
  wiring/pin-mode problem, which made this one of the harder bugs to isolate (see bug B in §4).

**Debugging tool added this session**: `I2C_ScanBus()` in `main.c` — an i2cdetect-style scan
that probes all 128 possible 7-bit addresses and prints a full grid (not just hits). This
answers the very first diagnostic question in any I2C problem: "is *anything* on this bus
responding at all, or is it just this one device?" A fully empty grid means bus/power/reset
wiring, not a driver bug in the specific sensor's code.

---

## 3. CSI-2 D-PHY + DCMIPP pixel path

This is the part with real subtlety. Skip ahead if you just want the bug list (§4) — this
section is the "why" behind bugs D and E specifically.

### 3.1 Clock tree: two *different* clocks feed two *different* blocks

People new to this chip assume "the camera has one clock." It doesn't — DCMIPP and the CSI
D-PHY each need their own clock, generated from completely independent dividers off the same
PLL:

```
PLL1 (1200 MHz, shared system PLL)
  ├── IC17 divider ──► DCMIPP pixel clock   (this project: /4  = 300 MHz)
  └── IC18 divider ──► CSI D-PHY config clk (this project: /60 =  20 MHz)
```

"IC" here stands for **Input Clock** — the STM32N6's RCC has a bank of these generic
programmable dividers (IC1..IC18ish) that peripherals pick from, instead of every peripheral
having its own dedicated prescaler. You tell the RCC "IC17 sources from PLL1, divide by 4,
and DCMIPP consumes IC17" via `HAL_RCCEx_PeriphCLKConfig()`.

**Neither of these clocks is optional.** The CSI D-PHY receiver in particular needs its
20 MHz config clock just to be able to attempt HS (high-speed) synchronization with the
sensor's clock lane. Without it, the receiver just sits idle forever — it isn't "trying and
failing," it's "never even trying." This distinction matters because it means there's no error
flag to look for; you have to know to check the clock config in the first place (see bug D).

### 3.2 CSI D-PHY receiver states: how to tell "idle" from "syncing" from "receiving"

The `CSI->SR1` status register's bits tell you exactly which of these three states the
receiver is in. The three states, in order, that the D-PHY *should* pass through when
everything works:

1. **Ultra-Low-Power (ULP) / idle** — the receiver's default resting state when no clock is
   configured or the sensor isn't driving the lines yet. Bits: `ULPNCLF` (clock lane),
   `ULPNACTF`, `ULPNDL0F`, `ULPNDL1F` (data lanes 0/1) — "ULP No-longer... Flag" naming is
   confusing, but in practice: **if these bits are set and nothing else changes over time, the
   D-PHY has never left idle.**
2. **HS sync attempt** — once the sensor starts driving its clock+data lanes and the D-PHY has
   a working config clock, it attempts byte/word synchronization. You'd see `SOT` (Start-Of-
   Transmission) related bits toggle.
3. **Active reception** — frames actually flowing; DCMIPP's frame-complete interrupt fires
   per frame.

When this project's D-PHY was stuck in state 1 forever (bug D), `CSI->SR1` read
`0x64100000` — decoding bits 20/26/29/30 against `stm32n657xx.h`'s bit definitions showed
`ULPNDL0F`/`ULPNDL1F`/`ULPNACTF`/`ULPNCLF` all set, and *nothing else* — no error flags, no
`ESOTDL0F` (error bits), just permanent idle. That absence of error flags is itself the tell:
a receiver that's trying and failing sets error bits; a receiver with no clock just never
tries.

### 3.3 DCMIPP pipes: PIPE0 vs PIPE1 vs PIPE2

The DCMIPP hardware has three parallel output "pipes" that can each independently tap the
incoming pixel stream and write it to a different place in RAM, in a different format:

- **PIPE0** — raw dump pipe, typically used to save the untouched Bayer/RAW data.
- **PIPE1** — the "main" pipe in this project: takes RAW10 Bayer input, and (through this
  project's ISP middleware, `ISP_MW/evision`) processes it into a debayered/color-corrected
  output (`DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1` in this project's config).
- **PIPE2** — ancillary/statistics pipe.

**This project only ever starts PIPE1.** That single fact explains two apparent "bugs" that
turned out not to be bugs at all:
- `HAL_DCMIPP_PIPE_GetDataCounter()` always reads Pipe0's counter register (`P0DCCNTR`)
  regardless of the `Pipe` argument you pass it — an ST HAL driver limitation, not something
  this project's code controls. Since Pipe0 is never started, that counter reads 0 forever,
  even on a perfectly successful PIPE1 capture. Don't use it as a success signal; use
  `Camera_CheckFrameBuffer()`'s non-zero-byte count instead (§3.5).
- The IPPlug (§3.4) fix could safely give PIPE1's AXI client the *entire* FIFO pool, because
  PIPE0's client isn't competing for it.

### 3.4 IPPlug: the AXI write-side buffer nobody configures by default

"IPPlug" is ST's name for DCMIPP's internal AXI *write-master* interface — the part that
actually issues burst writes out onto the chip's AXI bus into RAM. Each pipe's write client
(`DCMIPP_CLIENT1`/`CLIENT2`/etc.) has:
- an internal FIFO (buffer between "pixels arriving from the pipeline" and "AXI write bursts
  going out"),
- a burst size (how many bytes per AXI transaction),
- a max-outstanding-transactions count (how many AXI writes can be in flight at once before
  the client has to stall and wait for one to complete).

**None of this is configured by `HAL_DCMIPP_PIPE_SetConfig()`.** It's a separate call,
`HAL_DCMIPP_SetIPPlugConfig()`, and if you never make it, the client is left at hardware-reset
defaults — effectively zero FIFO depth, zero headroom. The practical symptom: the pipe starts
fine, captures the first few lines of a frame correctly, and then the DCMIPP *overrun* flag
(`P1SR.OVRF` / `CMSR2.P1OVRF`) sets and the frame truncates mid-line. This is exactly what this
project's original `Camera_README.md` "Lỗi" (bug) section documented, and it's the single bug
this entire investigation was originally opened to fix (§4, bug 1).

Why does this only bite *some* configs and not others? Because the extra pipeline latency added
by turning on Bayer→RGB conversion, downsizing, etc. between the CSI input and the AXI output
is exactly the kind of thing that turns "the near-zero default buffer was juuust barely enough"
into "now it isn't." A raw passthrough pipe might survive with defaults; this project's
ISP-processed PIPE1 output did not.

### 3.5 How to actually verify a capture succeeded (not just "no error")

`HAL_DCMIPP_CSI_PIPE_Start()` returning `HAL_OK` only means the start sequence was accepted —
it says nothing about whether a frame arrived. The trustworthy signals, in order of strength:

1. **`HAL_DCMIPP_PIPE_FrameEventCallback` fires at all.** If `frame_count` (incremented there)
   never leaves 0, no frame has ever completed — check the D-PHY/clock chain (§3.2), not the
   pipe config.
2. **`P1SR.OVRF` / `CMSR2.P1OVRF` are both 0 after capture.** Either set means the frame
   truncated mid-transfer (IPPlug/FIFO problem, §3.4).
3. **The frame buffer's actual byte content.** `Camera_CheckFrameBuffer()` in `main.c`
   memsets the buffer to 0 *before* capture, then after capture counts non-zero bytes and
   reports `non_zero_count / FRAME_BUFFER_SIZE`. Anything less than 100% means a truncated
   frame even if the overrun flags happened to read clean at the moment you checked them —
   this is the strongest signal because it's checking the actual data, not a status register
   that could have been cleared or missed.

**Don't stop at "no error printed."** Multiple bugs in this project's history (RIF silently
dropping writes, `HAL_Init()` succeeding on unconfigured GPIOs) demonstrate that this chip's
HAL layer will happily report success for operations that accomplished nothing in hardware.
Always check final data, not just return codes.

---

## 4. The six real bugs, in the order they were found

Each of these was found by comparing byte-for-byte against `../Camera_N6_AI_Test`, a sibling
project confirmed working on the exact same board + camera module. That comparison
methodology — diff the two projects' `main.c`/`stm32n6xx_hal_msp.c` section by section instead
of guessing from first principles — is what actually cracked each bug, and is worth reaching
for first if a similar "works there, not here" situation comes up again.

### Bug 1 — PIPE1 IPPlug never configured (the original README bug)
**Symptom**: DCMIPP overrun flag set, frame truncates partway through.
**Cause**: see §3.4 — `HAL_DCMIPP_SetIPPlugConfig()` for `DCMIPP_CLIENT2` (PIPE1's AXI write
client) was never called.
**Fix**: added the call in `MX_DCMIPP_Init()`, full FIFO pool since PIPE0 is unused, 128-byte
bursts, 16 outstanding transactions.

### Bug 2 — RIF silently blocking I2C2's GPIO pins
**Symptom**: `I2C_ScanBus()` found *nothing* — a completely empty grid, not even the IMX219 —
despite `HAL_I2C_Init()` reporting success.
**Cause**: RIF (Resource Isolation Framework, §5) had `GPIOB_PIN_10`/`GPIO_PIN_11` (which
double as `I2C2_SCL`/`I2C2_SDA` on this package) marked as security-restricted, as leftover
CubeMX codegen for an Ethernet peripheral this project doesn't use at all. RIF doesn't throw an
error when a disallowed master tries to write a restricted pin — **it just drops the write**.
So `HAL_GPIO_Init()`/register writes on those pins "succeeded" in the sense of not crashing,
but never took effect.
**Fix**: removed the entire unused Ethernet RIF/GPIO-security block; added the missing
`RIF_RISC_PERIPH_INDEX_CSI` slave-secure-attribute call (present in the working reference,
missing here).

### Bug 3 — camera reset/enable GPIOs never initialized as outputs
**Symptom**: with bug 2 fixed, still nothing on I2C. Camera's own status LED never lit.
**Cause**: `MX_GPIO_Init()` only enabled GPIO port clocks — it never called `HAL_GPIO_Init()`
for *any* pin. Every `HAL_GPIO_WritePin()` call targeting `CAM_NRST`/`EN_MODULE` was writing to
pins still in their post-reset default state (not even configured as outputs), and GPIOO's
clock specifically wasn't enabled at all. This had gone unnoticed during SWD RAM-load testing
(the board's own pull-up apparently kept `CAM_NRST` high by luck), but broke for real once
booting via FSBL — FSBL reconfigures many GPIOs for its own external flash interface before
jumping to the app, and nothing ever reclaimed these two pins afterward.
**Fix**: explicit `__HAL_RCC_GPIOO_CLK_ENABLE()` + `HAL_GPIO_Init()` (push-pull output) for
both pins, before they're first written. **This was the fix that actually got I2C responding.**

### Bug 4 — DCMIPP/CSI peripheral clocks never configured
**Symptom**: I2C now works, IMX219 configures and reports "streaming started" — but zero
frames ever arrive. `CSI->SR1` shows the D-PHY permanently in ULP/idle (§3.2).
**Cause**: nobody had ever called `HAL_RCCEx_PeriphCLKConfig()` for
`RCC_PERIPHCLK_DCMIPP`/`RCC_PERIPHCLK_CSI` — the sensor's own I2C-configured streaming mode is
irrelevant if the *receiver's* clock was never running to begin with.
**Fix**: added `MX_DCMIPP_ClockConfig()` (IC17 = PLL1/4 for DCMIPP, IC18 = PLL1/60 for CSI),
called before `MX_DCMIPP_Init()`.

### Bug 5 — that same clock config silently overwritten immediately after
**Symptom**: identical failure to bug 4, byte-for-byte, even after fix 4 was applied and
confirmed to return `HAL_OK`.
**Cause**: `HAL_DCMIPP_Init()` (called *inside* `MX_DCMIPP_Init()`, which runs *after*
`MX_DCMIPP_ClockConfig()`) triggers `HAL_DCMIPP_MspInit()`, which still had CubeMX's
**default**-generated `RCC_PeriphCLKInitTypeDef` block — routing CSI's clock through PLL4
instead of PLL1 — running *after* the fix and silently winning (same peripheral-clock
selection touched twice, last write wins). This is the single most instructive bug in this
project: a fix that is correct, confirmed applied (`HAL_OK`, register readback all matched),
and *still didn't work* because something else, later in the same init sequence, undid it.
**Fix**: deleted the stale default RCC block from `HAL_DCMIPP_MspInit()` in
`stm32n6xx_hal_msp.c`, matching the reference project's own (deliberately edited) MSP init.
⚠️ **This code lives outside `USER CODE` markers — a future "Generate Code" in CubeMX will
regenerate it and silently reintroduce this exact bug.** If you ever regenerate this project,
re-check `HAL_DCMIPP_MspInit()` for this block.

### Bug 6 — two minor/cosmetic issues found in the first fully successful log
- `printf("ok\n")` in the idle loop used `\n` alone instead of `\r\n` like every other printf
  in the file, causing a "staircase" indentation artifact on the terminal.
- `HAL_DCMIPP_PIPE_GetDataCounter()` always reading 0 — not a bug, see §3.3.

---

## 5. RIF (Resource Isolation Framework) — the silent-failure trap

RIF is the STM32N6's peripheral/GPIO access-control system: every bus master (CPU, DMA
channels, specific peripherals) and every GPIO pin can be tagged secure/non-secure,
privileged/unprivileged, and restricted to specific "CID" (compartment ID) masters. It exists
for genuine security isolation between trusted and untrusted code running on the same chip.

**The property that makes RIF dangerous to debug against**: when a master that isn't allowed
to touch a RIF-restricted resource tries to write to it, **RIF drops the write instead of
faulting**. There's no bus fault, no HAL error return, nothing. From the writing code's point
of view, the write "succeeded" — the peripheral's own init function returns `HAL_OK` — but the
actual hardware register never changed. This is exactly what happened in Bug 2: `HAL_I2C_Init()`
was completely happy, and the I2C peripheral was, in isolation, correctly configured — but the
*pins* it needed were RIF-locked to a different compartment, so the peripheral was talking to
nothing.

**Practical rule learned from this project**: if a peripheral's `Init()` succeeds but the
peripheral behaves as if it's not connected to anything, check RIF/GPIO security attributes for
its pins before re-examining the peripheral's own config — especially for any pin or peripheral
that's part of CubeMX-generated boilerplate for a feature (like Ethernet here) that the project
doesn't actually use. Leftover unused-feature security config is a very easy way to
accidentally lock down pins that got reassigned to something else.

---

## 6. TrustZone / CMSE — why this matters here even without touching security code directly

The STM32N6 (Cortex-M55) supports ARM TrustZone: code can run in a Secure or Non-secure world,
with hardware-enforced isolation between them. This project's Appli is built with `-mcmse`
(CMSE = Cortex-M Security Extensions), and there's a `Secure_nsclib`/`secure_nsc.c` providing
the non-secure-callable (NSC) entry points the FSBL (which runs Secure) exposes to the app.

You don't need to understand TrustZone deeply to work on the camera pipeline — none of the six
bugs above were TrustZone bugs — but it's worth knowing it's there because:
- The FSBL and Appli are **separately built and separately flashed** (`FSBL/` and `Appli/` are
  two different CMake projects, two different `.bin` files, see §7).
- RIF (§5) and TrustZone are related-but-distinct isolation mechanisms that can compound: a
  resource can be both RIF-restricted *and* TrustZone-Secure-only. This project's
  `SystemIsolation_Config()` function is doing RIF configuration specifically (RIMC master
  attributes, RISC slave attributes), not TrustZone world assignment — don't confuse the two
  when reading STM32N6 documentation.

---

## 7. Boot chain: how does code even start running on this board?

Unlike a typical "just flash one .bin and reset" MCU workflow, this board's story is:

1. **ST's BootROM** (fixed, in silicon, can't be changed) reads a boot-mode configuration and
   loads the **FSBL** (First-Stage Boot Loader) — the only binary the BootROM knows how to
   authenticate. The FSBL image is **signed** with `STM32_SigningTool_CLI`, because it's the
   only stage the immutable BootROM will accept.
2. **FSBL runs from AXISRAM2** (small internal RAM), initializes the external XSPI NOR flash
   controller, and hands off to the **Appli** image using ST's generic
   `STM32_ExtMem_Manager` "LRUN" mechanism (`Middlewares/ST/STM32_ExtMem_Manager/boot/
   stm32_boot_lrun.c`):
   - `BOOT_Application()` does a **fixed-size, unsigned** `memcpy` from external flash offset
     `EXTMEM_LRUN_SOURCE_ADDRESS` to RAM at `EXTMEM_LRUN_DESTINATION_ADDRESS`, then jumps to
     `destination + EXTMEM_HEADER_OFFSET` (skipping a 1KB padding header). **No signature
     check, no embedded size field** — the copy size is a compile-time constant
     (`EXTMEM_LRUN_SOURCE_SIZE`) on the FSBL side, and if it's smaller than the actual Appli
     image, Appli gets silently truncated on every boot. This was a real bug found and fixed
     this session (`0x10000` → `0x40000`, see WORKLOG entry 3).
   - Because of this, Appli's flash image is just `[1KB zero padding][Appli.bin]` — **not**
     run through the signing tool. Only FSBL is signed, because only FSBL is BootROM-loaded.
3. **Appli runs from RAM** (`_LRUN` linker script — "Load-and-RUN"), having been copied there
   by FSBL. This is different from the sibling `STM32N6_Face_Detection` project, which uses a
   different, hand-rolled boot container format — don't assume boot mechanisms are
   interchangeable between STM32N6 projects even on the same board.

`build_and_flash.sh dev` (SWD RAM-load, for fast iteration) and `build_and_flash.sh flash`
(the real FSBL→external-flash→Appli chain above, for testing the actual boot path) are
deliberately different code paths — `dev` mode bypasses FSBL entirely and is *not* representative
of what happens on a cold power-on. Bug 3 (§4) is the textbook example of why this distinction
matters: it was invisible in `dev` mode and only appeared once FSBL was actually in the loop.

---

## 8. General lessons for the next debugging session on this chip

1. **A `HAL_OK` return proves the write instruction executed, not that the hardware did what
   you asked.** RIF can silently drop writes (§5); MSP init functions can silently overwrite a
   config you set moments earlier (Bug 5); GPIO pins can be written before being configured as
   outputs. Trust final observed hardware state (register readback, actual buffer contents),
   never just the return code chain.
2. **When something "works there, not here," diff the two projects' init sequences file by
   file, function by function** — this found all six bugs in this project faster than any
   amount of reasoning from the datasheet alone would have.
3. **CubeMX-generated code outside `USER CODE BEGIN/END` markers will be silently regenerated**
   if the `.ioc` is ever re-run through CubeMX. Bug 5's fix lives in exactly such a region —
   documented with an explicit regen warning in the code for this reason.
4. **`dev` (SWD RAM-load) and `flash` (real boot chain) are not equivalent test environments**
   on this board — GPIO/peripheral state left behind by FSBL only exists in the `flash` path.
   A bug that only reproduces in one of the two isn't necessarily flaky; check what's different
   about that boot path specifically before assuming it's an intermittent hardware issue.
5. **Distinguish "receiver is trying and failing" from "receiver was never told to try."** No
   error flags set is not the same as no error — for CSI D-PHY specifically, check the clock
   config before assuming the sensor or wiring is at fault (§3.2).
6. **Log analysis alone is not proof of mechanism.** In the flicker saga (§10), three separate
   theories in a row — frame-length coupling's exact scope, JPEG-encode CPU duration, a
   USB-streaming correlation — were each built from real, consistent log evidence, and each was
   individually disproven by the next hardware test. Log correlation tells you *what changed
   together*; it does not tell you *why*. When a fix based on log analysis doesn't hold up, the
   next step is a controlled experiment that isolates one variable (board physically stationary,
   one known scene change at a time) rather than a fourth theory built from the same logs.
7. **A config struct's field list is the ground truth for what's tunable — check it before
   assuming a knob exists.** `ISP_AECAlgoTypeDef` (§10.2) has exactly three fields. No amount of
   searching for "the right value" to fix AEC's convergence speed will succeed, because there is
   no convergence-speed field to set — that logic is compiled into the closed-source `.a` file.
   Reading the actual `struct` definition (not the docs, not what similar libraries usually
   expose) settles this in seconds and avoids a lot of blind tuning.

---

## 9. USB UVC streaming pipeline: from a captured frame to a USB video packet

This section picks up *after* §3 already got a frame into `video_buf[0]`/`video_buf[1]` in RAM.
Getting that frame onto a PC as a UVC (USB Video Class) webcam feed is a second, mostly
independent pipeline, with its own concurrency model and its own class of bugs.

### 9.1 The three stages, and which thread/context each one runs in

```
DCMIPP frame-complete IRQ         CaptureUVC_Thread (prio 5)        USBX video write thread (prio 20)
        |                                  |                                    |
 Capture_OnFrameComplete()        ISP_BackgroundProcess()      USBD_VIDEO_StreamPayloadDone()
  - swap ready/capture buffer      (pumps AEC/AWB, §10)          -> fill_uvc_payload()
  - DCache invalidate                                              - once per JPEG frame: JPG_Encode()
  - tx_semaphore_put(frame_ready)                                  - every call: copy next ~1KB chunk
                                                                      into the USB payload buffer
```

- **`Capture_OnFrameComplete()`** (`app_threadx.c`) runs from the DCMIPP frame-event callback —
  effectively interrupt-adjacent, so it does the absolute minimum: flip which of the two
  `video_buf[]` halves DCMIPP writes into next, invalidate the D-Cache for the buffer that just
  finished (so CPU reads of it see real pixel data, not stale cache lines), and post a semaphore.
  It does **not** touch USB or JPEG at all.
- **`CaptureUVC_Thread`** (priority 5, created in `app_threadx.c`) waits on that semaphore, then
  calls `ISP_BackgroundProcess()` once per wake — this is the thread that keeps the AEC/AWB
  library alive (§10). It also does periodic `[UVC_CAP]` diagnostic printfs.
- **The actual JPEG encode happens on a *third*, lower-priority thread you don't create
  yourself**: USBX's own internal video-streaming thread
  (`_ux_device_class_video_write_thread_entry`, created by the USBX video class at
  `UX_THREAD_PRIORITY_CLASS` = 20 — a much *lower* priority number-wise-higher-is-lower-priority
  than `CaptureUVC_Thread`'s 5). Every time a USB isochronous IN payload finishes transmitting,
  USBX calls `USBD_VIDEO_StreamPayloadDone()` (`ux_device_video.c`) on **that** thread, which
  calls `fill_uvc_payload()`. At the start of each new JPEG frame (`uvc_frame_offset == 0`),
  `fill_uvc_payload()` calls `JPG_Encode()` synchronously — this is the one expensive step in the
  whole chain, and it blocks *that* USBX thread (not `CaptureUVC_Thread`) for however long it
  takes. Every other call that frame just memcpy's the next ~1KB chunk of the already-encoded
  JPEG buffer into the payload — cheap.

### 9.2 Why there's a software double-buffer instead of using DCMIPP's own hardware DBM mode

DCMIPP supports a hardware "double-buffer mode" (DBM) where the peripheral itself alternates
between two fixed addresses every frame, no software involved. This project doesn't use it,
because DBM can't express the one thing that actually matters here: *"don't touch this buffer,
the JPEG encoder is still reading it."* If DCMIPP is free-running between two fixed buffers and
the encoder is slower than the frame rate (it is — see §9.3), DCMIPP will eventually write into
the buffer the encoder is still reading mid-encode, tearing the image.

The fix implemented here is a software-mediated ping-pong: `Capture_OnFrameComplete()` only
retargets DCMIPP to the *other* buffer if that buffer isn't the one currently locked by
`fill_uvc_payload()` (`uvc_locked_buf_idx`). If the encoder still holds it, DCMIPP keeps
overwriting the buffer it just finished (dropping that one camera frame) rather than touching the
locked one. This trades an occasional dropped capture frame for guaranteed torn-frame-free output
— the right trade for a viewable video stream.

### 9.3 Why JPEG encoding needs a staging buffer regardless of source pixel format

The STM32N6's hardware JPEG encoder (`HAL_JPEG_Encode`, used here in polling mode) consumes
pixel data in **MCU-block order** (8×8 or 16×8 pixel blocks, per the JPEG spec's minimum coded
unit), not in the scanline order the camera/DCMIPP produces. This is true *no matter what pixel
format the source is* — RGB565 or YUV422, a reordering staging buffer (`mcu_buffer`) is
unavoidable. What the source format *does* change is how expensive filling that staging buffer
is:

- **From RGB565** (`CVT_FormatRgb565ToYuv422Jpeg()`): has to do real RGB→YUV color-space math
  (multiply-accumulate per pixel) on top of the reordering. Measured cost on this hardware:
  ~18-22ms per 640×360 frame.
- **From YUV422** (`CVT_FormatYuv422ToYuv422Jpeg()`, when DCMIPP's own hardware color-conversion
  block already output YUV422 — §10 mentions this pipeline choice too): pure byte reordering, no
  color math at all. Measured cost: ~5-6ms for the same frame.

Since the hardware hands DCMIPP's own YUV conversion block the RGB→YUV math almost for free
(it's a fixed pipeline stage, not extra CPU work), sourcing YUV422 instead of RGB565 is a
straightforward, close-to-free 3-4x cut in `JPG_Encode()`'s blocking time — useful context if a
future change needs to claw back CPU/USB-thread time (this was tried as a fix for a different
problem in §10 and, on its own, didn't turn out to be the actual lever for that specific bug —
but it's a real, measured win regardless).

### 9.4 Sizing the frame/JPEG buffers

`video_buf1`/`camera_framebuffer` are sized to the **cropped** output resolution (640×360, via
DCMIPP's own hardware crop — see §10.3's frame-length/FPS discussion for why cropping matters
here too), not the sensor's full native frame. `mcu_buffer`'s size is independent of pixel format
(§9.3) — reformatting for JPEG always needs the full reordering buffer regardless of which
`CVT_Format...` path fills it.

---

## 10. Auto-exposure (AEC): how it's wired up, and the flicker debugging saga

### 10.1 What "ISP_MW/evision" actually is

This project uses ST's closed-source ISP middleware, `ISP_MW/evision` — two precompiled static
libraries (`libn6-evision-st-ae_gcc.a` for auto-exposure/AE, `libn6-evision-awb_gcc.a` for
auto-white-balance/AWB) linked into the app with **no source available**. The only way to
interact with them is:

- **Config in**: `ISP_IQParamTypeDef` (`isp_param_conf_imx219.h`) — a big struct of mostly
  static/one-time tuning values (demosaic strength, static gains, the AEC/AWB enable flags and
  the handful of fields each exposes).
- **Pump loop**: `ISP_BackgroundProcess()`, called every frame from `CaptureUVC_Thread`'s main
  loop (§9.1) and also from `main.c`'s pre-RTOS warm-up loop. This is what actually runs the
  AE/AWB algorithms against fresh statistics each frame — nothing happens without it being called
  regularly.
- **appliHelpers callbacks**: `GetSensorInfoHelper`/`SetSensorGainHelper`/
  `SetSensorExposureHelper`/`GetSensorGainHelper`/`GetSensorExposureHelper`, registered once in
  `main.c`. This is the *entire* interface between the closed-source algorithm's decisions and
  real IMX219 I2C register writes — the algorithm never touches the sensor directly, it always
  goes through these.

**The one invariant that matters most**: whatever `SetSensorGainHelper`/`SetSensorExposureHelper`
actually applies to hardware must be exactly what `GetSensorGainHelper`/`GetSensorExposureHelper`
reports back on the next call. The algorithm keeps its own internal model of "what did I last
tell the sensor to do," and if that model diverges from reality — because the helper silently
clamped/delayed the value without saying so, or because something else (dynamic frame length,
§10.3) changed sensor timing behind its back — its next correction is computed from a wrong
premise and convergence breaks. Every fix that worked in this saga (§10.4's slew-limiting) obeys
this invariant; every approach that fought it from outside without preserving it either didn't
help or made things worse.

**Statistics**: the algorithm reads back two brightness figures per frame, `stats.down` (measured
*after* `ispGainStatic`'s R/G/B multiplication is applied) and `stats.up` (a reverse-computed
estimate of what the brightness was *before* that gain). AEC's own exposure decisions are driven
by `stats.down` — meaning any static gain applied in `ispGainStatic` directly biases what the
algorithm believes the scene's true brightness is. A gain boost there once fooled AEC into
under-exposing in bright light (documented in WORKLOG.md's Bug 23) — a good example of §10.1's
invariant being violated one level up, in the ISP gain stage rather than the sensor stage.

### 10.2 The AEC config surface is much smaller than you'd expect

`ISP_AECAlgoTypeDef` (`isp_core.h`) is:

```c
typedef struct
{
  uint8_t enable;
  ISP_ExposureCompTypeDef exposureCompensation;  /* -2.0 EV .. +2.0 EV in 0.5 EV steps */
  uint32_t exposureTarget;                       /* derived from exposureCompensation, not set directly */
  ISP_AntiFlickerTypeDef antiFlickerFreq;         /* 0, 50, or 60 (Hz) */
} ISP_AECAlgoTypeDef;
```

That's the entire tunable surface. There is **no damping, speed, hysteresis, or convergence-rate
field anywhere in this struct.** Whatever makes the algorithm converge quickly or slowly, smoothly
or in large steps, is compiled into the `.a` file and cannot be adjusted from application code —
confirmed by reading the struct definition directly, not inferred from behavior. This matters a
lot for §10.4: it rules out an entire category of "just tune the AEC to be gentler" fixes before
they're even tried.

### 10.3 Why frame length can't be dynamic (the FPS/exposure trade-off)

The sensor's frame period (`FRM_LENGTH_LINES`, IMX219 registers `0x0160`/`0x0161`) sets a hard
ceiling on how long a single exposure can be — exposure is specified in *lines*, and can never
exceed the frame length. A short frame length gives high FPS but caps how much exposure AEC can
ask for in low light; a long frame length gives more exposure headroom (better low-light image)
at the cost of FPS. On this sensor/lens/binning config, measured on real hardware:
`FRM_LENGTH_LINES=1763` → ~31 FPS, `FRM_LENGTH_LINES=3526` → ~16 FPS.

The tempting design (and the one this project tried first) is to make this dynamic: let AEC's own
exposure decisions drive `FRM_LENGTH_LINES` live — grow it only when an exposure request needs
more room, shrink it back when there's enough light to run fast. This **does not work** with a
closed-source AEC that has no way to be told its own sensor's timing just changed underneath it.
Every version tried — continuously recomputing the frame length every call, then a coarser
2-state switch with a 60-call shrink debounce — still broke AEC's convergence (visible as
flickering brightness), because changing `FRM_LENGTH_LINES` mid-stream violates §10.1's invariant
in a way neither helper function can paper over: the sensor's actual line-time changes, so a
given exposure register value now corresponds to a different real exposure duration than AEC
believes.

**The confirming evidence, from comparing against ST's own official IMX335 camera pipeline**
(`STM32N6_Face_Recognition`, `stm32-mw-camera/sensors/imx335/imx335.c`): the real reference driver
ships five pre-validated register tables, one per supported FPS (10/15/20/25/30), and FPS is
selected **once**, before streaming starts (`IMX335_SetFrameRate()`), never touched again while
AEC runs. Raspberry Pi/libcamera does the same thing (a fixed per-mode frame duration; its AGC
only ever trades exposure/gain *within* that fixed period). This project's fix matches that
pattern: `FRM_LENGTH_LINES` is now pinned at a single fixed value for the entire streaming
session (`DEBUG_DISABLE_DYNAMIC_FRAME_LENGTH` in `main.c` — the name is a holdover from when this
was still being isolated as a test; functionally it's now just "frame length is static," which is
the permanent, correct design here, not a temporary debug flag). If variable FPS is wanted again
in the future, it needs to be a discrete mode switch (re-init between a small number of fixed
profiles), never a live per-frame AEC-driven adjustment.

### 10.4 The flicker debugging saga: four theories, three wrong, one confirmed fix

With frame length fixed (§10.3), a *second*, unrelated flicker remained: `isp_gain`/
`isp_exposure` would swing between extremes specifically when the camera was pointed at a bright
highlight (a window, a light, outdoors), while sitting perfectly rock-stable — pinned at a single
value for minutes at a time — whenever it wasn't. Four theories were chased, in order, each
tested on real hardware rather than assumed:

1. **JPEG-encode CPU duration** (a plausible read of "oscillates only while USB streaming is
   active"): restoring DCMIPP's hardware YUV422 conversion (§9.3) cut `JPG_Encode()`'s blocking
   time from ~24ms to ~10ms — **no change** in the oscillation. Disproven.
2. **The Stream-ON/OFF correlation itself**: further testing (a controlled, hands-off setup, board
   resting untouched, pointed at a fixed scene) showed the real discriminator was never USB
   activity at all — it was almost certainly the board being physically handled/moved while a
   phone recorded the live screen during "streaming" tests, vs. resting still during "not
   streaming" gaps. Coincidental correlation, not a cause.
3. **Global exposure compensation** (`exposureCompensation`, §10.2 — the one real tuning knob
   available): `-1.0 EV` eliminated the hunting completely (rock-steady, matching the "pointed
   away from bright content" baseline) but pinned exposure/gain at the absolute floor — a fully
   black image in *every* condition, not just bright ones. `-0.5 EV` was still too dark. Root
   issue: this is a single *global* number applied to every scene uniformly; there's no value
   that keeps normal-light scenes visible while also being dark enough to prevent a bright
   highlight from forcing a large, hunt-prone correction. Retired.
4. **Slew-rate-limiting the sensor writes** (the fix that worked): clamp the per-call change to
   `isp_gain`/`isp_exposure` in `SetSensorGainHelper()`/`SetSensorExposureHelper()` to a fixed
   maximum step, and — critically, per §10.1's invariant — report back the *actually-applied,
   slewed* value via `GetSensorGainHelper()`/`GetSensorExposureHelper()`, never the raw AEC
   request. This is the same technique Raspberry Pi/libcamera's AGC uses (its "speed" parameter)
   to avoid exactly this failure mode. Confirmed on hardware: smooth, gradual brightness
   transitions instead of hard jumps, in both directions.

Why the *first* attempt at this exact fix (tried earlier in the same debugging arc, before frame
length was made static) looked like a failure at the time: a slew-limited actuator chasing a
target that's *also* being independently perturbed (by the still-live dynamic-frame-length bug,
§10.3) can never catch up — it gets stuck partway, which looked like "slew-limiting makes the
image stuck too dark" but was really two bugs compounding. Once §10.3 was fixed first, the same
technique worked cleanly. **Lesson**: when a fix doesn't work, check whether an *independent* bug
was still active during that test before concluding the fix itself was wrong.

### 10.5 Root cause, stated plainly

Simple average-luma auto-exposure (no highlight weighting, no histogram-based metering, no HDR)
pointed at a scene containing both a bright highlight and normal room content will, in general,
need a large, fast correction whenever the highlight enters or leaves the metering window — and
if the algorithm's steady-state for "normal" content already sits at a hard gain/exposure ceiling
(as it does in typical indoor light on this sensor/lens combo), that large correction starts from
right at a saturation boundary, which is where simple control loops hunt. This is a real,
well-known limitation of basic AEC algorithms in backlit/highlight scenes, not a bug unique to
this project or this session's changes — the fix available without AEC source access is damping
the actuator (§10.4, item 4), not eliminating the underlying tendency to want a large correction
in the first place.
