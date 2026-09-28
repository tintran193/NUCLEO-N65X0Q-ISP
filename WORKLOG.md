# Worklog — NUCLEO-N65X0Q-ISP

Chronological record of this project's CMake conversion + bring-up debugging, so a future
session can pick up context without re-deriving it. Newest entry on top.

---

## 2026-09-28 (latest #26) — Retried #16's slew-limiting now that the system is in a clean, well-understood state -- statAreaStatic repositioning ruled out (user confirmed bright sources appear unpredictably, no fixed direction to narrow toward)

**Context**: asked the user where bright sources typically sit in frame, to inform a
`statAreaStatic` adjustment. Answer: "Không cố định, có thể ở bất kỳ đâu" (not fixed, could be
anywhere -- camera is handheld/moved freely). This rules out spatial ROI narrowing/repositioning
as a principled fix -- there's no direction to shrink toward that wouldn't just as often make
things worse as better for an unpredictable highlight position.

**Reconsidered #16 (slew-limiting) instead of abandoning it**: #16 tried clamping the per-call
change to `isp_gain`/`isp_exposure` and reporting the slewed value back via `Get*Helper`, tested
on hardware, and got a WORSE result (image stuck too dark) -- at the time this looked like
disproof of the whole approach. On reflection: that test ran while Part 1's dynamic frame length
was STILL fully active (only disabled two steps later in #18), which #18 later proved was itself
feeding AEC inconsistent frame timing. A slew-limited actuator chasing a target that's ALSO being
independently perturbed by a second bug cannot converge -- that matches #16's "stuck in a low-mid
band" symptom without it being a real flaw in slew-limiting. The system is now in a much cleaner,
better-understood state: Part 1 disabled (#18), Part 2c restored and confirmed not the cause
(#21/#22), global EV compensation proven the wrong lever and reverted (#25). Root cause is
narrowed to exactly one clean case: AEC hunts specifically when a real highlight forces a large,
fast correction from the gain/exposure ceiling. This is a good candidate to retry slew-limiting
on its own merits -- it's the same technique Raspberry Pi/libcamera's AGC uses for exactly this.

**Change** (`Appli/Core/Src/main.c`, inside the `DEBUG_DISABLE_DYNAMIC_FRAME_LENGTH` branch that
is currently active per #18): `SetSensorGainHelper()` and `SetSensorExposureHelper()` clamp the
per-call change to `isp_gain`/`isp_exposure` to `GAIN_SLEW_MAX_STEP=24` /
`EXPOSURE_SLEW_MAX_STEP=400` (same values as #16 -- full range ramps in ~9-10 calls) before
writing to the sensor. `Get*Helper` already reads back `isp_gain`/`isp_exposure`, which now hold
the actually-applied slewed value, not the raw AEC request -- same requirement #16 established.

Build clean, RAM 64.64% (logic-only change).

**Next steps**: flash and retest pointing at the same bright area that triggered hunting before.
Watch for: (1) smoothed, gradual brightness transition instead of a hard flicker when panning
into/out of a bright area -- the target outcome; (2) getting stuck too dark again like #16 -- if
this happens even now, it would mean the slew rate itself needs to be faster (larger
`*_SLEW_MAX_STEP`), not that the technique is wrong; (3) no visible change at all -- would suggest
the earlier Stream-based correlation (#21/#22) wasn't fully resolved by physical-handling alone
and something else is still going on. If slew-limiting doesn't pan out either after this cleaner
retry, remaining options are accepting the hunting as an inherent limitation of this AEC library
in backlit/highlight scenes (a known real limitation of simple average-metering autoexposure, not
unique to this project), or exploring whether `sensorDelay`/other untried `ISP_IQParamTypeDef`
fields (`isp_core.h`) have any bearing -- not yet investigated.

---

## 2026-09-28 (#25) — -0.5 EV (#24) still too dark; reverted exposureCompensation to 0.0 EV -- global EV shift is the wrong lever for this problem

**Result**: user reports "vẫn tối" (still dark) at `-0.5 EV`. Combined with #24's `-1.0 EV`
result (fully black), the conclusion is now clear: `exposureCompensation` is a GLOBAL, uniform
shift applied to every scene equally -- there is no single EV value that keeps normal-light
scenes properly visible while also being dark enough to prevent hunting when a highlight enters
frame, because normal-light visibility and highlight-triggered overcorrection are controlled by
the SAME single number pulling in opposite directions. `0.0 EV` (hunts in bright light, correct
brightness otherwise) and `-1.0 EV` (never hunts, always too dark) are the two ends actually
measured on real hardware; `-0.5 EV` sits between and is unsatisfying on both axes rather than
solving either. This whole EV-compensation direction is retired.

**Reverted** (`Appli/Core/Inc/isp_param_conf_imx219.h`): `.exposureCompensation` back to
`EXPOSURE_TARGET_0_0_EV` (original brightness restored; hunting-in-bright-light returns, but
this is a known, better-understood state than an unusable dark camera). `antiFlickerFreq` stays
at `ANTIFLICKER_50HZ` (unaffected by this, no reason to revert it).

**Why the next fix needs to target `statAreaStatic` instead**: the real problem is that the
metering window includes both the highlight AND the room's normal dark content, so their average
swings wildly as a highlight enters/exits -- shrinking or repositioning that window (so a
highlight is less likely to dominate it, or is excluded from it) attacks the actual mechanism
instead of just darkening everything as a blunt workaround. This needs to know something about
the real scene layout to do well (asked the user; see next step) -- guessing a rectangle blind
risks another wasted flash-test cycle like the last three.

Build clean, RAM 64.64% (config-only change).

**Next steps**: waiting on the user for where bright sources (windows/lights) typically sit in
frame for this camera's real mounting/use, to choose a `statAreaStatic` adjustment with actual
justification instead of guessing.

---

## 2026-09-28 (#24) — -1.0 EV (#23) stopped the hunting entirely but overshot to a fully black image in every condition; backed off to -0.5 EV

**Result**: user flashed #23's `-1.0 EV` change and reported the image is now black in EVERY
condition ("giờ nó đen luôn kg thấy gì cả" -- now it's always black, not scene-dependent). Log
confirms: `isp_gain=0 isp_exposure=1` (the absolute floor of both) held PERFECTLY steady for the
entire log (frames=800 through 1248, hundreds of samples, zero drift) -- this is actually a
valuable positive result buried in the bad outcome: **the hunting is completely gone** (rock
steady, same character of stability as the "pointed away from bright content" baseline from #22),
confirming #23's diagnosis and mechanism were right. The problem is purely magnitude: `-1.0 EV`
(halves the exposure target per `isp_core.c`'s `pow(2, exposureCompensation/2)` formula) pulled
the target below what even the sensor's minimum gain/exposure can reach, given this metering
window still averages in the room's normal (non-highlight) content alongside any bright source --
overshot into permanent floor-clipping instead of landing on a reasonable, stable, visible
operating point.

**Fix** (`Appli/Core/Inc/isp_param_conf_imx219.h`): `.exposureCompensation` backed off from
`EXPOSURE_TARGET_MINUS_1_0_EV` to `EXPOSURE_TARGET_MINUS_0_5_EV` -- half the correction. Comment
updated in place to record the `-1.0 EV` result so a future session doesn't retry it blind.
`antiFlickerFreq=ANTIFLICKER_50HZ` kept as-is (unrelated to this overshoot).

Build clean, RAM 64.64% (config-only change).

**Next steps**: flash and retest across BOTH normal room light and the bright area/window/light
that originally triggered hunting. Three possible outcomes: (1) stable AND visible in both --
done; (2) still visible but hunting returns in bright light -- the -0.5/-1.0 EV bracket has been
measured (stable-but-black at -1.0, hunting at 0.0), so the right value likely sits at some
intermediate fraction and may need a value between (this ISP_ExposureCompTypeDef enum only offers
0.5 EV steps, so -0.5 may need to be it, or accept the residual hunting as the visible-image
tradeoff); (3) still too dark at -0.5 -- back off further toward `0.0 EV` and reconsider
`statAreaStatic` (narrowing/repositioning the metering window) as the more targeted fix instead of
a blanket global exposure shift, since that's what actually determines how much a highlight can
dominate the average.

---

## 2026-09-28 (#23) — Root cause finally isolated: AEC hunts specifically when the camera is pointed at a bright highlight (confirmed real use case: windows/lights/outdoors), not USB/encode/frame-length; tuned exposureCompensation + antiFlickerFreq (the only real tuning surface available)

**#22's controlled-test request paid off**: user reported the exact discriminator directly --
"khi tôi kg để cam chĩa vào chỗ có nhiều ánh sáng thì nó tĩnh kg nhảy, khi chĩa ra chỗ sáng thì nó
nhảy" (stable when NOT pointed at a bright area, jumps when pointed at one). The accompanying log
shows 450+ consecutive `[UVC_CAP]` samples at `isp_gain=232 isp_exposure=3522` with ZERO drift
(not pointed at bright content) -- the most stable stretch seen in this entire debugging session,
confirming the earlier Stream-ON/OFF correlation (#21/#22) really was coincidental (very likely
just "board handled/moved while watching" vs. "board resting still", as #22 speculated), not a
USB/JPEG-encode effect. Every theory chased in #15 through #22 (frame-length coupling, gain-
saturation limit-cycling as a standalone cause, JPEG-encode CPU duration) is now superseded by
this one: **the trigger is specifically pointing at a bright highlight.**

**Confirmed with the user this is a real use case** (not just a stress test) -- the camera will
genuinely be pointed at windows/lights/outdoors sometimes, so this needed an actual fix, not just
documentation of a known limitation.

**Why this happens**: classic failure mode of simple average-luma-metering AEC pointed at a
partially-saturated highlight. In this room's normal light, `isp_gain`/`isp_exposure` sit pinned
exactly at the hard ceiling (232/3522) -- itself already an awkward place for any control loop to
live (a saturation boundary, see #16's writeup on why). When a bright highlight enters the
`statAreaStatic` metering window (`isp_param_conf_imx219.h`: X0=16,Y0=16,320x240 within the
640x360 frame -- covers most of the upper 2/3 of frame), AEC needs a large, fast downward
correction from right at that boundary -- exactly the kind of large-step-from-a-clamped-start that
produces overshoot/hunting in a simple control loop. Checked `ISP_AECAlgoTypeDef`
(`isp_core.h:338-344`) directly: it exposes ONLY `enable`, `exposureCompensation`,
`antiFlickerFreq` -- no damping/speed/hysteresis knob at all, confirming (again) that this
behavior is baked into the precompiled `libn6-evision-st-ae_gcc.a` binary and cannot be tuned
from application code beyond these three fields.

**Fix** (`Appli/Core/Inc/isp_param_conf_imx219.h`): using the one legitimate vendor-provided lever
instead of another app-level workaround (this session already tried and reverted two of those --
frame-length coupling, slew-limiting -- both fighting the closed-source AEC from outside rather
than using its own config surface):
- `.exposureCompensation`: `EXPOSURE_TARGET_0_0_EV` -> `EXPOSURE_TARGET_MINUS_1_0_EV`. Moves the
  normal-light steady-state target away from the hard ceiling, so normal operation isn't sitting
  right at the saturation boundary, and shrinks how large a downward correction is needed when a
  highlight appears.
- `.antiFlickerFreq`: `0` (disabled) -> `ANTIFLICKER_50HZ` (Vietnam mains). Free, strictly correct
  for indoor AC-powered lighting (a lit room behind a window, an LED/fluorescent lamp) -- enabled
  regardless of whether it's the primary cause here, since there was no reason it was off.

Build clean, RAM 64.64% (unaffected -- config-only change, no new storage).

**Next steps**: flash and retest specifically pointing at the same bright area/window/light that
previously triggered hunting. If still unstable, `exposureCompensation` can be pushed further
(`EXPOSURE_TARGET_MINUS_1_5_EV` or `-2_0_EV`) at the cost of a dimmer normal-light image; if the
image is now uncomfortably dark in normal light, dial back toward `-0_5_EV`. If hunting persists
regardless of exposureCompensation, the next real lever would be `statAreaStatic` itself --
narrowing/repositioning the metering window to reduce how easily a bright source dominates it
(scene/mounting-dependent, not a universal fix, would need to know where bright sources typically
appear in this camera's actual mounting).

---

## 2026-09-28 (#22) — Restoring Part 2c (#21) did NOT fix the oscillation despite cutting JPG_Encode() time ~2.5x; the CPU/encode-duration theory is refuted

**Result**: user flashed #21. `[JPG] enc=... t=10-12ms (cvt=4700-6700us hal=5000-5600us)` confirms
the hardware YUV422 path is active and working as intended -- encode time dropped from
~24-28ms to ~10-12ms, roughly the 2.5x cut expected. Despite this, `isp_gain` still alternates
between `0` and `232` in blocks of a few samples throughout the whole log (`frames=33` through
`frames=305`, continuously, while streaming) -- same character of oscillation as before #21,
undiminished in frequency. This refutes #21's theory: whatever disturbs AEC while streaming is
active is NOT simply proportional to how long `JPG_Encode()` blocks the USBX video-write thread
-- cutting that blocking window by more than half changed nothing observable.

**Reconsidering the Stream-ON/OFF correlation from #21's log**: that correlation (rock-stable
during a ~1 minute `Stream OFF` window, oscillating again once `Stream ON` resumed) is still real
in the data, but with the CPU-duration theory now refuted, the mechanism connecting it to AEC is
back to unknown. A plausible alternative that has nothing to do with USB/bus contention at all:
the user was almost certainly holding/handling the board and watching the live video during the
`Stream ON` windows (recording it with a phone), and likely set it down or stopped touching it
during the `Stream OFF` gap -- ordinary hand tremor or small repositioning while actively watching
a live feed is a completely mundane explanation for real, correct AEC responses to real, small
lighting/framing changes, and would produce exactly this correlation without implicating USB or
JPEG encoding in any way. This has not been tested and is not yet a conclusion -- flagged
explicitly because three log-analysis-only theories in a row (frame-length coupling, gain-
saturation limit-cycling, JPEG-encode-duration) have now each been individually disproven by
hardware retests, which is a strong signal to stop inferring mechanism from `[UVC_CAP]` log lines
alone and gather a more controlled data point instead.

**No code change this round.** Requested from the user instead: a controlled test with the board
resting untouched on a surface, camera pointed at a completely static scene (nothing moving in
frame, no one handling the board), streaming continuously for at least 20-30s, then report
whether `isp_gain`/`isp_exposure` still swing between extremes under those conditions. This
directly discriminates the two remaining candidates -- if it's now stable, physical handling
during active viewing was the real explanation all along (in which case Part 1-#18 and Part 2c
were both legitimate, working fixes and nothing further needs chasing); if it still oscillates
under a fully static, hands-off setup, USB/streaming-linked interference is confirmed real and
worth continuing to chase (next real test, not yet built: encode every frame from
`CaptureUVC_Thread` unconditionally, discarding the result, entirely independent of
`uvc_streaming`/USBX -- isolates JPEG-HW-core+CPU activity from actual USB OTG DMA transfer
activity, which #21's test could not distinguish).

Part 2c stays restored (`YUV422` hardware pipeline) regardless of this test's outcome -- it is a
legitimate CPU-time win either way and #22 didn't find any evidence against it, only that it
wasn't sufficient on its own to explain the remaining oscillation.

---

## 2026-09-28 (#21) — Found the oscillation correlates with active JPEG encode/USB streaming, not with time or scene; restored Part 2c (hardware YUV422) to shrink JPG_Encode()'s per-frame busy window as a targeted fix

**New log, much more informative** (includes a `[UVC] Stream OFF (alt=0)` / re-`Stream ON`
cycle mid-capture): `isp_gain`/`isp_exposure` swing across almost the FULL range while streaming
is active -- not just the clean 0/232 alternation seen before, but `exposure` sweeping down to
`1` (minimum) and back up to `3522` (maximum) within a few samples -- then the instant
`[UVC] Stream OFF (alt=0)` appears, both values freeze COMPLETELY: 16 consecutive
`[UVC_CAP]` samples (frames=174 through 590, ~1 minute of real time) all read exactly
`isp_gain=232 isp_exposure=3522`, not one LSB of drift, while `usb_irq` is also frozen (no USB
activity). The instant streaming resumes (`Stream ON alt=1` / `transmission_start OK` near the
end of the log), the pattern of change resumes too. This is a clean, repeated, unambiguous
correlation: **the oscillation exists only while USB is actively pulling video, not as a function
of elapsed time or a fixed scene property** -- ruling out "scene is just hard to meter" as the
(sole) explanation and pointing at something in the active streaming path itself.

**Traced the mechanism**: `ux_device_video.c`'s `fill_uvc_payload()` calls `JPG_Encode()`
synchronously, once per JPEG frame, directly inside `USBD_VIDEO_StreamPayloadDone()` -- which
USBX invokes from its own internal `_ux_device_class_video_write_thread_entry` (created at
`UX_THREAD_PRIORITY_CLASS` = 20, `ux_port.h`) every time an isochronous IN payload finishes. With
Part 2c reverted (since #17), this log's own `[JPG] enc=... t=24-28ms (cvt=18-22us... hal=5ms)`
lines show each encode blocking that thread for ~24-28ms, dominated by the software
`CVT_FormatRgb565ToYuv422Jpeg()` conversion (~18-22ms) -- happening roughly once per frame, i.e.
~16 times/sec, so up to ~35-45% of all CPU/bus time goes through this one blocking call whenever
streaming is active. The exact mechanism connecting that busy window to AEC's own convergence
(ThreadX preemption timing, DCMIPP/JPEG-HW/USB-OTG AXI bus contention delaying the ISP's own
statistics-extraction hardware, or something else) isn't nailed down with certainty from source
alone -- but the correlation itself (frozen the instant encoding stops, disturbed the instant it
resumes) is airtight from this log, regardless of which exact mechanism it is.

**Important scope note on #17's earlier test**: #17 reverted Part 2c and the user reported
"vẫn y hệt" (no change), which we read at the time as clearing 2c entirely. That test is now
understood to have been CONFOUNDED: Part 1's dynamic frame length was still fully active at that
point (only disabled two steps later, in #18) and was almost certainly the dominant source of
oscillation in that specific test, masking any smaller effect from 2c's encode-duration
difference. #17's conclusion ("2c doesn't matter") was reasonable given what was known then, but
wasn't actually a clean test of 2c on its own.

**Fix, restoring Part 2c** (reverting #17's revert): `Appli/Core/Src/main.c`
(`MX_DCMIPP_Init()`) -- `PixelPackerFormat` back to `DCMIPP_PIXEL_PACKER_FORMAT_YUV422_1`,
`HAL_DCMIPP_PIPE_SetYUVConversionConfig()`/`HAL_DCMIPP_PIPE_EnableYUVConversion()` re-added
(BT.601 matrix, same as before, ordering after `SetConfig()` preserved). `Appli/Core/Src/app_threadx.c`
-- `jpg_conf.fmt_src` back to `JPG_SRC_YUV422`, using the cheap `CVT_FormatYuv422ToYuv422Jpeg()`
byte-reorder path (no RGB->YUV math) instead of the ~18-22ms software conversion, cutting
`JPG_Encode()`'s blocking window down toward the ~5ms HW-JPEG-core floor.

Build clean, RAM 64.64% (unaffected -- `mcu_buffer` size is independent of source pixel format,
consistent with every previous round touching this).

**Next steps**: flash and retest, specifically watching `isp_gain`/`isp_exposure` WHILE actively
streaming (not just at boot) -- the key question is whether shrinking the encode window
eliminates the while-streaming oscillation or only reduces its magnitude/frequency. If it's not
fully gone, the busy-window theory is confirmed directionally but insufficient alone, and the
next step would be decoupling `JPG_Encode()` from the USBX video-write thread entirely (encode in
`CaptureUVC_Thread` instead, handing the USBX thread only a ready-made JPEG buffer to drip-feed)
so no matter how long encoding takes, it can no longer compete with whatever is disturbing AEC.

---

## 2026-09-28 (#20) — #19's fixed 1.5s settle delay wasn't long enough for a dark test scene; replaced with convergence-based waiting (poll until isp_gain/isp_exposure hold steady, capped by a timeout)

**Symptom**: user flashed #19 and pointed the camera at a dark ceiling with a bare light tube (a
deliberately high-contrast, mostly-dark scene) -- still saw flicker, and reported the image now
also looks darker. Log confirms: at `frames=16` (~1s in) and `frames=32` (~2s in), `isp_gain` is
still `0` (not yet ramped to what this scene needs); `[UVC] transmission_start OK` (USB actually
starts sending video to the viewer) happens in this same early window, so the viewer was still
watching AEC's gain=0->232 ramp live -- the exact thing #19 was meant to hide. Only by
`frames=48` (~3s in) does it reach `isp_gain=232, isp_exposure=3522` and hold there steadily for
the rest of the log (5 consecutive samples, no oscillation) -- so the underlying sustained-
oscillation bug (#15-#18) really is fixed; what's left is purely that #19's fixed 1.5s guess was
too short for this specific dark scene, and the "darker" look is this scene's own genuinely low
average brightness pinning gain/exposure at their hard ceiling (already the sensor's physical
limit at the current fixed frame length -- nothing left to give without sacrificing FPS further).

**Why a fixed duration can't be right**: how long AEC needs to reach the correct operating point
depends entirely on how far the real scene's target is from the cold-boot default
(`isp_gain=0`, `isp_exposure=1600`) -- a bright scene needs almost no ramp, a very dark one (like
this test) needs the full walk to the ceiling. No single constant fits both.

**Fix** (`Appli/Core/Src/app_threadx.c`, `CaptureUVC_Thread()`): replaced the fixed 1.5s loop
with a convergence poll -- keep pumping `ISP_BackgroundProcess()` (20ms steps) until
`isp_gain`/`isp_exposure` have both held exactly steady for `AEC_SETTLE_STABLE_STEPS=20`
consecutive steps (~400ms unchanged), which ordinary AEC dither is too small/fast to satisfy by
accident, so this reliably means "the boot ramp is over, not just between two dither samples".
Capped by `AEC_SETTLE_TIMEOUT_MS=4000` so a scene that never truly settles (e.g. an actually
flickering light source, or an AEC edge case) cannot delay stream start forever -- it streams
anyway past that point rather than hanging. Added a one-time
`[UVC_CAP] AEC settle: <ms>, gain=... exposure=...` printf so the actual convergence time (and
whether it timed out) is visible in the log on future tests, instead of guessing blind again.

Build clean, RAM 64.59% (unaffected -- still a boot-time-only loop, no new storage).

**Next steps**: flash and retest, including specifically with a dark/high-contrast scene like the
one that exposed #19's gap. Check the new `[UVC_CAP] AEC settle:` log line -- if it reports
"(timed out)" often, `AEC_SETTLE_TIMEOUT_MS`/`AEC_SETTLE_STABLE_STEPS` may need retuning; if
convergence time is consistently much less than 4s, the timeout could be tightened to reduce
worst-case boot latency. Separately: the "darker" image in a scene like the ceiling/light-tube
test is expected given the sensor is already at its gain/exposure ceiling for the current fixed
frame length -- not a new bug, and not further fixable without either accepting more noise
(pushing analog gain past its documented ceiling, already ruled out in Bug 22) or trading more
FPS for a longer frame length (more exposure headroom).

---

## 2026-09-28 (#19) — Compared against ST's official STM32N6_Face_Recognition (IMX335, stm32-mw-camera) to answer: why 16fps here vs 30fps there, and why that project doesn't run out of RAM at high resolution; added an AEC settle delay for the remaining ~1-2s boot flicker

**Context**: user confirmed #18's fix worked (flicker gone except ~1-2s at boot), then asked two
architecture questions, initially pointing at `STM32N6_Face_Detection` (a bare FSBL/NPU/AXISRAM
bring-up skeleton with NO camera code at all, confirmed by its own README and a repo-wide grep --
not a valid comparison target). User redirected to `STM32N6_Face_Recognition`
(`Application/STM32N6570-DK/`), ST's official face-recognition demo, which does have a full
camera pipeline (`Middlewares/stm32-mw-camera`, IMX335).

**Why IMX335 hits 30fps and IMX219 was stuck at 16fps here**: NOT a sensor capability gap --
this project's own earlier hardware testing already proved IMX219 hits ~31fps at
`FRM_LENGTH_LINES=1763` (matches IMX335's 30fps almost exactly); 16fps is only what #18's fix
pins for guaranteed low-light exposure headroom. The real lesson is architectural, confirmed by
reading ST's own sensor driver source
(`Middlewares/stm32-mw-camera/sensors/imx335/imx335.c:195-218,719-753`): it ships five
pre-validated register tables (`framerate_10/15/20/25/30fps_regs`, each writing VMAX registers
0x3030/0x3031 -- IMX335's exact equivalent of IMX219's FRM_LENGTH_LINES). FPS is a MODE selected
ONCE via `IMX335_SetFrameRate()` before streaming starts (`app_camerapipeline.c:122`,
`CAMERA_FPS 30`) -- there is no code path anywhere in this reference project that changes VMAX
while AEC is actively running. This matches Raspberry Pi/libcamera's real design too (a fixed
per-mode frame duration; AGC only ever trades exposure/gain within it). This project's entire
flicker saga (original Part 1 continuous recompute, #15's hysteresis, #16's slew-limiting) was
fighting this same lesson from three different angles: evision's precompiled AEC was never
designed to tolerate its own frame timing changing underneath it while running, no matter how
gently. #18's fix (pin frame length, never touch it live) isn't a workaround -- it's the
architecturally correct thing, confirmed by how ST's own reference sensor driver does it.
**Implication for later, if 30fps in normal light is wanted back**: implement FAST/SLOW as two
fixed profiles chosen ONCE (e.g. at boot from an initial light read, or by explicit user trigger),
never by AEC mid-session -- accept that a mode switch is a discrete event (brief re-init), not a
seamless live adjustment.

**Why that project doesn't run out of RAM at high resolution**: two independent, compounding
reasons, both confirmed from source, not assumed:
1. **Different pipeline shape.** `app_camerapipeline.c`'s `DCMIPP_PipeInitDisplay()` /
   `DCMIPP_PipeInitNn()` configure DCMIPP's two hardware output pipes to downscale directly from
   the sensor into (a) an LCD-display-sized RGB565 buffer and (b) a tiny AI-model-input-sized
   buffer (`STAI_NETWORK_IN_1_WIDTH/HEIGHT`, typically ~128-256px) -- there is no software JPEG
   encoder, no MCU-block staging buffer, and no full-resolution frame ever held in RAM. It's a
   local-display + on-device-inference demo, not a USB UVC webcam -- it never needs to produce a
   full-resolution encoded frame at all, which is the entire source of this project's RAM cost
   (`video_buf1`, `mcu_buffer`). Not a fairer/smarter design for our use case, just a different
   task with a genuinely smaller memory requirement.
2. **Different board hardware.** `Application/STM32N6570-DK/STM32CubeIDE/STM32N657xx.ld`:
   `PSRAM (xrw): ORIGIN = 0x91000000, LENGTH = 16M` -- the STM32N6570-DK Discovery Kit has an
   onboard 32MB APS256XX PSRAM chip (16MB of it mapped/used here) on top of ~1MB+ of internal
   AXISRAM. This project's own existing comments already established the NUCLEO-N65X0Q-ISP board
   has **no PSRAM at all** (`app_jpg.c:45`, `app_threadx.c:24`: "no PSRAM on target board") --
   ~2MB internal SRAM total is genuinely everything available here, vs. 16MB+ extra external
   memory on that board. Even if this project needed to buffer full frames the way it does now,
   that board could absorb it trivially; this one cannot.

**Residual boot flicker (~1-2s)**: with frame length now fixed throughout streaming (#18), this
is no longer the same bug -- almost certainly just AEC's normal convergence transient from its
cold-boot defaults (`isp_gain=0`, `isp_exposure=1600`, `main.c`) to whatever the real room needs,
which every camera app shows briefly when first opened; the only reason it was VISIBLE here is
that USB started transmitting frames to the viewer immediately, before AEC had a chance to
converge somewhere nobody's watching. Fix (`Appli/Core/Src/app_threadx.c`,
`CaptureUVC_Thread()`): added a ~1.5s loop pumping `ISP_BackgroundProcess()` (20ms steps) right
before `uvc_capture_active = 1U` -- delays USB stream start by ~1.5s but should let AEC settle
before any frame reaches the viewer. The 1.5s figure is a guess (no way to directly observe
convergence progress from outside); adjust if boot flicker persists or the extra wait feels too
long.

Build clean, RAM 64.58% (unaffected -- this is a boot-time delay loop, no new storage).

**Next steps**: flash and confirm the boot flicker is gone (or reduced) and note whether ~1.5s
feels right. Decide separately (no code changed for this yet) whether the FAST/SLOW fixed-profile
mode-switch idea above is worth implementing to recover ~31fps in bright rooms, given it can only
ever be a discrete/explicit switch, never a live AEC-driven one.

---

## 2026-09-28 (#18) — Part 2c reverted but flicker/darkness identical (confirmed by user); isolating Part 1 (dynamic frame length) next by disabling it entirely via a debug gate

**Result of #17's isolation test**: user flashed the Part 2c revert (RGB565 + no hardware YUV
conversion) and reported "vẫn y hệt" -- still exactly the same symptom. This formally CLEARS
Part 2c (the DCMIPP hardware YUV422 color-conversion pipeline) as a cause -- it was not disturbing
AEC/AWB metering after all, so its "upstream, should be independent" comment was actually correct
this time (unlike Bug 23's case). No need to re-investigate 2c further; it can be safely
re-applied once the real cause is found and confirmed unrelated.

**Narrowing further**: this leaves Part 1 (dynamic frame length + #15's hysteresis) and Part 2a
(hardware crop to 640x360) as the only remaining changes from this round that could explain the
flicker. Rather than guess again, isolate Part 1 directly: added a debug gate
`DEBUG_DISABLE_DYNAMIC_FRAME_LENGTH` (`Appli/Core/Src/main.c`, set to `1`) that short-circuits
`SetSensorExposureHelper()` to skip ALL frame-length switching and hysteresis logic, permanently
pinning `FRM_LENGTH_LINES` at `IMX219_FRAME_LENGTH_LONG_MAX` (3526) -- i.e. behaving exactly like
the code did before Part 1 was ever introduced (fixed ~16fps ceiling, but that config was
confirmed stable with correct color/exposure convergence back in the Bug 20-23 testing rounds).
Exposure itself is still written every call via `IMX219_SetExposure()`, unchanged -- only the
frame-length dynamics are removed.

**This is a clean either/or test**:
1. Flicker/darkness disappears -> Part 1's frame-length dynamics is the real cause (even with
   #15's hysteresis debounce, something about writing `FRM_LENGTH_LINES` dynamically at all is
   still disturbing AEC). Would need a fundamentally different approach than hysteresis tuning --
   possibly accept the fixed ~16fps ceiling as the practical answer, since every attempt to make
   frame length dynamic (continuous recompute in the original Part 1, then hysteresis in #15) has
   made things worse or not fixed it.
2. Flicker/darkness persists identically -> Part 1 is cleared too, by elimination the cause must
   be Part 2a's hardware crop -- most likely `statAreaStatic`'s ROI (`isp_param_conf_imx219.h`:
   X0=16,Y0=16,XSize=320,YSize=240) sampling a different physical region than intended, since
   `ISP_SVC_ISP_SetStatArea()` (`Appli/ISP_MW/isp/Src/isp_services.c:689`) programs this ROI onto
   DCMIPP_PIPE1's own hardware statistic-extraction block (`HAL_DCMIPP_PIPE_SetISPAreaStatisticExtractionConfig`)
   -- the SAME pipe Part 2a's crop was added to. Whether that hardware stat-extraction tap sits
   before or after the crop stage inside PIPE1 is not yet confirmed from source; if after, Part
   2a's VStart=60 crop would shift what the ROI actually samples by 60 rows without any code
   realizing it.

Build clean, RAM 64.58% (unaffected, debug-gate is logic-only).

**Next steps**: flash and retest with this gate. Report back which of the two outcomes above
occurred so the next fix can target the right part with confidence, instead of another guess.
Set `DEBUG_DISABLE_DYNAMIC_FRAME_LENGTH` back to `0` once this test's result is known and acted
on -- it is a temporary diagnostic, not a keeper.

**Result (confirmed by user)**: "oke đã ổn" -- flicker resolved with this gate on, EXCEPT for
~1-2 seconds of residual flicker right at boot/stream-start. This is outcome 1: Part 1's dynamic
frame-length switching (even with #15's hysteresis debounce) IS the real cause of the sustained
flicker. Part 2a's hardware crop is cleared -- `statAreaStatic`'s ROI is not the issue. The
residual boot-time flicker is a separate, much smaller-scope symptom (likely just AEC's normal
initial convergence transient before it settles, now that the sustained oscillation is gone) --
not yet investigated, tracked as a follow-up.

**Current state**: `DEBUG_DISABLE_DYNAMIC_FRAME_LENGTH` is left at `1` (Part 1 dynamic frame
length disabled, fixed ~16fps ceiling) since this is the confirmed-working state. Re-enabling
Part 1 needs a fundamentally different approach than continuous recompute (original) or
hysteresis (#15) -- both made AEC's convergence worse -- so it is parked, not abandoned.

---

## 2026-09-28 (#17) — Slew-limit fix (#16) made image measurably darker on real hardware without fixing flicker; reverted it + reverted Part 2c's YUV422 hardware pipeline as an isolation test

**Symptom after flashing #16**: user reports the image is now noticeably darker than before AND
still flickers (two screenshots: dim, low-contrast, grayish). This is a real regression -- #16's
slew-limiting logic itself is confirmed WRONG or at least net-harmful, not just insufficient.

**Why #16 likely backfired**: most plausible explanation is that AEC's own desired target keeps
swinging between the two extremes FASTER than the chosen ramp (`GAIN_SLEW_MAX_STEP=24`,
`EXPOSURE_SLEW_MAX_STEP=400` per call) could track -- so instead of converging, `isp_gain`
perpetually wobbles in a low-to-mid band that is too dark for the actual scene, while still
showing visible wobble (residual flicker). This means #16's underlying theory (pure actuator-side
limit-cycling from sitting at the analog-gain saturation boundary) is, at best, an incomplete
explanation -- slew-limiting the actuator cannot fix a setpoint that itself is being computed
from something more fundamentally wrong. Per this project's own lesson (do not conclude root
cause from log analysis alone without hardware confirmation, and diff against the last known-good
state rather than stacking new theories on unconfirmed ones): reverted #16's slew-limiting in
`SetSensorGainHelper()`/`SetSensorExposureHelper()` back to direct assignment (kept #15's frame-
length hysteresis, which is not implicated by this regression).

**New suspect, chosen for isolation testing**: Part 2c (the DCMIPP hardware YUV422 color-
conversion pipeline, introduced in the SAME round as Part 1's dynamic frame length -- i.e. the
flicker was never observed with Part 2c absent, only ever after both landed together). That
block's own comment asserted the AEC statistics tap (`stats.down`) sits upstream of the YUV-
conversion/pixel-packer stage, so it "should be" unaffected -- but Bug 23 already proved once
that this kind of "should be independent" assumption about this pipeline can be wrong, and it was
never actually verified on hardware in isolation (Part 1 and Part 2 were flashed and tested
together, skipping the plan's own recommended per-part isolation testing).

**Change** (isolation test, not a confirmed fix):
- `Appli/Core/Src/main.c` (`MX_DCMIPP_Init()`): reverted `PixelPackerFormat` back to
  `DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1`; removed the `HAL_DCMIPP_PIPE_SetYUVConversionConfig()` /
  `HAL_DCMIPP_PIPE_EnableYUVConversion()` calls entirely.
- `Appli/Core/Src/app_threadx.c`: `jpg_conf.fmt_src` reverted `JPG_SRC_YUV422` -> `JPG_SRC_RGB565`
  (back to the software `CVT_FormatRgb565ToYuv422Jpeg()` path, ~18-20ms/frame instead of ~5ms --
  a real CPU cost, accepted temporarily for debugging).
- Part 2a (hardware crop to 640x360) and Part 2b (USBX pool shrink) are UNCHANGED and NOT part of
  this isolation test -- they don't touch the pixel/statistics path the way 2c does.
- Build: RAM 64.59% (was 64.64% with #16's slew code; mcu_buffer is unaffected by source format
  either way, so no meaningful RAM change expected or seen).

**Next steps**: flash and retest. Two outcomes:
1. Flicker/darkness resolves with 2c reverted -> Part 2c's hardware YUV-conversion stage IS
   disturbing AEC/AWB metering despite the "upstream, should be independent" assumption; do not
   re-apply it without first finding out why (candidate: the BT.601 matrix coefficients affecting
   whatever the stats block actually taps, or a timing/latency change from the extra pipeline
   stage). FPS/CPU win from Part 2c would need to wait until that's understood.
2. Flicker/darkness persists identically with 2c reverted -> rules out 2c entirely; the
   instability is really in the frame-length dynamics (Part 1) or gain-saturation/metering theory
   from #15/#16, or possibly `statAreaStatic`'s ROI (`isp_param_conf_imx219.h`: X0=16,Y0=16,
   XSize=320,YSize=240) now sampling a different physical region than intended after Part 2a's
   hardware crop shifted PIPE1's coordinate origin -- worth checking whether the ISP statistics
   block taps before or after the crop stage next.

---

## 2026-09-28 (#16) — Frame-length hysteresis (#15) did not fully fix the flicker; real cause is AEC limit-cycling at the analog-gain ceiling in normal light; fixed with slew-rate limiting on gain/exposure writes (same technique as Raspberry Pi/libcamera's AGC)

**Symptom**: after flashing #15's frame-length hysteresis fix, user still saw flicker in a new
video + `[UVC_CAP]` log. Crucially, user reported: covering the lens (true darkness) does NOT
flicker -- image pins stably at max gain/exposure -- but uncovering it (normal room light)
flickers again.

**Log analysis**: `isp_gain` alternates cleanly between `0` and `232` (the analog-gain ceiling,
Bug 22) every few AEC updates (roughly every 1-3 seconds), and every single time `isp_gain=232`
it is paired with `isp_exposure=3522` (the exposure ceiling) with zero exceptions across the
whole log. When `isp_gain=0`, `isp_exposure` varies (2690-3522) but is often still high. This
ruled out #15's frame-length coupling as the (sole) cause -- frame length only changes on a
60-call debounce, far less often than this gain toggling -- and pointed at a genuine AEC
control-loop instability.

**Root cause**: `ispGainStatic` (`isp_param_conf_imx219.h`, WB-ratio-only since Bug 23:
R=150000000, G=100000000, B=140000000 in ISP's fixed-point gain units) weights G noticeably
lower than R/B. `isp_algo.c` confirms AEC's brightness metric is `stats.down.averageL`, measured
AFTER this gain is applied (not before). Because G carries the largest luma weight, this
under-weights the metered brightness relative to the true scene -- AEC believes the scene is
dimmer than it is, so its correct operating point in NORMAL light sits right at/near the
analog-gain ceiling, not comfortably below it. A closed-loop controller with no slew/rate
limiting that operates right at a saturation boundary classically limit-cycles: full step to the
ceiling, overshoot past target once there, full step back down, repeat -- exactly the clean
0<->232 alternation seen in the log. In TRUE darkness (lens covered) there is no ambiguity --
max gain/exposure is simply the correct answer -- so it pins stably with no cycling, matching
what the user observed.

We cannot patch the precompiled AEC (`libn6-evision-st-ae_gcc.a`), so the fix is the same one
Raspberry Pi/libcamera's AGC uses to avoid this exact failure mode (its "speed" parameter) --
this is also the concrete answer to the user's earlier question of why the same IMX219 doesn't
flicker on RPi5: its AGC smooths/slews gain and exposure changes instead of applying the full
AEC-requested delta in one step.

**Fix** (`Appli/Core/Src/main.c`): in `SetSensorGainHelper()` and `SetSensorExposureHelper()`,
clamp the per-call change to `isp_gain`/`isp_exposure` to `GAIN_SLEW_MAX_STEP=24` /
`EXPOSURE_SLEW_MAX_STEP=400` respectively (full range ramps in ~9-10 calls) before writing to
the sensor. `GetSensorGainHelper()`/`GetSensorExposureHelper()` already read back
`isp_gain`/`isp_exposure`, which now hold the SLEWED (actually-applied) value rather than the
raw AEC request -- this is essential: if AEC's internal state tracking believed its full request
had already landed (when only a fraction did), the clamp would not damp its control loop at all.
The `Exposure` parameter is overwritten with the slewed value before #15's
`short_is_enough`/frame-length logic runs, so that logic (unchanged) now hysteresis-switches
based on the smoothed exposure trajectory, not the raw AEC request.

**Next steps**: flash and re-check the video for flicker. Expect gain/exposure to ramp smoothly
over roughly half a second instead of jumping instantly between extremes -- real exposure
changes (e.g. turning off a light) should still be visible within ~1s, just smoothed, not
snapped. If flicker is reduced but not fully gone, `GAIN_SLEW_MAX_STEP`/`EXPOSURE_SLEW_MAX_STEP`
can be lowered further (slower ramp, more damping) at the cost of slower real response to
lighting changes. If flicker persists completely unchanged, log `buf_idx` alongside
`isp_gain`/`isp_exposure` next, per #15's own fallback note, to rule out the Part 2c YUV422
hardware pipeline / double-buffer asymmetry as a contributing factor instead.

---

## 2026-09-28 (#15) — Dynamic frame length (Part 1) caused AEC to oscillate (visible as flickering brightness in a user-recorded video); fixed with a coarse 2-state switch + shrink debounce instead of continuous recomputation

**Symptom**: after flashing the Part 1/2 changes, the user recorded a video showing visible
brightness flickering, and the log confirmed it precisely: `isp_gain`/`isp_exposure` alternated
**every single ~1s poll** between a mid-range value (`gain=0, exposure~2259-2378`, naturally
varying a little) and the hard ceiling (`gain=232, exposure=3522`, byte-identical every time) --
a clean 2-state limit cycle, not normal AEC dithering. `[UVC_CAP] fps` alternated 17/18-19 in
lockstep, confirming the sensor's actual `FRM_LENGTH_LINES` was flipping between two different
values in sync with the exposure oscillation.

**Root cause**: the first version of `SetSensorExposureHelper()` recomputed `needed_frame_length
= Exposure + 4` and wrote it on *every single call*, in whichever direction changed. Before this
session's Part 1, AEC was confirmed *stable* in dark conditions (held `gain=232`/`exposure=3522`
steady across dozens of consecutive polls, no oscillation) -- introducing a `FRM_LENGTH_LINES`
write into the same loop AEC's own convergence depends on is what broke that stability. Most
likely mechanism: a frame-length change takes at least one frame to fully settle the sensor's
internal timing, and `evision`'s AEC (a precompiled library, its control-loop internals not
inspectable) evidently assumes only exposure/gain change between its own updates -- an
unannounced frame-length change on top violates that assumption and made its simple control loop
overshoot in both directions instead of converging.

**Fix**: replaced the continuously-recomputed frame length with a coarse 2-state switch
(`SetSensorExposureHelper()`, `main.c`):
- Only ever `IMX219_FRAME_LENGTH_SHORT` (1763) or `IMX219_FRAME_LENGTH_LONG_MAX` (3526) -- never
  an intermediate value tied to the exact current exposure. Switching UP still happens
  immediately when exposure needs more room than SHORT allows (correctness requires this -- can't
  delay without risking a genuinely underexposed frame), but always to the single LONG_MAX value,
  so growing itself cannot oscillate between two different "long" values the way the original
  version did.
- Switching back DOWN to SHORT only happens after exposure has comfortably fit SHORT (with a
  100-line margin) for `FRAME_LENGTH_SHRINK_DEBOUNCE` (60) consecutive calls in a row -- a
  deliberate debounce so a single low reading during normal AEC dithering can't immediately yank
  the frame length back down, giving AEC's own loop time to actually settle between the rare
  frame-length transitions that do happen.
- This is exactly the hysteresis the original plan anticipated might be needed ("KHÔNG thêm
  hysteresis ở lần đầu... chỉ thêm nếu quan sát thấy FPS/exposure nhấp nháy") -- now confirmed
  necessary by the observed oscillation, not added speculatively.

**Also fixed in the same pass**: `GetSensorInfoHelper()`'s `Info->width/height` still said
640x480 -- stale from before Part 2a's hardware crop (main.c's `MX_DCMIPP_Init()` now crops PIPE1
to 640x360 directly). Updated to 640x360 for consistency; not confirmed to be related to the
oscillation, but a real inconsistency worth closing while in this function.

Rebuilt clean (64.63% RAM, unchanged -- logic-only change, no new static allocation). **Not yet
tested on real hardware as of this entry.**

### Next steps

1. **Flash and re-check for flicker** -- both visually and via consecutive `isp_gain=.../
   isp_exposure=...` log lines. Expect long, stable runs at a single value (like the pre-Part-1
   dark-room logs), with at most an occasional deliberate SHORT<->LONG_MAX transition when
   lighting genuinely changes, not per-poll alternation.
2. **If oscillation persists even with this fix**: that would point away from the frame-length
   coupling theory and toward something else introduced in the same batch of changes -- most
   likely the Part 2c YUV-conversion/crop hardware pipeline somehow producing a real
   frame-to-frame brightness difference AEC is correctly reacting to (e.g. an asymmetry between
   the two ping-pong buffers). Next diagnostic in that case: log which `buf_idx`
   (`VIDEO_GetReadyBufferIdx()`) corresponds to which `isp_gain`/`isp_exposure` reading, to check
   whether the oscillation correlates with which physical buffer was just captured.
3. If shrink debounce (60 calls, roughly ~1-2s depending on actual fps) feels too slow/fast once
   seen on real hardware, it's a single `#define` (`FRAME_LENGTH_SHRINK_DEBOUNCE`) to tune -- not
   a structural change.

---

## 2026-09-28 (latest #14) — Post-Bug-23 architecture work: dynamic frame length (restores ~31fps in bright light) + RAM optimization via DCMIPP hardware crop and right-sized USBX pool (74.23% -> 64.63%) + JPEG pipeline switched to DCMIPP's own hardware YUV conversion

**User pushed back on treating the FPS drop as an unavoidable exposure-headroom tradeoff**: this
same IMX219 module runs fine on Raspberry Pi without this cost. Correct observation -- RPi's
driver (like any real camera ISP) uses **dynamic frame length**: it only stretches
`FRM_LENGTH_LINES` (and pays the FPS cost) when a scene actually needs a longer exposure than the
fast default allows, then shrinks back down once it doesn't. This project's Bug 18/21 fix instead
pinned `FRM_LENGTH_LINES` at the long value (3526) *permanently*, paying the ~16fps cost even in
bright light where `isp_exposure=1` -- confirmed directly in this session's own logs. Planned and
implemented (plan file: `~/.claude/plans/robust-beaming-pine.md`) this fix plus two RAM
optimizations and a JPEG pipeline change, approved by the user before implementation.

### Part 1 — Dynamic frame length

- `imx219.h`: added `IMX219_FRAME_LENGTH_SHORT` (1763, confirmed ~31fps on this hardware) and
  `IMX219_FRAME_LENGTH_LONG_MAX` (3526, confirmed ~16fps, the low-light ceiling), and
  `IMX219_SetFrameLength()` declaration.
- `imx219.c`: added `IMX219_SetFrameLength()` (same pattern as `IMX219_SetExposure()`). Changed
  `imx219_common_regs`'s init table to boot at the SHORT value (was long, from Bug 18) --
  the sensor should start fast and only slow down when AEC actually needs it. **Also removed a
  second, redundant frame-length write** (`IMX219_Configure640x480()`'s old "step 4", which used
  to independently duplicate this same register -- exactly the kind of duplicate-source-of-truth
  bug Bug 21 already found once this session; better to delete the second copy than keep both in
  sync by hand going forward).
- `main.c`: added `static uint16_t current_frame_length` (tracks what the sensor is actually
  running at). `SetSensorExposureHelper()` now computes `needed = max(SHORT, Exposure + 4)` and
  grows `FRM_LENGTH_LINES` (via `IMX219_SetFrameLength()`) *before* writing a longer exposure, or
  shrinks it *after* writing a shorter one -- never leaving the sensor in a state where
  `COARSE_INTEGRATION_TIME > FRM_LENGTH_LINES`. `GetSensorInfoHelper()`'s `exposure_max` stays at
  the long ceiling (3522) unchanged -- AEC can still ask for a long exposure any time, the frame
  length just grows to match instead of being fixed there always.

### Part 2 — RAM optimization (no resolution change, per user's explicit choice)

**2a. DCMIPP hardware crop** (`main.c`'s `MX_DCMIPP_Init()`): added
`HAL_DCMIPP_PIPE_SetCropConfig()` + `EnableCrop()` for PIPE1 (`VStart=60, HStart=0, VSize=360,
HSize=640`) -- DCMIPP now captures only the 640x360 region actually streamed, instead of the full
640x480 that `ux_device_video.c` immediately discarded 120 rows of via a manual `+60*640*2`
pointer offset. That offset is now removed (the buffer already IS the cropped region).
`FRAME_HEIGHT` (`main.c`) and `VIDEO_BUF_HEIGHT` (`app_threadx.c`) both changed 480 -> 360 to
match. **Measured: `video_buf1` 614400B -> 460800B, RAM 74.23% -> 66.97%** (~150KB freed, matches
the plan's estimate almost exactly).

**2b. Right-sized USBX memory pool** (`app_azure_rtos_config.h`): `UX_APP_MEM_POOL_SIZE` 192KB ->
144KB. Verified by reading the code first (not guessed): the only two allocations ever drawn from
`ux_app_byte_pool` are `USBX_MEMORY_STACK_SIZE` (128KB, `app_usbx.h`) and
`UX_DEVICE_APP_THREAD_STACK_SIZE` (4KB, `app_usbx_device.h`) = 132KB real usage against a 192KB
backing array. **Measured: RAM 66.97% -> 64.63%** (~48KB freed, matches estimate).

**Combined**: RAM usage **74.23% -> 64.63%**, ~197KB freed, with the streamed resolution
unchanged (640x360) -- banked for future use rather than spent on resolution now, per the user's
explicit choice when this was planned.

### Part 2c — JPEG pipeline switched to DCMIPP's own hardware YUV conversion (CPU/FPS, not RAM)

Corrected an overstated claim from the entry before Bug 23: this does NOT eliminate `mcu_buffer`
(the JPEG HW encoder needs MCU-block-ordered input regardless of source pixel format, so a staging
buffer is unavoidable either way). What it actually saves is CPU time: the log's own `cvt=`
timing showed ~18-20ms/frame for the software `RGB565 -> YUV422 + MCU-block` conversion
(`CVT_FormatRgb565ToYuv422Jpeg`/`CVT_CvtRgb565ToMcu422` in `app_cvt.c`) versus ~5ms for the actual
hardware JPEG encode (`HAL_JPEG_Encode`) -- the software conversion was 75-80% of total per-frame
time. `CVT_CvtYuv422ToMcu422` (used when the source is already YUV422) does pure byte reordering,
no per-pixel color math -- confirmed by reading `app_cvt.c` directly, not assumed.

- `main.c` (`MX_DCMIPP_Init()`): added `HAL_DCMIPP_PIPE_SetYUVConversionConfig()` +
  `EnableYUVConversion()` for PIPE1, using `../Camera_N6_AI_Test`'s confirmed-working BT.601
  matrix coefficients verbatim. Changed `pPipeConfig.PixelPackerFormat` from
  `DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1` to `..._YUV422_1`. Ordering matters and was followed
  exactly per the reference project's own comment: `SetConfig()` (packer) first, then
  `EnableYUVConversion()` after -- `SetConfig()` resets the P1CCCR register the YUV block also
  uses.
- `app_threadx.c`: `JPG_Init()`'s `fmt_src` changed from `JPG_SRC_RGB565` to `JPG_SRC_YUV422` --
  the cheaper path already existed in `app_jpg.c`/`app_cvt.c`, just wasn't wired up.
- **Explicitly flagged as needing hardware verification, not assumed safe**: the YUV-conversion
  block sits downstream of ISP_MW/evision's demosaic/AWB/AE stages and its own statistics tap
  (`stats.down`, see Bug 23), so it *should* be invisible to AEC/AWB convergence -- but Bug 23 was
  exactly this kind of "should be independent, wasn't" surprise, so this must be confirmed with
  real `isp_gain=.../isp_exposure=...` numbers on hardware, not assumed correct because the theory
  sounds right.

Rebuilt clean after every sub-step (main.c, app_threadx.c, imx219.c, ux_device_video.c,
app_azure_rtos_config.h) -- final combined build: **64.63% RAM**, no errors (pre-existing
`%u`/`ULONG` printf format warnings in `app_usbx_device.c` are unrelated pre-existing noise, not
new). **Not yet tested on real hardware as of this entry** -- this is a substantial, multi-part
change; testing each part somewhat independently (per the plan's verification section) is
important before trusting all of it at once.

### Next steps

1. **Flash and test Part 1 first conceptually** even though all parts are in one build: bright
   room should show `isp_exposure` low and FPS back near ~30; a genuinely dark room should show
   FPS drop to ~16 only then, not always.
2. **Confirm Part 2a's crop is correct**: image should show the same 640x360 field of view as
   before (no vertical shift/stretch), not a different crop position.
3. **Confirm Part 2c didn't disturb AEC/AWB**: read `isp_gain=.../isp_exposure=...` across both
   lighting conditions again -- should still converge to sensible values, not pin at either
   extreme the way Bug 22/23 did.
4. Compare `[JPG] enc=...cvt=...hal=...` before/after -- `cvt` should have dropped substantially.
5. If frame length oscillates visibly (FPS flickering) when a scene is borderline: revisit the
   plan's noted possibility of adding hysteresis to the grow/shrink logic in
   `SetSensorExposureHelper()` -- deliberately not added on the first pass to keep the initial
   implementation simple and correct.

---

## 2026-09-28 (latest #13) — Bug 23: Bug 22's digital gain boost was WRONG and made things worse -- it lies to AEC's own brightness metering, since AEC measures luminance on data the boost has already been applied to; reverted to plain white-balance-only gains

**Tested Bug 22's fix under bright room lighting (user's own next step from that entry, done
faster than expected -- good)**: `isp_gain=0 isp_exposure=1` -- both pinned at the *minimum* this
time (exact opposite of the previous dim-room test's both-pinned-at-*maximum*). That direction is
correct given more light. But the displayed image got **worse**, not better: a completely flat,
textureless dark purple/brown frame with no visible detail at all -- worse than the previous
"dark but outlines visible" result.

**Root cause, traced into `isp_algo.c`**: the AEC algorithm's brightness metric
(`avgL = stats.down.averageL;`, ~line 447) is computed from statistics gathered on pixel data that
**already has `ispGainStatic`'s multiplication applied** -- confirmed by a nearby computation
(~line 568) that goes the other direction, `up.averageR = down.averageR * FACTOR /
ISPGain.ispGainR`, explicitly *dividing out* the ISP gain to estimate a pre-gain value from a
post-gain one. That only makes sense if `stats.down` (what AEC reads) is measured *after*
`ispGainStatic` is applied.

**This means Bug 22's fix was based on a wrong mental model.** Boosting `ispGainStatic` doesn't
add brightness that's otherwise unreachable -- it makes the *scene AEC measures* look brighter
than the sensor is actually capturing, without changing what the sensor really receives. AEC,
doing its job correctly against a lying meter, drove real exposure/sensor-gain down to compensate
for a "brightness" that was actually just this static digital multiplier -- with real exposure
crushed to `1` (the minimum), the sensor is capturing almost no real signal at all, and the same
digital gain that fooled the metering then amplifies that near-black noise floor into a flat,
detail-less mid-tone instead of an actual image. The two dim-room and bright-room tests are
consistent with the same underlying mechanism: this static gain sits *inside* the AEC feedback
loop it wasn't designed to fight, in both directions.

**Fix**: `ispGainStatic` reverted from `300000000/200000000/280000000` (3.0x/2.0x/2.8x, Bug 22's
boost) back to `150000000/100000000/140000000` (1.5x/1.0x/1.4x, the original white-balance-only
ratio from three entries back) -- no added magnitude, just the channel-to-channel ratio for color
balance. Letting AEC's own exposure/sensor-gain control loop be the *only* thing that determines
brightness, since that's the loop this ISP middleware actually meters against; a static digital
multiplier ahead of that meter can only distort it, not help it. Rebuilt clean (74.22% RAM,
unchanged). **Not yet tested on real hardware as of this entry.**

**Lesson**: this is the second brightness "fix" this session (after Bug 21's exposure_max/isp_
exposure sync) that needed a real understanding of *where in the pipeline* a value is measured,
not just "which knob affects brightness." Before adjusting any more ISP gain/exposure parameter,
check where the relevant statistics are actually sampled from (`isp_algo.c`/`isp_services.c`'s
`stats.up`/`stats.down` distinction) rather than assuming a knob's effect is independent of the
control loop reading it.

### Next steps

1. **Flash and check both lighting conditions again** (bright room, and whatever the earlier
   "dark but exposure/gain pinned at max" room was) with plain white-balance-only gain. Read the
   new `isp_gain=... isp_exposure=...` numbers in both cases -- expect them to land somewhere
   in the *middle* of their respective ranges now, not pinned at either extreme, if AEC is finally
   converging against a truthful signal.
2. If brightness is now reasonable in bright light but the original dim room is still maxed out
   and dark: that's a genuine "not enough light for this sensor at this config" situation (see the
   entry before Bug 22 for the noise/graininess caveat on any further digital push), not a bug to
   keep chasing in software.
3. Once exposure is confirmed properly responsive to actual lighting: only then revisit whether
   the white-balance *ratio* itself (not magnitude) needs adjustment for accurate color, ideally
   compared against a known neutral subject in consistent lighting.

---

## 2026-09-28 (latest #12) — Bug 22: AEC is genuinely pinned at both ceilings (isp_gain=255, isp_exposure=3522) and the image is still dark -- gain_max was reporting past the sensor's real analog-gain limit, and a digital brightness boost was added since there's no sensor-side headroom left

**Real numbers from the new instrumentation, exactly as hoped**: `[UVC_CAP] ... isp_gain=255
isp_exposure=3522` -- both pinned at the absolute ceilings this project itself reports to `evision`
(`exposure_max=3522` from Bug 20/21, `gain_max=255` from `GetSensorInfoHelper`). AEC is not failing
to try; it has maxed out every control it was told it has, and the image is still dark. This
matches this entry's own predicted "scenario 1" from the previous entry.

**Found a real bug in the ceiling AEC was given**: `gain_max = 255` is past the IMX219's own
documented analog-gain range. `../Camera_N6_AI_Test`'s comment on the same register
(`ANALOG_GAIN`, `0x0157`): `Range: 0x00=1x ... 0xC0=4x ... 0xE0=8x ... 0xE8=16x(max)`. `0xE8 = 232`
is the sensor's real ceiling -- values `233-255` are out of spec and, per the hardware evidence
above (pinned at 255 with no further brightness gain), not doing anything useful once past `232`.
Fixed `GetSensorInfoHelper()`'s `gain_max` to `232`.

**This alone won't meaningfully fix the darkness** -- `232` and `255` are close enough that the
sensor was already operating near its true maximum gain either way; the real finding is that
**both the sensor's exposure ceiling and its analog gain ceiling are now genuinely maxed out, and
the scene is still dark**. There is no more headroom on the sensor side to give AEC. The only place
left to add brightness is a digital multiplier after capture: `ispGainStatic`'s per-channel gains
(currently used for white balance, see the entry before Bug 20/21) were doubled in overall
magnitude while preserving their R:G:B ratio (`R=1.5x/G=1.0x/B=1.4x` -> `R=3.0x/G=2.0x/B=2.8x`,
this field's documented ceiling is `x16`, so there's plenty of room left if this still isn't
enough).

Rebuilt clean (`isp_param_conf_imx219.h`, `main.c`; 74.22% RAM, unchanged -- constant-value
changes only). **Not yet tested on real hardware as of this entry.**

**Being upfront about the limits of what a config change can fix here**: if the test environment
is genuinely low-light, digital gain amplifies noise along with brightness -- there's a point past
which no amount of `ispGainStatic` tuning produces a clean image, only a brighter but grainier one.
IMX219 is not a particularly light-sensitive sensor. If this digital boost still isn't enough, the
honest next question is about the physical test environment (more ambient light, or accepting a
noisier image), not another round of ISP parameter guessing.

### Next steps

1. **Flash and check brightness + noise level.** If it's now usably bright but grainy/noisy,
   that's the expected tradeoff of digital gain in low light -- consider whether more ambient
   light is available in the test environment rather than pushing `ispGainStatic` even higher.
2. If still too dark: `ispGainStatic` can go up to `x16` (`1600000000`) per channel -- there's
   headroom to push further, but diminishing returns/noise should be expected well before that
   ceiling.
3. Once brightness is acceptable: return to white-balance/color-accuracy tuning proper (the
   R:G:B *ratio* in `ispGainStatic`, not just its overall magnitude) -- this has been riding along
   as a side effect of the brightness fix and hasn't been independently verified against real
   captured colors yet.

---

## 2026-09-28 (latest #11) — Bug 21's fix stabilized exposure (no more flash-then-crush) but the image is still dark; FPS halving (31->16) is a strong clue AEC is pinned near the exposure ceiling; instrumentation added to get real numbers instead of guessing further; the "STAT AREA: X0=0..." print mystery finally resolved (harmless print-ordering, not a real bug)

**Result after Bug 21's fix**: the flash-then-crush-to-black behavior is gone -- the image now
flashes briefly at boot then settles into a **stable** (not still degrading) but still clearly
underexposed dark image (user screenshot: very dark, but object outlines are visible, unlike
literal black). This is real progress -- exposure is no longer diverging -- but it's not yet a
usable image.

**A strong quantitative clue was sitting in the log and easy to miss**: `[UVC_CAP] fps=16 ...`
throughout the stable dark period, vs. `~31` in every earlier log before `AECAlgo` was enabled.
FPS very close to exactly *half* of normal is a strong signal that `COARSE_INTEGRATION_TIME` is
being driven up near (or the sensor's internal timing is stretching to accommodate an exposure
request near) the frame-length ceiling -- consistent with `evision`'s AEC maxing out exposure
trying, and failing, to reach its target brightness. If exposure is already pinned near the
maximum the sensor allows and the image is still dark, the remaining lever is analog gain, or the
scene genuinely needs more light than exposure time alone can provide, or gain isn't being driven
the way it should be.

**Resolved the "STAT AREA: X0=0 Y0=0 XSIZE=0 YSIZE=0" mystery that's been sitting unexplained
since several entries back** (previously guessed as "probably unrelated, not pursued further"):
it's a harmless print-ordering issue, not a real ISP misconfiguration. `main.c` prints
`hcamera_isp.statArea` *between* `ISP_Init()` and `ISP_Start()` -- but the actual statistic-area
configuration code (`ISP_SVC_ISP_SetStatArea()`, along with demosaicing/contrast/gain/etc.) lives
inside `ISP_Start()` (`isp_core.c`), not `ISP_Init()`. At the moment `main.c` prints it, `ISP_Start()`
hasn't run yet, so `hIsp->statArea` is still genuinely all-zero from `ISP_Init()`'s `memset()` --
a few lines later, once `ISP_Start()` actually executes, it gets set to `(16,16,320,240)` as
configured. The statistics engine (which feeds AEC/AWB metering) is very likely correctly
configured by the time real capture starts; this print line is just checking too early. Not
pursued further -- confirmed not the cause of the darkness.

**Instrumentation added instead of guessing another IQ parameter blind**: `isp_gain`/
`isp_exposure` (`main.c` -- the same globals `GetSensorGainHelper`/`GetSensorExposureHelper`
already track, no longer `static`) are now printed every second in `app_threadx.c`'s existing
`[UVC_CAP]` status line: `... isp_gain=%ld isp_exposure=%ld`. This will show directly what AEC
has actually converged the sensor to -- confirming or ruling out "exposure pinned near the
`exposure_max=3522` ceiling" instead of inferring it indirectly from FPS alone. Rebuilt clean
(74.22% RAM, negligible increase from the two globals losing `static`).

### Next steps

1. **Flash and read the new `isp_gain=... isp_exposure=...` values** once the image has settled
   into its stable dark state. Two scenarios:
   - `isp_exposure` at or very near `3522` (the ceiling) and `isp_gain` low/near `0`: AEC is
     exposure-maxed but not applying gain -- check whether `evision`'s AEC is actually supposed to
     drive gain at all once exposure ceilings out, or whether that's a separate/manual
     responsibility this project needs to add (e.g. a fallback that raises `isp_gain` once
     `isp_exposure` is pinned).
   - `isp_exposure` well below the ceiling and `isp_gain` also low: AEC isn't actually pushing
     either value very hard, meaning it may believe the scene is *already* close to its target --
     which would point back at the statistics/metering path after all (contradicting the
     print-ordering explanation above) or at `ISP_IDEAL_TARGET_EXPOSURE`/`exposureCompensation`
     being tuned for a much darker target than expected.
2. Depending on which scenario: either add/enable gain control explicitly, or try
   `exposureCompensation = EXPOSURE_TARGET_PLUS_1_0_EV` (or higher) to push the target brighter,
   or dig into whether `evision`'s AEC has its own gain-vs-exposure prioritization knobs not yet
   discovered in this codebase.
3. Keep in mind lighting conditions matter here too -- ask what the actual test environment's
   ambient light level is before assuming this is purely a software tuning problem; IMX219 is not
   a particularly light-sensitive sensor and some environments may need supplementary lighting
   regardless of how well AEC/gain is tuned.

---

## 2026-09-28 (latest #10) — Bug 21, and a correction to Bug 18: a THIRD copy of FRM_LENGTH_LINES was silently overwriting Bug 18's fix on every single boot; Bug 18 never actually reached the sensor until now

**While grepping for other `1762`/`1763`/`0x06E3` duplicates** (this entry's own previous-entry
"next steps" item, prompted by finding Bug 20's stale `exposure_max`) **found the real one, and
it changes the story**: `imx219.c`'s `IMX219_Configure640x480()` writes `imx219_common_regs` (the
table Bug 18 fixed, `FRM_LENGTH_LINES` -> `0x0DC6`/3526) first, but then, a few steps later in the
*same function*, does an **unconditional explicit write** of `IMX219_REG_FRAME_LENGTH_MSB/LSB` to
`0x06`/`0xE3` (1763) again, with its own comment: `/* 4. Frame length = 0x06E3 */`. This runs
*after* the table write and silently overwrites it, every single boot.

**This means Bug 18's frame-length fix has never actually reached the sensor, before or after
that entry.** Before Bug 18, both this override and the table agreed (both `1763`) -- pure
coincidence, and exactly why nothing caught the duplication at the time. After Bug 18 "fixed" the
table to `3526`, this override kept silently putting it back to `1763` on every boot. The
sensor's real `FRM_LENGTH_LINES` has been `1763` continuously, unaffected by that entry's fix.

**This does NOT change Bug 19's conclusion** -- the striping fix (IPPlug `WLRURatio`/
`DPREGStart`/`DPREGEnd`) is still correct and confirmed on hardware; frame length staying at 1763
the whole time is actually *consistent* with Bug 18's frame-length experiment showing "zero
effect" on the striping (Bugs 15-18's session before this one) -- there was never a real change
to observe, for a different reason than assumed at the time (not "frame length doesn't matter",
but "frame length never actually changed"). Bug 18's `0x030D` (PLL) and `0x0174`/`0x0175`
(binning) fixes are unaffected by this and remain genuinely applied.

**Why this matters now**: this entry's own Bug 20 fix (`exposure_max = 3522`, assuming the sensor
really runs at `FRM_LENGTH_LINES = 3526`) was computed against a ceiling the sensor was **not
actually operating at** -- telling `evision`'s AEC it can request exposure up to `3522` lines when
the sensor can really only handle up to about `1759` is exactly the kind of assumed-vs-actual
mismatch that produces erratic convergence, on top of the exposure/gain state-reporting bug found
in the same pass (below).

**Second bug found in the same file, same investigation**: `IMX219_Configure640x480()` also
explicitly writes an initial `COARSE_INTEGRATION_TIME = 0x0640` (1600) and `ANALOG_GAIN = 0x00`
to the sensor at init (steps 5-6, right after the frame-length override) -- but `main.c`'s
`isp_exposure`/`isp_gain` static globals (what `GetSensorExposureHelper`/`GetSensorGainHelper`
report back to `evision` as "the sensor's current state") started at `0`/`0`, not `1600`/`0`.
`evision`'s AEC would have believed the sensor's starting exposure was `0` (minimum) when it was
actually genuinely `1600` (a real, moderately long exposure -- consistent with the reported
"flashes reasonably bright for ~1s" at boot, before AEC's first correction). AEC's very first
control step would then be computed against a lie about the starting point, which is a plausible
mechanism for the observed "crushes to near-black and stays there" -- an aggressive correction
computed from a wrong baseline, with no self-correcting feedback afterward if the algorithm
considers itself converged.

**Fix, all three pieces together** (this needs the frame-length fix to actually reach hardware
before Bug 20's `exposure_max` value is even meaningful):
1. `imx219.c`: the rogue frame-length override now writes `0x0D`/`0xC6` (3526), matching the
   table it was silently overwriting.
2. `main.c`: `isp_exposure` global's initial value changed from `0` to `1600`, matching what
   `IMX219_Configure640x480()` actually writes to the sensor at init, so `evision`'s first read of
   "current exposure" is no longer a lie.
3. Bug 20's `exposure_max = 3522` (previous entry) is now actually correct, since the sensor's
   frame length genuinely is `3526` as of fix #1 above.

Rebuilt clean (74.22% RAM, unchanged). **Not yet tested on real hardware as of this entry.**

**Lesson, stated plainly since it's now happened twice in one investigation**: a single real-world
fact (this sensor's frame length) was duplicated in *three* places in this codebase (the register
table, this override, and `GetSensorInfoHelper`'s `exposure_max` comment) and a *second*,
unrelated fact (initial exposure/gain) was duplicated in two places (the sensor init write and the
IQ-middleware's tracking globals) with no single source of truth for either. Before considering
any sensor-timing or exposure/gain value "fixed" on this project going forward, grep for every
place the underlying constant appears, not just the first one found.

### Next steps

1. **Flash and check exposure behavior.** This is the real test of whether the flash-then-crush
   symptom is fixed -- watch specifically for whether it now settles into a stable, reasonable
   brightness instead of crushing to black.
2. If still unstable: the `evision` AEC algorithm itself is a precompiled library
   (`libn6-evision-st-ae_gcc.a`) -- its internal control-loop logic can't be inspected or
   line-by-line debugged from source. Remaining adjustable inputs are `exposureCompensation`,
   `exposureTarget`(computed from it), `antiFlickerFreq`, and the sensor info min/max bounds --
   if those are all now consistent with real hardware and it's still unstable, that's a genuine
   evision/hardware interaction issue worth raising with ST rather than something fixable by
   adjusting this project's own IQ parameters further.
3. Once exposure is confirmed stable: return to evaluating white balance (the placeholder
   `ispGainStatic` values from the entry before this one).

---

## 2026-09-28 (latest #9) — Bug 20 found: GetSensorInfoHelper()'s exposure_max was a stale duplicate of the FRM_LENGTH_LINES value Bug 18 already fixed elsewhere -- AEC was told the exposure ceiling is ~half what it actually is

**Symptom after enabling `AECAlgo`**: the image flashes reasonably bright for about a second,
then crushes down to near-black and stays there (user report + screenshot -- barely visible dark
scene). This isn't the expected "converges to a stable, reasonable exposure" behavior.

**Root cause**: `main.c`'s `GetSensorInfoHelper()` -- the `[MANDATORY]` callback that tells
`evision`'s AEC algorithm the sensor's valid exposure range -- had:

```c
Info->exposure_max = 1762;      // FrameLength-1 (0x06E3-1)
```

`0x06E3 = 1763` is the OLD, hand-tampered `FRM_LENGTH_LINES` value from `imx219.c` that **Bug 18
already fixed** (restored to `0x0DC6 = 3526`, matching `../Camera_N6_AI_Test`). This second,
independent copy of the same fact -- duplicated as a hardcoded comment-justified constant here --
was never updated when Bug 18 changed the real register value. Since Bug 18, the sensor's actual
usable exposure range has been roughly double what this callback was telling the AEC library:
`evision` has been computing its control loop against a ceiling that's off by 2x, which is
entirely consistent with an initial reasonable-looking exposure (probably close to the sensor's
power-on default, before AEC's first correction) followed by the control loop mis-converging once
it starts adjusting against the wrong assumed ceiling.

**Fix**: `exposure_max` changed from `1762` to `3522` (`FRM_LENGTH_LINES(3526) - 4`, matching
`../Camera_N6_AI_Test`'s own `COARSE_INTEGRATION_TIME` margin convention -- "max - 4 lines").
Rebuilt clean (74.22% RAM, unchanged -- a constant-value fix only). **Not yet tested on real
hardware as of this entry.**

**Lesson**: this is the second time in this session a single real-world fact (the sensor's frame
length) turned out to be duplicated in more than one place in the source (Bug 18 found it
hardcoded in `imx219.c`'s register table; this entry found a second, independent copy baked into
a comment-justified constant in `GetSensorInfoHelper()`). Worth grepping for any other places a
sensor timing constant might be duplicated (search for `1762`, `1763`, `0x06E3` again) before
declaring the color/exposure investigation fully done.

### Next steps

1. **Flash and check exposure behavior specifically**: does it still flash-then-crush, or does it
   now settle into a stable, reasonable brightness within the warmup window? This is a narrower,
   more specific check than "does the color look right" -- exposure and white balance are
   separate concerns, and this fix only addresses the former.
2. If exposure is now stable but still not well-tuned (too bright/dark at the stable point):
   `AECAlgo.exposureCompensation` (currently `EXPOSURE_TARGET_0_0_EV`) is the adjustment knob --
   `isp_core.h` defines steps from `EXPOSURE_TARGET_MINUS_2_0_EV` to `EXPOSURE_TARGET_PLUS_2_0_EV`.
3. Once exposure is confirmed stable, go back to evaluating white balance (Bug 19's follow-up
   entry's placeholder `ispGainStatic` values) -- it's hard to judge color accuracy on an image
   that's still fighting a broken exposure loop.
4. `grep -rn "1762\|1763\|0x06E3"` across `Appli/Core/Src` once more before considering the
   Bug 18/20 sensor-timing cleanup fully finished.

---

## 2026-09-28 (latest #8) — Post-fix cleanup + first pass at color: debug prints gated off, IQ params were ST's untouched "DUMMY sensor" template with AEC/AWB/ColorConv all disabled -- AEC + a placeholder static white balance enabled

**Debug cleanup**: all the diagnostic printf blocks added while chasing Bugs 15-19 are now gated
behind `#if DEBUG_xxx 0` (easy to re-enable, not deleted, since they're genuinely useful if a
similar corruption ever recurs):
- `main.c`: `[MEM_TEST]` (`DEBUG_MEM_TEST`), `[WARMUP_SRC]`/`[MEM_SCAN]` (`DEBUG_MEM_SCAN`)
- `app_threadx.c`: `[DCMIPP_DIAG]` register poll (`DEBUG_DCMIPP_DIAG`) -- the `hdcmipp.ErrorCode =
  HAL_DCMIPP_ERROR_NONE;` reset stays unconditional (cheap, keeps the sticky latch useful if
  re-enabled)
- `ux_device_video.c`: `[UVC_SRC]` throttled source dump (`DEBUG_UVC_SRC`)

Rebuilt clean (74.22% RAM, a small drop from removing a couple of session-local diagnostic
arrays -- confirms the debug code was never a meaningful RAM consumer, see below).

**RAM usage explained** (user asked why 74% of the 2047K `RAM` region is used): parsed the
`.map` file's `.bss` section directly rather than guessing. Four allocations account for 1330 of
the 1338.5 KB total:

| Size | Symbol | What it is |
|---|---|---|
| 600 KB | `video_buf1` | Second full-resolution (640x480x2) ping-pong capture buffer |
| 450 KB | `mcu_buffer` | JPEG encoder's uncompressed staging buffer (640x360x2, cropped) |
| 192 KB | `ux_byte_pool_buffer` | USBX's internal memory pool (descriptors, transfer buffers) |
| 64 KB | `jpeg_out_buf` | Compressed JPEG output buffer |

Note `camera_framebuffer` (the *first* ping-pong buffer, also 600 KB) does NOT count toward this
-- it lives at the fixed `0x34200000` address in a separate physical SRAM bank outside the
linker's `RAM` region entirely (see the "physical memory gap" investigation several entries back
for why it's placed there). This is architecturally normal for a full-resolution double-buffered
capture + JPEG + USBX pipeline on this RAM budget, not a leak or bug -- no action taken.

**Color investigation started**: the striped/hardware bug (Bugs 15-19) is now closed; the
dark/greenish image the user is now seeing is a *different*, pre-existing issue with the ISP IQ
(image-quality) tuning, not a new regression. Read `isp_param_conf_imx219.h` in full: its own file
header literally says `/* Minimal configuration (demosaicing + decimation = 1) of DUMMY sensor */`
-- **every IQ block is disabled except demosaicing**: `AECAlgo.enable=0` (no auto-exposure),
`AWBAlgo.enable=0` (no auto white balance, and its 5-profile color-temperature table is entirely
empty -- `id={"","","","",""}`, all gains/coefficients 0), `ispGainStatic.enable=0`,
`colorConvStatic.enable=0` (a 3x3 RGB matrix, currently all-zero), `contrast.enable=0`. This file
was never actually tuned for this sensor/module -- `evision`'s AE/AWB libraries load and
`ISP_BackgroundProcess()` is already pumped every frame (both in `main.c`'s warmup loop and
`app_threadx.c`'s UVC loop), but with `AECAlgo.enable=0` the auto-exposure loop has nothing to
converge, and the sensor is left running on whatever exposure/gain it powers up with -- consistent
with a dark image. No white balance of any kind is ever applied, consistent with an
uncorrected/greenish cast (Bayer RGGB has 2x as many green samples as red or blue, so an
unbalanced image skews green to the eye).

**Fix, first pass** (`isp_param_conf_imx219.h`):
- `AECAlgo.enable`: `0` -> `1`. This is the cheapest real fix available -- every piece of
  infrastructure it needs (the `evision` ST-AE library, the `ISP_BackgroundProcess()` pump loop,
  `main.c`'s `SetSensorGainHelper`/`SetSensorExposureHelper` -> real `IMX219_SetGain`/
  `IMX219_SetExposure` I2C calls) already exists and was already wired up; it was simply never
  turned on. Should let the image auto-brighten to a reasonable exposure over the warmup period.
- `ispGainStatic`: `enable=0` -> `1`, with placeholder static white-balance gains (`ispGainR=
  1.5x, ispGainG=1.0x, ispGainB=1.4x` in this struct's `1e8=1.0x` fixed-point unit) -- ballpark
  values borrowed from `../Camera_N6_AI_Test`'s manually-tuned daylight WB (`R~1.5x, G=1.0x,
  B~1.4x`), NOT independently calibrated for this specific module. This only takes effect while
  `AWBAlgo` stays disabled (`isp_core.c`'s `ApplyIQParams`: `if (ispGainStatic.enable &&
  !AWBAlgo.enable)`).
- `AWBAlgo` deliberately left disabled: its 5-profile color-temperature table needs real
  calibration data (normally produced with ST's X-CUBE-ISP tool against actual captures under
  known lighting) that doesn't exist for this module yet. Enabling it with the current
  all-empty/all-zero profile array would very likely misbehave, not just "not help."
- `colorConvStatic` (the general 3x3 RGB matrix) and `contrast` left disabled for now --
  brightness and white balance are the bigger, more obviously-broken issues; revisit these only
  if the image still looks off after AEC/WB convergence is actually seen on hardware.

Rebuilt clean (main.c.obj recompiled via the header dependency, same 74.22% RAM -- these are
compile-time constant table values, no new runtime allocation). **Not yet tested on real
hardware.** This is explicitly a first pass, not a finished color calibration -- expect to need
at least one more round of "flash, look at the image, adjust gains" since none of these specific
numeric values have been verified against this module's actual output yet.

### Next steps

1. **Flash and look at the actual image.** Two things to check: (a) does it brighten to a
   reasonable exposure within the ~60-frame warmup window (watch for it getting less dark over
   the first second or two of streaming), and (b) does the color cast improve, even if not
   perfect. Report what's still wrong (too dark/bright, still a color cast and which direction)
   so the next adjustment is aimed at real feedback instead of another guess.
2. If exposure still doesn't move: check whether `ISP_BackgroundProcess()` is actually being
   reached/doing anything now that `AECAlgo.enable=1` -- add a throttled print of
   `isp_gain`/`isp_exposure` (already-existing globals `main.c`'s helpers write to) to confirm the
   loop is actually driving real I2C writes, the same "verify it reached hardware, don't assume"
   discipline that mattered for Bugs 16-19.
3. If white balance is still off in a clear direction (too red/blue/green): adjust
   `ispGainStatic`'s R/G/B values proportionally -- these are a linear per-channel gain, so e.g.
   "too green" means reducing `ispGainG` relative to R/B, or raising both R and B.
4. Proper AWB (via `AWBAlgo`) and a real color-conversion matrix (`colorConvStatic`) are follow-up
   work, not blocking -- they need actual calibration captures (X-CUBE-ISP or equivalent) that
   this session cannot produce; the static placeholder above is meant to get a usable image now,
   not to be the final answer.

---

## 2026-09-28 (latest #7) — Bug 19 CONFIRMED: matching IPPlug CLIENT2's WLRURatio + DPREGStart/End to Camera_N6_AI_Test's exact values fixed the striping completely -- `[MEM_SCAN]` now `bad_rows=0` across the full 480-row frame

**Flashed Bug 19's fix and the corruption is gone.** `[MEM_SCAN] bad_rows=0 (of which full-line
bad=0) gap_between_bad_rows: min=0 max=0`, full 480-row bitmap all `.`, and the actual UVC image
(screenshot) shows a clean, unstriped picture for the first time this entire investigation. This
is the real fix.

**Root cause, now confirmed**: PIPE1's IPPlug (`CLIENT2`) `WLRURatio` and/or `DPREGStart`/
`DPREGEnd` were misconfigured -- `WLRURatio=15` (max AXI arbitration priority) and a full
0x000-0x3FF FIFO pool, instead of `WLRURatio=4` and a 0x100-0x1FF half-FIFO slice, as
`../Camera_N6_AI_Test`'s confirmed-working `CLIENT2` config uses. `MemoryPageSize`/`Traffic`/
`MaxOutstandingTransactions` were changed to match the reference in the same commit but were
already independently proven not to matter (Bugs 16/17); `WLRURatio`/`DPREGStart`/`DPREGEnd` were
the two fields never tested until this pass, and one or both of them was the actual bug. (Not yet
narrowed down which -- see Next Steps if that distinction ever matters.)

**Everything else chased through Bugs 15-18 (IMX219 PLL/binning/frame-length, ISP demosaic,
`MemoryPageSize`, `MaxOutstandingTransactions`, the physical-memory-gap theory) was a real,
legitimate experiment that correctly ruled itself out -- none of it was wrong to try, and Bug 18's
IMX219 register fixes are still worth keeping even though they weren't the cause of this
particular bug (the original hand-tampered `0x030D`/`0x0160`/`0x0174` values were genuinely wrong
regardless). The lesson from the previous several entries holds: matching a full, working
reference configuration field-by-field beat every individual datasheet-reasoning guess.**

### Next steps (post-fix)

1. **Cleanup**: all the diagnostic printf blocks added while chasing this bug (`[MEM_TEST]`,
   `[WARMUP_SRC]`, `[MEM_SCAN]`, `[DCMIPP_DIAG]` in `app_threadx.c`, `[UVC_SRC]` in
   `ux_device_video.c`) are no longer needed for day-to-day use -- being commented out/removed now
   that the bug is confirmed fixed (see the dated entry below for exactly what was touched).
2. **Color correctness is the next real task** -- the original point of this entire ISP
   investigation, finally reachable now that a real, complete, non-corrupted frame exists. The
   live image currently looks dark/greenish (user-reported, screenshot). This is a fresh
   investigation, not a continuation of Bugs 15-19's territory -- likely candidates: the IMX219
   Bayer pattern assumption (`ISP_DEMOS_TYPE_RGGB` in `isp_param_conf_imx219.h`) actually matching
   this sensor/module's real color filter orientation, `evision` AWB/AE convergence behavior,
   `DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1`'s bit layout expectations, and whether `SWAPRB` needs to
   be set.
3. If it's ever worth knowing precisely which of `WLRURatio` vs `DPREGStart`/`DPREGEnd` was the
   actual lever (not required for anything to work today): revert one back toward this project's
   old value at a time and re-check `[MEM_SCAN]`.

---

## 2026-09-28 (latest #6) — Bug 18's full fix (PLL + frame length + binning, all three registers) ALSO had zero effect, though confirmed to genuinely reach hardware; IPPlug's two never-tested fields (WLRURatio, DPREGStart/End) matched to Camera_N6_AI_Test's confirmed-working CLIENT2 values as the next, more disciplined experiment

**Flashed all three of Bug 18's register fixes together and the corruption is still byte-for-byte
identical**: `bad_rows=180`, `full-line bad=120`, same exact row list. This time, before concluding
"the fix didn't work," verified it actually reached hardware rather than assuming: `Appli/build/
Appli.bin`'s mtime was checked against the edited source files' mtimes and confirmed newer (the
binary really was rebuilt after the edits), and the captured pixel values themselves changed
between logs (`82 10` before -> `03 19` after -- a genuinely different brightness/color, consistent
with the binning-mode and frame-length changes actually altering the sensor's real analog
behavior, not just cosmetic). **This is a trustworthy negative result, not an unflashed one.**

**Where this leaves the running tally of ruled-out hypotheses, each with direct hardware
evidence**: IPPlug `MemoryPageSize`, IPPlug `MaxOutstandingTransactions`, a physical memory-map
gap, the ISP demosaic (Bayer2RGB) block, IMX219's MIPI output PLL multiplier, IMX219's frame
length, and IMX219's binning mode -- seven independent, plausible-looking hypotheses, each
falsified with proof the change actually landed in hardware. This is an unusually thorough
negative result and worth being honest about: at this point, guessing another single register
value has a poor track record and diminishing returns.

**Found two IPPlug fields that were genuinely never varied, with concrete reference values to
try instead of guessing blind**: re-reading `../Camera_N6_AI_Test`'s CLIENT2 (its own PIPE1)
`HAL_DCMIPP_SetIPPlugConfig()` call -- not just CLIENT1's, which is what an earlier session pass
happened to read first -- found:

```
                     This project (current)   Camera_N6_AI_Test CLIENT2 (confirmed working)
MemoryPageSize       256 bytes                 64 bytes
Traffic (burst)      128 bytes                 64 bytes
MaxOutstanding       4                          8
WLRURatio            15 (max priority)          4
DPREGStart/End       0x000 / 0x3FF (full pool)  0x100 / 0x1FF (a 256-word half)
```

Two things stand out: (1) the reference uses `MemoryPageSize` == `Traffic` == 64 bytes, both
equal, not `page >= burst` as Bug 16 theorized (and this project's own earlier "fixed" value of
256 wasn't the reference's choice either) -- direct evidence Bug 16's whole theory about this
field was never right, just a plausible-sounding read of the datasheet-style comment. (2)
`WLRURatio` and `DPREGStart`/`DPREGEnd` -- the AXI arbitration weight and this client's slice of
the shared internal FIFO -- have never been touched by any experiment this session, unlike every
other IPPlug field.

**Fix / next experiment**: matched this project's `CLIENT2` config to the reference's exactly --
`MemoryPageSize=64B`, `Traffic=64B`, `MaxOutstandingTransactions=8`, `WLRURatio=4`,
`DPREGStart=0x100`, `DPREGEnd=0x1FF`. This is a full field-for-field match against a *proven*
working configuration, not another isolated guess. Rebuilt clean (`cmake --build build/Debug`,
74.38% RAM, unchanged). **Not yet re-tested on real hardware as of this entry.**

**If this also has no effect**: every DCMIPP/IPPlug/ISP/sensor-timing register this project's
source code touches will have been exhausted, either individually confirmed correct+ineffective
or now matched exactly to a working reference. At that point the honest conclusion is that this
is very likely either (a) a board/hardware-level physical-layer issue (the camera module's FPC
ribbon/connector seating is worth the user physically checking -- a marginal mechanical contact
can produce a deterministic, reproducible glitch rather than random noise, which fits this bug's
profile of being 100% byte-identical across flashes) or (b) an undocumented DCMIPP/CSI silicon
behavior not covered by the public HAL/errata sheet, which would need ST support/community input
with this session's now extremely detailed, reproducible bug report (exact bad-row bitmask, every
ruled-out hypothesis and how it was ruled out) rather than further local guessing.

### Next steps

1. **Flash and check.** If fixed: this was IPPlug's `WLRURatio` and/or `DPREGStart`/`DPREGEnd`
   after all, and it would be worth then testing which of the two mattered (revert one at a time
   back toward this project's old values) purely out of curiosity, though not required for the
   feature to work.
2. **If NOT fixed** (same exact bad-row bitmask again): stop testing DCMIPP/IPPlug/ISP/sensor
   registers -- this list is now exhausted. Recommend to the user: (a) physically inspect/reseat
   the camera module's ribbon cable/connector, since a marginal physical connection is one of the
   few remaining explanations consistent with "invariant to every software change tried," and
   (b) post this session's findings to ST's community forum or support channel -- a precise,
   reproducible bug report with every ruled-out hypothesis documented is exactly the kind of
   report that gets useful answers from people with access to internal DCMIPP timing
   documentation this project does not have.
3. Do not re-open `MemoryPageSize`/`MaxOutstandingTransactions`/demosaic/IMX219 PLL-binning-frame-
   length without genuinely new evidence -- all confirmed, with hardware proof, not the cause.

---

## 2026-09-28 (latest #5) — Bug 18's first fix (PLL_OP_MPY alone) was NOT sufficient on hardware; a full register-table diff found two more hand-tampered IMX219 registers (FRM_LENGTH_LINES halved, BINNING_MODE changed) with the same tamper signature; all three fixed together

**Flashed Bug 18's fix (just `0x030D` + `PHYBitrate`) and the striping was completely unchanged**
-- same visual banding, same `row60+8`/`row60+16` reading solid `0xFF`. The PLL bit-rate fix alone
did not work.

**Went back and did the full, careful diff this time** (every register address in both projects'
`imx219.c` tables, not just the one that first stood out) and found **two more differences**,
both with the exact same "hand-tampered, not from the reference" signature as `0x030D`:

```
                          This project (broken)      Camera_N6_AI_Test (confirmed working)
FRM_LENGTH_LINES (0x0160/61)   0x06E3 = 1763               0x0DC6 = 3526   (exactly HALF)
BINNING_MODE_H/V_A (0x0174/75) 0x01 / 0x01                 0x03 / 0x03
```

The `0x0174`/`0x0175` lines in this project's source even had a bare trailing `//` comment with
nothing after it -- the same shape as `0x030D`'s stripped `//{0x030D,0x72},` line right above the
hand-edited value, strongly suggesting the same past editing session touched all three registers
together (most likely an attempt to hand-tune framerate/bandwidth by dividing several timing
parameters, done inconsistently, that was never fully reverted). Every other register in both
tables -- sensor window (`0x0164-0x016B`), output size (`0x016C-0x016F`), `LINE_LENGTH_PCK`
(`0x0162/63`, identical `0x0D78` in both), lane count, VT PLL (`0x0301/0303/0304/0306/0307`) --
matches exactly.

**Why this plausibly compounds with the PLL bit-rate bug**: `BINNING_MODE` controls how the
sensor's analog readout actually combines pixels (this project's own reference comment: `0x03` =
"2×2 analog binning" for both H and V) -- a different binning submode is a different analog
readout timing internally, not just a software-visible format choice. A shorter
`FRM_LENGTH_LINES` means less vertical blanking between frames. Both are exactly the kind of
change that would need to be co-tuned with the OP PLL multiplier to keep the sensor's internal
timing self-consistent -- changing PLL_OP_MPY alone, with the other two still at their tampered
values, would still leave the link's actual timing behavior different from the reference's
validated configuration. This is consistent with why the first, partial fix had zero visible
effect.

**Fix**: restored both remaining registers to the reference's confirmed values in `imx219.c`:
- `0x0160/0x0161` (`FRM_LENGTH_LINES`): `0x06E3` (1763) -> `0x0DC6` (3526)
- `0x0174/0x0175` (`BINNING_MODE_H_A`/`BINNING_MODE_V_A`): `0x01`/`0x01` -> `0x03`/`0x03`

Combined with the previous entry's `0x030D` (`PLL_OP_MPY`) and `main.c`'s `PHYBitrate` fix, this
project's IMX219 sensor configuration should now match `Camera_N6_AI_Test`'s confirmed-working
table in every register that affects sensor timing/readout -- only genuinely orthogonal
differences remain (this project uses software AWB/AE via `evision`/`ISP_MW` instead of the
reference's fixed manual exposure/gain registers, and RGB565+ISP-demosaic output instead of the
reference's YUV422 -- both are deliberate architectural choices for this project, not bugs).
Rebuilt clean (`cmake --build build/Debug`, 74.38% RAM, no size-relevant change). **Not yet
re-tested on real hardware as of this entry.**

**Lesson reinforced**: the first pass at Bug 18 found the *first* thing that looked wrong and
declared victory without finishing the diff. The comparison methodology this project's own
`knowledge_archive.md` credits for cracking every original bring-up bug (1-6) is "diff the whole
relevant table/section, not just until you find one plausible-looking difference." Worth
remembering explicitly for next time: a single found discrepancy is a lead, not automatically the
whole answer, especially when nearby registers in the same functional area (frame timing, in this
case) haven't been checked yet.

### Next steps

1. **Flash and check.** Same as before -- easiest check is just looking at the UVC image for
   banding, `[MEM_SCAN]` for confirmation. If still striped, do the diff a third time with even
   more care (check every single register address in both tables character-by-character,
   including ones not touched in this entry), since two rounds of "found a real, hardware-tampered
   difference, fixed it, still broken" would mean either a fourth tampered register remains or
   this diffing approach has reached its limit and the physical-layer investigation from two
   entries back needs to be revisited instead.
2. If confirmed fixed this time: same follow-ups as noted in the previous entry (AWB/AE color
   tuning is next, update `Camera_README.md`/`knowledge_archive.md` with the full Bug 18 story
   including this correction).

---

## 2026-09-28 (latest #4) — Bug 18 FOUND: IMX219's MIPI output PLL hand-edited to ~1/4 its correct multiplier, running the D-PHY link at ~224 Mbps/lane instead of ~912 -- almost certainly the real root cause of Bugs 15-17's period-32 corruption

**Found by the user, not by more register-guessing**: the user pointed at `../Camera_N6_AI_Test`
-- a *different* sibling project, confirmed working with only color/FPS issues (never any
striping), also on IMX219, also 640x480 RAW10 2-lane 30fps -- and asked directly why this project
stripes and that one doesn't. Diffing the two projects' IMX219 PLL register tables side by side
(`imx219.c` in each) found it immediately:

```
Camera_N6_AI_Test/Appli/Src/imx219.c:   { 0x030D, 0x72 },   /* PLL_OP_MPY[7:0] = 114 */
NUCLEO-N65X0Q-ISP/Appli/Core/Src/imx219.c:
    //{0x030D,0x72},
    {0x030D,0x1C}, // change here (div 4)
```

**The correct value was sitting right there, commented out.** Someone hand-edited this register
from `0x72` (114) down to `0x1C` (28, "change here (div 4)" -- 114/4 ≈ 28) at some point in this
project's history, and it was never reverted. `0x030D` is the low byte of `PLL_OP_MPY`, IMX219's
MIPI-output PLL multiplier -- it directly sets the sensor's actual CSI-2 per-lane bit rate:

```
bit_rate_Mbps = (INCK_MHz / PREPLLCK_OP_DIV / OPSYCK_DIV) * PLL_OP_MPY
              = (24 / 3 / 1) * PLL_OP_MPY = 8 * PLL_OP_MPY
Camera_N6_AI_Test (PLL_OP_MPY=114): 8 * 114 = 912 Mbps/lane -- matches its own comment
                                     ("912 Mbps/lane for IMX219") and its DCMIPP_CSI_PHY_BT_900.
This project      (PLL_OP_MPY=28):  8 * 28  = 224 Mbps/lane -- matches this project's own
                                     DCMIPP_CSI_PHY_BT_220, so the PHY_BT setting was internally
                                     "consistent" with the wrong sensor clock, which is exactly
                                     why nothing ever flagged an obvious mismatch.
```

Every other PLL register in both projects' tables is byte-for-byte identical (`0x0301=5,
0x0303=1, 0x0304=3, 0x0305=3, 0x0306=0, 0x0307=0x39, 0x030B=1, 0x030C=0`), same sensor window
(640x480), same lane count (2), same frame rate (30fps) -- `0x030D` is the *only* difference in
the entire PLL table, and it's a 4x difference in the actual physical link speed.

**Why this explains everything Bugs 15-17 spent so long trying to pin on DCMIPP register
configuration**: running the IMX219's D-PHY transmitter ~4x below the bit rate its own PLL/timing
was otherwise designed around (window timing, blanking, etc. all still configured as if running
at full speed) is a genuine physical-layer misconfiguration -- and unlike random signal-integrity
noise, a *wrong but fixed* clock ratio produces a *deterministic, exactly repeatable* error
pattern, which is precisely what was observed: byte-for-byte identical corruption across separate
flashes, unaffected by system load, unaffected by `MemoryPageSize`/`MaxOutstandingTransactions`
(confirmed correct via register readback), and unaffected by disabling the ISP demosaic block
entirely (confirmed identical corruption with it OFF). All of that evidence pointed at "something
upstream of the entire DCMIPP/ISP pixel pipe, in CSI reception itself" -- which is exactly what a
wrong MIPI PLL multiplier is. The five-sub-cluster-per-32-row rhythm is consistent with a beat
pattern between the sensor's actual (wrong, slow) bit clock and DCMIPP's CSI receiver state
machine, though the exact mechanism isn't necessary to fix the bug now that the real
misconfiguration is identified.

**Fix**: 
1. `imx219.c` -- restored `0x030D` to `0x72` (114), removing the "div 4" hand-edit. Left a
   comment explaining the bit-rate math and pointing at this worklog entry, since this is a
   `USER CODE`-adjacent hand-maintained register table that a future person could just as easily
   re-break without knowing why `0x72` matters.
2. `main.c`'s `MX_DCMIPP_Init()` -- changed `pCSI_Config.PHYBitrate` from `DCMIPP_CSI_PHY_BT_220`
   to `DCMIPP_CSI_PHY_BT_900`, matching the corrected sensor bit rate and mirroring
   `Camera_N6_AI_Test`'s confirmed-working config exactly.

Rebuilt clean (`cmake --build build/Debug`, 74.38% RAM, no change -- this is a pure register-value
fix, no new code). **Not yet re-tested on real hardware as of this entry.**

**Methodology note, worth remembering**: this project's own knowledge_archive.md already
documented "diff against a known-good sibling project" as the technique that cracked every one of
Bugs 1-6 (the original camera bring-up). Bugs 15-17 drifted away from that discipline into
register-level theorizing from datasheets/HAL comments alone (IPPlug page size, outstanding
transactions, demosaic bisection) -- all individually reasonable experiments, and useful for
ruling things out with hardware proof, but none of them were found by the one method that had
worked every time before: a direct side-by-side diff against a second confirmed-working reference
using the *same sensor*. `Camera_N6_AI_Test` was sitting right there the whole time. Next time a
"works there, not here" situation comes up on this project, diff the sibling project's relevant
config table *first*, before reaching for a HAL header and guessing.

### Next steps

1. **Flash and check `[MEM_SCAN]`.** If the pattern is gone (or the image is simply a normal,
   correctly-colored, unstriped picture -- easiest confirmation of all: for the first time this
   session, just look at the UVC stream), Bug 18 is confirmed as the actual root cause of Bugs
   15-17's corruption, and the demosaic-bypass/IPPlug detours (still valuable for ruling things
   out) can be marked as closed dead ends rather than live leads.
2. **If confirmed fixed**: color/AWB tuning is still untouched territory (this project's own
   `evision` AWB/AE vs. `Camera_N6_AI_Test`'s manual YUV/exposure gains are a different subsystem)
   -- the original point of the whole ISP investigation, now hopefully finally reachable.
   `Camera_README.md`/`knowledge_archive.md` should get a new entry for this bug once confirmed,
   same as the other major bugs found this project's history.
3. **If NOT confirmed** (pattern persists even at the correct bit rate): re-open the CSI/D-PHY
   physical-layer investigation from the previous entry, but this would be a genuine surprise
   given how precisely the reference project's numbers lined up.
4. Do not re-introduce a "divide the PLL by N" shortcut for any future frame-rate/bandwidth
   tuning on this sensor -- if a lower bit rate is ever wanted, the *sensor's actual timing
   registers* (line length, frame length) need to be recalculated together with the PLL, not just
   the PLL multiplier in isolation; that mismatch is exactly what this bug was.

---

## 2026-09-28 (latest #3) — Demosaic bisection test CONFIRMS the ISP Bayer2RGB block is innocent; bug is upstream of the entire ISP, in CSI reception / DCMIPP's raw capture path; test reverted

**Flashed with `HAL_DCMIPP_PIPE_DisableISPRawBayer2RGB()` active (demosaic off).** Result: the
image now shows raw, undemosaiced Bayer samples reinterpreted as RGB565 (as expected -- wrong
colors, by design of the test), but the `[MEM_SCAN]` output is **byte-for-byte, number-for-number
identical to the demosaic-ON baseline**: `bad_rows=180`, `full-line bad=120`, and the exact same
row list (`4 5 6 11 12 17 18 19 24 25 30 31 36 37 38 ...`). Not just "the same shape" -- the exact
same counts down to the integer.

**This conclusively rules out the ISP Bayer2RGB demosaic block.** Turning it off entirely changed
nothing about the corruption. Combined with everything else already ruled out this session
(`MemoryPageSize`, `MaxOutstandingTransactions`, and a physical memory-map gap, all disproven with
direct hardware/register evidence), **the defect must be upstream of the entire ISP pixel-pipe
stage** -- somewhere between the CSI-2 receiver taking data off the MIPI lanes and DCMIPP's raw
capture/line-buffering logic handing it to the (now proven irrelevant) ISP and AXI write stages.
This is no longer a DCMIPP peripheral-register configuration question in the sense this whole
session has been chasing -- every software-configurable knob in that space has been tried and
ruled out.

**Test reverted**: `BUG17_DEMOSAIC_BISECT_TEST` flipped back to `0` in `main.c` (demosaic
re-enabled) immediately after reading this result -- this test build must not be mistaken for
forward progress on image correctness, and normal color operation is restored. Rebuilt clean.

**Where this leaves things, and why further remote root-causing is reaching its limit**: the
remaining candidate space is CSI-2 physical/protocol-layer behavior -- D-PHY bit-rate calibration
margin (`DCMIPP_CSI_PHY_BT_220`, chosen to match the IMX219's configured MIPI clock), the 2-lane
byte-deinterleaving logic (`DCMIPP_CSI_TWO_DATA_LANES`), or a clock-domain-crossing FIFO between
the CSI byte-clock domain and DCMIPP's own 300MHz processing clock (`MX_DCMIPP_ClockConfig()`,
confirmed generously fast on paper -- ~20x headroom over the pixel rate this resolution needs, so
a plain throughput shortfall looks unlikely, though a per-pixel ISP pipeline *latency* budget is
harder to rule out without ST's internal timing documentation). None of these are configurable
from application code the way `MemoryPageSize` or `MaxOutstandingTransactions` were -- there's no
further register knob visible in the public HAL to try next.

Two observations argue against pure analog signal-integrity noise as the explanation, which is
worth keeping in mind for whoever picks this up next: (1) the corruption is **byte-for-byte
identical** across separate flashes/runs, not the kind of varying pattern random bit errors on a
marginal physical link would produce, and (2) the sub-cluster rhythm within each 32-row period
(bad-row runs starting roughly every 6-7 rows, five times per 32-row cycle -- `{4,11,17,24,30}` as
sub-cluster start offsets, deltas of 7,6,7,6,6) has the signature of a **deterministic digital
rate mismatch or periodic buffer catch-up**, not analog jitter -- e.g. a clock-domain FIFO between
CSI-2 byte reception and DCMIPP's internal pixel pipeline that structurally can't keep up for a
line every ~6-7 lines given this specific sensor timing (line length / blanking) and clock
configuration. This is an architecture-level hypothesis, not something confirmable by more
printf/register reads from this codebase alone.

**Considered and set aside**: switching to a different CSI lane count as a further bisection test
(1-lane vs 2-lane, to check whether the 2-lane byte-deinterleave logic is implicated). Not
attempted -- `imx219.h` only defines `IMX219_CSI_2_LANE_MODE`/`IMX219_CSI_4_LANE_MODE` (no 1-lane
constant), and this board's camera module's physical lane wiring (2 vs 4 lanes actually connected)
isn't confirmed from software alone; flipping this blind risks a total loss of signal rather than
a useful data point, unlike every other experiment this session (which degraded gracefully to
"still corrupted" in the worst case).

### Next steps

1. **This is now a strong candidate for escalating to ST's community forum or support channel.**
   This session has a precise, 100%-reproducible bug report ready to hand over: exact bad-row
   bitmask (`row mod 32` in `{4,5,6,11,12,17,18,19,24,25,30,31}`), confirmed independent of
   `MemoryPageSize`/`MaxOutstandingTransactions`/demosaic (all bisected out with register-level
   proof), present with zero system load, IMX219 RAW10 640x480 2-lane CSI-2 config. That's
   materially more specific than anything found in this session's public-forum research (see the
   entry further below) and worth a fresh post of its own rather than continuing to guess blind.
2. If continuing in-house: the next thing actually worth measuring (not just reading code) is the
   real MIPI line rate arriving from the IMX219 (compute from its PLL registers already dumped in
   every boot log: `0x0301=5, 0x0303=1, 0x0304=3, 0x0305=3, 0x0307=0x39, 0x030B=1, 0x030D=0x1C`,
   against this board's actual `INCK` external clock frequency -- not yet confirmed from this
   codebase, would need the schematic) versus what `DCMIPP_CSI_PHY_BT_220`'s calibration bin
   assumes. This needs the schematic's `INCK` value to do correctly; do not guess it.
3. Do not spend more time on `MemoryPageSize`, `MaxOutstandingTransactions`, `DPREGStart/End`,
   `WLRURatio`, or the ISP demosaic block -- all confirmed, with hardware evidence, not to be the
   cause. Revisiting any of them without new evidence would be repeating already-closed work.
4. Do not flip CSI lane count without first confirming, from the schematic or board documentation
   (not from software), how many CSI-2 data lanes are actually wired between the IMX219 module and
   this MCU -- guessing wrong here risks losing the image signal entirely rather than producing a
   useful data point.

---

## 2026-09-28 (latest #2) — Correction: the full 480-row scan shows a CLEAN, exact, deterministic period-32 pattern after all (previous entry's "irregular jitter" call was a mod-32 arithmetic mistake); ISP demosaic-bypass bisection test queued

**Correcting the previous entry.** It predicted the full 480-row scan would show either a clean
period or an irregular/jittery one, and guessed jitter based on `row 68`/`76` (bad) appearing to
disagree with the coarse scan's `row 64/72/80` (good). That was a mod-32 arithmetic error: `68 mod
32 = 4` and `76 mod 32 = 12`, and the new full scan's bad-row set **does** include offsets 4 and
12 in every 32-row block -- there was never a disagreement. Full data now available:

```
bad_rows=180 (of 480)   full-line bad=120   gap_between_bad_rows: min=1 max=5
bad row numbers: 4 5 6 11 12 17 18 19 24 25 30 31  36 37 38 43 44 49 50 51 56 57 62 63 ...
```

Reducing every bad row to `row mod 32` gives exactly one fixed set, repeating identically all 15
times across the 480-row frame with zero drift: **`{4,5,6, 11,12, 17,18,19, 24,25, 30,31}`** --
12 of every 32 rows (37.5%), arranged as five sub-clusters of sizes 3/2/3/2/2. This is not noise
or jitter -- it is a **clean, exact, 100% deterministic hardware pattern**, just one too fine-
grained to survive an 8-row-stride sample (0/8/16/24 mod 32 -- only 24 happens to land inside the
bad set, which is exactly why every earlier coarse `[MEM_SCAN]` across this entire session only
ever showed row-mod-32-equals-24 as "the" bad row: it was never wrong, just aliased). Of the 180
bad detections, 120 (two-thirds) are corrupted start-to-middle-to-end of the line; the other 60
are only partially bad -- worth keeping in mind, since a uniform "whole line replaced with idle
value" mechanism would predict 100% full-line, not 66%.

**Where this leaves the investigation**: three independent hypotheses for this bug are now
conclusively closed, each with hardware-level proof, not just "it didn't seem to help":
`MemoryPageSize` and `MaxOutstandingTransactions` (register readback confirmed correct, zero
change to the pattern) and a physical memory-map gap (CPU write-back test passed on every
known-bad row). A clean, exact period of 32 that survives all of that has to be coming from
somewhere in the hardware pixel pipe itself -- either the raw CSI/DCMIPP capture path, or the
ISP's hardware Bayer2RGB demosaic block that `DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1` always routes
through on PIPE1 (there is no raw-passthrough packer format available for this pipe -- checked
`stm32n6xx_hal_dcmipp.h`'s full `DCMIPP_PIXEL_PACKER_FORMAT_*` list). Register-level inspection of
`P1FCTCR` (`=0x08`, decodes to `CPTREQ` set, `FRATE`=0 i.e. no frame-rate decimation) and `P1PPCR`
(`=0x01`, decodes to `FORMAT`=RGB565, `LINEMULT`/`DBM`/`LMAWE` all 0 -- no line-multiplexing or
address-wrapping features active) turned up nothing that would explain a 32-row cycle; both
registers are configured exactly as expected for plain single-buffer capture.

(Also checked in passing: `hcamera_isp.statArea` printing `XSIZE=0`/`YSIZE=0` after `ISP_Init()`
despite `isp_param_conf_imx219.h` providing a real `statAreaStatic` of `X0=16 Y0=16 XSize=320
YSize=240` -- traced into `ISP_SVC_ISP_SetStatArea()` in `isp_services.c`, which writes `hIsp->
statArea = *pConfig` on success. Since `ISP_Init()` reported `ISP_OK` overall, this print is most
likely reading a value that's stale/inconsistent for a reason not yet chased down, rather than
proof the hardware stat-area register itself is wrong. Statistics-area is the AWB/AE metering
window, a separate hardware path from the main pixel/demosaic pipe PIPE1 write goes through, so
this is very unlikely to be related to the frame corruption -- noted for completeness, not
pursued further right now.)

**Bisection test added, to separate the two remaining suspects with one flash instead of guessing
between them**: right after `ISP_Start()`, before DCMIPP capture begins, `main.c` now calls
`HAL_DCMIPP_PIPE_DisableISPRawBayer2RGB(&hdcmipp, DCMIPP_PIPE1)` -- turning off only the demosaic
block, leaving CSI reception, IPPlug, and the pixel packer untouched. The resulting image will
look wrong (raw Bayer samples reinterpreted as RGB565 pixels, not real colors) but that's
irrelevant to this test -- only the `[MEM_SCAN]` bitmap matters:
- **Pattern gone** -> the demosaic hardware block is the culprit; something in its internal
  line-buffering has a 32-line cycle.
- **Pattern unchanged** -> demosaic is innocent; the bug is upstream, in CSI reception or
  DCMIPP's own raw capture/write path before the ISP even touches the data.

Gated behind `#define BUG17_DEMOSAIC_BISECT_TEST 1` right at the call site (`main.c`, just after
`printf("after ISP_Start\r\n")`) -- **temporary, must be reverted (flip to 0, or delete the
block) once the test result is read; do not ship with demosaic disabled.** Rebuilt clean
(`cmake --build build/Debug`, 74.38% RAM). **Not yet re-tested on real hardware as of this
entry.**

### Next steps

1. **Flash this test build and read the new `[MEM_SCAN]` bitmap/summary.** Compare `bad_rows`
   and the row-number list directly against this entry's `{4,5,6,11,12,17,18,19,24,25,30,31}
   mod 32` baseline.
2. **If the pattern is gone (or clearly different)**: the demosaic block is confirmed as the
   cause. Next would be looking at whether `ISP_SVC_ISP_SetDemosaicing()`'s
   `DCMIPP_RawBayer2RGBConfTypeDef` fields (`PeakStrength`/`VLineStrength`/`HLineStrength`/
   `EdgeStrength` -- all currently 0 per `isp_param_conf_imx219.h`'s `demosaicing` block) or the
   Bayer pattern type (`RawBayerType = DCMIPP_RAWBAYER_RGGB`) have a documented interaction with
   a fixed-size internal line buffer.
3. **If the pattern is unchanged**: demosaic is ruled out too, and every hardware knob this
   project can configure from software will have been exhausted. At that point this becomes a
   CSI-reception/physical-layer question rather than a DCMIPP-configuration one -- worth a fresh
   look at `PHY_BT_220`'s timing margin against the IMX219's actual MIPI clock output (see
   knowledge_archive.md §3.1-3.2's method), or escalating to ST support/community with this
   session's now very precise, reproducible bad-row bitmask as supporting evidence, since no
   public report of this exact pattern was found in this session's research (see the entry
   further below).
4. **Whichever way it goes, revert `BUG17_DEMOSAIC_BISECT_TEST` to 0 before any real use** --
   this test build produces a wrong-looking image on purpose and must not be mistaken for
   forward progress on image correctness.

---

## 2026-09-28 (latest) — Memory write-back test PASSED (rules out physical memory gap); coarse 8-row scan was hiding an irregular, non-32-periodic pattern; full 480-row scan queued

**Flashed the `[MEM_TEST]` pre-capture CPU write-back check.** Result: **all 15 known-bad rows
read back `00 00 OK`** -- every single one, before DCMIPP has captured a single frame. This
conclusively rules out the "physical memory gap/alias" theory (the one shelved a few entries back
in favor of the IPPlug theories, never actually disproven until now): the destination address
range is genuine, correctly-decoded, writable RAM. Combined with the previous entry (both
`MemoryPageSize` and `MaxOutstandingTransactions` confirmed correct via register readback with
zero effect on the corruption), **all three of this project's own hypotheses for this bug are
now conclusively ruled out**: not a memory-map gap, not a page-size misconfiguration, not an
outstanding-transactions saturation issue. Whatever is happening, it happens between the CSI
receiver and the point DCMIPP's write-master decides what bytes to write -- the destination
memory and the write-master's throughput settings are both innocent.

**Re-reading this project's own earlier diagnostics more carefully turned up something the
Bug 16/17 theories had glossed over**: the very same log that shows the "clean" every-32-row
`[MEM_SCAN]` pattern (`row024, row056, row088, ...`, sampled every 8th row) *also* prints the
older, finer-grained `[WARMUP_SRC]` spot-check (rows 60/68/76/200/400, added back in the Bug 15
investigation) -- and in that same single capture, **row 68 and row 76 are both `0xFF`, while
`[MEM_SCAN]`'s neighboring samples at row 64, 72, and 80 are all clean**. A true period-32 defect
sampled correctly at those two different row sets should agree; here they don't; the "clean
32-row period" was an artifact of the coarse 8-row sampling stride aliasing a messier, more
irregular defect into a falsely tidy-looking one. The real bad-row set in that window is
something like `{56, 68, 76, 88, ...}` -- gaps of 12, 8, 12 -- not a clean arithmetic sequence.
This matters: Bug 16's entire theory (`MemoryPageSize` too small) was built on "perfectly
periodic every 32nd row" as its key supporting evidence, and that evidence was never actually
as clean as it looked once cross-checked against data this project already had sitting right
next to it in the same log.

**Diagnostic added**: replaced the 8-row-stride `[MEM_SCAN]` with a full 480-row scan (every row,
not every 8th), checking three columns per row (start/middle/end of line) instead of just column
0, so it can tell whether a bad row is corrupted end-to-end or only partially. Output is a compact
480-character `.`/`#` bitmap plus a computed summary (bad row count, how many are bad
start-to-end, and the min/max gap between consecutive bad rows) and the full list of bad row
numbers -- enough to see the actual shape of the defect (clean period vs. irregular/jittery
pattern) without doing it by hand from a coarse sample again. Rebuilt clean (`cmake --build
build/Debug`, 74.38% RAM). **Not yet re-tested on real hardware as of this entry.**

**Where this points next, pending the real data**: an irregular (non-arithmetic) bad-row pattern
that appears even with zero system load, produces clean `0xFF` fill (not scrambled/random bytes),
and never trips DCMIPP's sticky error latch a second time after the one historical `DPHY_CTRL`
event (see the D-PHY entry further below -- confirmed one-time, not recurring, again in the
latest log) is more consistent with an occasional CSI line-sync miss that DCMIPP silently fills
with idle/blank data for that one line, than with any AXI-write-master timing setting. That's a
physical-layer/CSI-timing question, not a buffering one -- but wait for the actual bad-row list
before committing to that theory; it might yet turn out to be a real period once seen without the
8-row aliasing.

### Next steps

1. **Flash and read the new `[MEM_SCAN]` bitmap and summary.** Compare `min`/`max` gap: if they're
   equal, it really is a clean fixed period after all (and the earlier row-68/76-vs-64/72/80
   disagreement needs a different explanation -- e.g. the bad-row position drifting a few rows
   between the two diagnostic blocks, which run moments apart in time on a continuously-updating
   circular buffer). If they differ noticeably, treat it as irregular/jitter, not a clean divisor
   of 32, and stop looking for a single misconfigured "every-N-lines" register.
2. Check the `full-line bad` count against the total bad count: if every bad row is bad
   start-to-middle-to-end, that's consistent with DCMIPP substituting a whole blank/idle line;
   if some are only partially bad, that points more at a mid-line glitch (e.g. a CSI short packet
   arriving late) than a whole-line substitution.
3. If the pattern is confirmed irregular/jitter rather than periodic: this is no longer an
   IPPlug/AXI-configuration question (three hypotheses in that space are now exhausted). Next
   avenue is the CSI D-PHY timing margin itself -- re-check `PHY_BT_220`'s actual margin against
   the IMX219's real MIPI clock, and consider whether `evision`'s software AWB/AE loop
   (`ISP_BackgroundProcess()`, confirmed running even during the pre-RTOS warmup loop via the
   `BGP OK` print) does anything I2C-side that could coincide with occasional line drops --
   though note this needs to explain a defect present from the very first few warmup frames, so
   any AWB/AE interaction would have to be near-immediate, not something that only appears once
   convergence logic kicks in later.
4. Keep `MemoryPageSize=256B` and `MaxOutstandingTransactions=4` as they are -- both confirmed
   correct via register readback, neither is the bug, no reason to touch them again.

---

## 2026-09-28 (later than that) — Bug 17's MaxOutstandingTransactions experiment ALSO disproven by register-confirmed hardware readback; both IPPlug levers now ruled out; researched public STM32N6/DCMIPP issue reports; new CPU-only write-back test queued

**Flashed Bug 17's experiment (`MaxOutstandingTransactions` 16 -> 4, plus the `IPGR1` readback fix)
and got a new log.** Result:
- `IPPlug CLIENT2: IPGR1=0x00000002 IPC2R1=0x00000304 IPC2R2=0x000F0000 IPC2R3=0x03FF0000` --
  decoded, this **confirms both register writes landed exactly as configured**: `IPGR1=0x02`
  is `DCMIPP_MEMORY_PAGE_SIZE_256BYTES`, and `IPC2R1=0x304` splits into the burst-size field
  (`0x04` = 128 bytes) and the outstanding-transactions field (`0x03` = the *4th* enum value,
  i.e. `DCMIPP_OUTSTANDING_TRANSACTION_4` -- matches what was requested). This closes the
  diagnostic gap from the previous entry: it is no longer a question of whether the register
  writes took hold, only whether they matter.
- **The `[MEM_SCAN]` grid is, again, byte-for-byte identical**: the same `row024, row056,
  row088, ... row472` (every 32 rows) still read back as `0xFF`, now against a `0x8210`
  background instead of `0xC210`/`0xA210` (just normal frame-to-frame AWB/scene variation,
  irrelevant to the bug). A user-supplied photo of the actual UVC-streamed image over the USB
  connection shows this directly: **regular horizontal white/dark banding across the entire
  frame**, confirming this is a real, visually-obvious defect, not a debug-print artifact.

**This rules out `MaxOutstandingTransactions` too, with the same rigor Bug 16 lacked the first
time**: two independent IPPlug parameters, both now verified correct in hardware via direct
register readback, and the corruption pattern has not moved by a single byte or row across
either change. The write-master's configured timing/throughput parameters are conclusively not
the cause. (`MemoryPageSize=256B` and `MaxOutstandingTransactions=4` are left as-is going
forward -- neither is wrong, they're just proven irrelevant to this bug.)

**Researched public STM32N6/DCMIPP issue reports for a matching case** (web search + ST
community forum + official ST/OpenMV GitHub repos), since this project's own two hypotheses
were exhausted:
- ST's own official examples (`STM32CubeN6` DCMIPP_ContinuousMode, and the `stm32-mw-camera` /
  `stm32-mw-isp` middleware libraries ST ships for exactly this kind of PIPE1+ISP+UVC use case)
  **never call `HAL_DCMIPP_SetIPPlugConfig()` at all** -- not for PIPE0, not for PIPE1. Neither
  does OpenMV's STM32N6 port. This doesn't prove IPPlug config is unnecessary in general (this
  project's own Bug 1 needed it to fix a real overrun, confirmed on hardware), but it does mean
  there's no publicly available reference implementation actually exercising these specific
  fields to compare parameter choices against.
- The official STM32N6xxxx errata sheet (ES0620) has exactly one DCMIPP-related entry found: a
  VENC/DCMIPP synchronization issue in streaming mode (unrelated -- VENC is the H.264 video
  encoder block, not used by this project's JPEG-based UVC pipeline).
- ST Community forum threads with superficially similar symptoms ("DCMIPP Horizontal Distortion
  Beyond Certain Line Length", a Pipe0 "data not written to RAM" case) turned out to have
  unrelated root causes (sensor misconfiguration in one case, missing RIF/RIMC security
  attributes in the other -- the RIF one is structurally identical to this project's own,
  already-fixed Bug 2, for a different peripheral). None matched a periodic every-Nth-row
  pattern.
- No public report of a fixed-period (every-32-lines) DCMIPP PIPE1/ISP corruption was found.
  This appears to be a corruption pattern specific to this project's configuration rather than
  a widely-hit issue with a known public fix.

**Next experiment, cheaper and more decisive than another IPPlug guess**: rule out the
possibility that these specific destination byte ranges simply aren't real, writable memory --
independent of DCMIPP entirely. `Camera_CheckFrameBuffer`'s pre-capture `memset(camera_
framebuffer, 0x00, FRAME_BUFFER_SIZE)` (main.c, "Preparing frame buffer") is flushed to physical
RAM by the existing `SCB_CleanDCache_by_Addr()` call right after it. Added a new `[MEM_TEST]`
block immediately after that flush -- before DCMIPP has captured a single frame -- that
invalidates the cache and reads back byte 0/1 of every previously-identified bad row (24, 56,
88, ... 472). If those bytes come back as anything other than `0x00` (the value the CPU itself
just wrote and flushed), DCMIPP was never at fault for those specific rows -- the address range
itself doesn't hold a plain write the way normal RAM does, which would point at a physical
memory-map gap/alias in that separate SRAM bank (`CAMERA_BUFFER_ADDR = 0x34200000`, chosen
specifically to sit outside the linker's normal 2047K RAM region -- see the "physical memory
gap" entry further below in this file for the address-map reasoning, which was shelved in favor
of the IPPlug theories but never actually disproven this directly). If they DO read back `0x00`
correctly, the address range is confirmed genuinely writable, and the bug is conclusively
somewhere in the DCMIPP/ISP hardware pixel pipeline itself (demosaic/packer stage) rather than
anywhere in the write path or the destination memory -- worth checking `ISP_MW`'s stat-area
config next in that case (`hcamera_isp.statArea` prints `XSIZE=0 YSIZE=0` after `ISP_Init()`,
traced to `imx219`'s IQ-param config header never filling in `statAreaStatic` -- currently
believed unrelated to the main pixel path since statistics and main image conversion are
separate concerns on this IP, but worth a second look if the write-back test comes back clean).

Rebuilt clean (`cmake --build build/Debug`, 74.31% RAM used). **Not yet re-tested on real
hardware as of this entry.**

### Next steps

1. **Flash and read the new `[MEM_TEST]` block's output**, printed right after `Testing
   framebuffer CPU write...` / `Before capture: ...`, before any DCMIPP activity starts. Every
   line should say `OK`; any line saying `*** STUCK, NOT WRITABLE ***` identifies this as a
   memory-map problem, not a DCMIPP problem, and exactly which rows.
2. **If all rows read `OK`** (confirming the memory is fine and it really is a DCMIPP/ISP
   write-side defect): stop tuning IPPlug parameters (two are now ruled out) and look at the
   pixel-pipe/ISP hardware stage directly -- specifically whether `pPipeConfig` (main.c,
   `MX_DCMIPP_Init()`) needs an explicit crop/decimation/downsize config even when passing
   through at native resolution (this project currently configures none at all -- no
   `HAL_DCMIPP_PIPE_SetDownsizeConfig`/`SetCropConfig` call exists), or whether the ISP
   demosaic block itself has a line-buffer depth around 32 lines that something needs to be
   told about explicitly.
3. **If any row reads stuck/non-zero**: that's the real bug, and it has nothing to do with
   DCMIPP configuration at all -- next step would be binary-searching the exact byte-address
   boundaries of the non-writable region (similar to the superseded "physical memory gap"
   entry's approach) to characterize it, then either avoid placing frame data there or find the
   correct usable address range for this buffer.

---

## 2026-09-28 (even later) — Bug 16's fix disproven by hardware; readback gap found; Bug 17 experiment queued

**Flashed Bug 16's fix (`MemoryPageSize` 64B -> 256B) and got a new log.** Result: the
`[MEM_SCAN]` grid is **byte-for-byte identical** to the log that motivated Bug 16 in the first
place -- the exact same rows (`row024, row056, row088, row120, row152, row184, row216, row248,
row280, row312, row344, row376, row408, row440, row472`, every one exactly 32 rows apart) still
read back as `0xFF`. Quadrupling the page size changed **nothing observable**. This falsifies
Bug 16's theory as the (or at least the whole) root cause -- it was a plausible-sounding
explanation derived from reading the field's documentation, but it was never actually confirmed
on hardware before being written up as "the fix," and now that it has been tested, the evidence
says no.

**Found a real gap while investigating why the fix apparently did nothing**: the confirmation
printf added right after `HAL_DCMIPP_SetIPPlugConfig()` (`"IPPlug CLIENT2: IPC2R1=... IPC2R2=...
IPC2R3=..."`) only ever read back `IPC2R1`/`IPC2R2`/`IPC2R3` -- the **per-client** registers.
`MemoryPageSize` is written to `IPGR1`, a **global** register (see
`HAL_DCMIPP_SetIPPlugConfig()` in `stm32n6xx_hal_dcmipp.c`: `hdcmipp->Instance->IPGR1 =
(pIPPlugConfig->MemoryPageSize);`, completely separate from the `switch(Client)` block that
writes `IPC2Rx`). So Bug 16's fix was flashed and declared "took effect" without the printf ever
actually being capable of showing whether the `IPGR1` write held, was rejected, or was silently
reset by something else. This is the same category of mistake as Bug 9 (a fix assumed to have
landed in hardware, undone by something not checked) -- except this time it was caught by
re-reading the confirmation code itself rather than by another register dump, worth remembering:
when a printf claims to "confirm X took effect," check that it's actually reading the register X
is written to, not merely a register in the same function call.

**Fix / next experiment (bundled into one flash to save a round-trip)**:
1. Added `IPGR1` to the confirmation printf (`main.c`, `MX_DCMIPP_Init()`, `USER CODE BEGIN/END
   DCMIPP_Init 2`) -- next log will show definitively whether the page-size write is even
   landing in hardware at all.
2. Per this project's own contingency plan (see the superseded entry below, "Next steps" item
   3): since the page-size lever produced zero change, tried the other IPPlug knob next --
   `MaxOutstandingTransactions` cut from 16 down to 4. Hypothesis: 16 simultaneous in-flight
   128-byte AXI writes may be more than this specific destination bank's own internal write
   queue can sustain, and it may be silently dropping/corrupting writes on some fixed period of
   its own once saturated -- a mechanism that has nothing to do with `MemoryPageSize` and would
   explain why that change had no effect. `MemoryPageSize` is left at 256 bytes (still
   theoretically more correct than 64, even if not the culprit).

Rebuilt clean (`cmake --build build/Debug`, 74.29% RAM used, same as before -- this change adds
no new statically-allocated memory). **Not yet re-tested on real hardware as of this entry.**

### Next steps

1. **Flash and read the new `IPGR1=...` line.** If it does NOT show `0x00000002` (the
   `DCMIPP_MEMORY_PAGE_SIZE_256BYTES` encoding), the write itself is the problem (rejected,
   reset by an assert/param check, or overwritten by something not yet found) -- chase that
   before touching any more IPPlug fields. If it DOES show the expected value, the register
   write is confirmed correct and `MemoryPageSize` can be fully ruled out as a lever.
2. **Check the `[MEM_SCAN]` grid again.** If the periodic `0xFF` rows are gone (or shifted to a
   different period/phase) with `MaxOutstandingTransactions` at 4, that confirms it as the real
   lever -- then it's worth trying values between 4 and 16 to find the actual threshold rather
   than leaving it at the most conservative value untuned.
3. **If the pattern is still byte-for-byte identical even with `MaxOutstandingTransactions=4`**,
   both of this project's IPPlug-config theories (Bug 16 and this one) are wrong, and the
   corruption is not an IPPlug/AXI-write-master timing issue at all -- worth stepping back to
   re-examine whether it's something in the ISP hardware pipe itself (Bayer2RGB/packer stage)
   rather than the write-master feeding it, since nothing on the write side has moved the
   needle twice in a row now.
4. Once genuinely fixed and confirmed: same follow-ups as before (evaluate AWB/AE color
   correctness, decide whether `Camera_README.md`/`knowledge_archive.md` need a new bug entry).

---

## 2026-09-28 (later still) — Bug 16 found: MemoryPageSize < burst size, causing periodic corruption

**Superseded by the entry above** — this fix was flashed and the periodic corruption came back
byte-for-byte identical, so `MemoryPageSize` was not the (sole) root cause. Left below for the
reasoning trail, but don't treat "Fix" in this entry as applied/effective any more.

**Result of the full 480-row memory scan**: overturns the previous entry's "physical memory
gap" theory completely, and replaces it with something much more precise and fixable. The bad
rows are **perfectly periodic**: `row024, row056, row088, row120, row152, row184, row216,
row248, row280, row312, row344, row376, row408, row440, row472` -- every one exactly 32 rows
after the last, with zero drift, across the entire 480-row frame. A physical memory gap would be
one contiguous bad region; this is a fixed-period artifact, which points squarely at DMA/FIFO
burst timing rather than an address-map problem. It also explains why nothing ever caught this
before: every diagnostic this whole project has ever printed only checked row 0, and row 0
(0 mod 32) is never in the corrupted phase -- so a bug present since this project's very first
camera bring-up looked completely invisible by pure coincidence of which row got checked.

**Root cause, found by reading the IPPlug config struct's field documentation directly**:
```c
pIPPlugConfig.MemoryPageSize = DCMIPP_MEMORY_PAGE_SIZE_64BYTES;   /* 64 bytes  */
pIPPlugConfig.Traffic        = DCMIPP_TRAFFIC_BURST_SIZE_128BYTES; /* 128 bytes */
```
`MemoryPageSize` describes the memory-side page/row boundary IPPlug must not let a single AXI
burst straddle -- and it was configured **smaller than the burst size itself** (64B page vs.
128B burst). This is backwards from how the field is meant to be used, and has been this way
since this exact IPPlug config was first written -- this session's very first bug fix (the
original PIPE1 overrun fix, months before UVC existed). A burst larger than its own declared
page boundary is exactly the kind of AXI-write-master misconfiguration that produces a
fixed-period corruption pattern (some internal page-boundary/counter logic interacting
incorrectly with bursts that violate its own stated constraint), matching every piece of
evidence gathered: present with zero system load (it's not about USB/JPEG contention at all,
Bugs 13-15's investigation direction), completely invisible to any DCMIPP status/error register
(P1SR/CMSR2/ErrorCode all read clean the entire time, because nothing is actually erroring at
the interface level -- the corruption happens in how the write-master paces its own bursts
against a boundary it was told incorrectly), and periodic rather than a one-time event.

**Fix**: `MemoryPageSize` changed from 64 bytes to 256 bytes (double the 128-byte burst size,
for headroom -- page size must be `>=` burst size). Rebuilt clean. **Not yet re-tested on real
hardware as of this entry.**

**Why this took so long to find, and the general lesson**: this bug has almost certainly been
present since this project's very first working camera capture, silently, for the project's
entire history -- it was never a regression introduced by the UVC work. It stayed invisible
because every verification this project ever did (`Camera_CheckFrameBuffer()`'s non-zero count
and checksum, the "first 64 bytes" print, even this session's own earlier register polls) either
couldn't distinguish valid data from `0xFF` filler, or happened to only ever sample row 0 -- a
row that, by the exact phase of this 32-row-periodic bug, was *always* going to look fine. The
UVC pipeline didn't introduce a new bug; it was simply the first thing in this project's history
that rendered enough of the frame, visually, for a human to notice something was wrong. Worth
remembering for any future large-buffer/streaming-data correctness check on this project: sample
many rows/offsets across the *whole* structure, not just the beginning, and don't rely on
"non-zero" as a proxy for "correct."

### Next steps

1. **Flash and re-test.** Check the `[MEM_SCAN]` grid again first -- confirm the periodic `0xFF`
   rows are gone entirely with the corrected `MemoryPageSize`. Then check the UVC-streamed image
   in a viewer for a clean, non-striped picture.
2. Once confirmed clean: finally evaluate actual color correctness -- the original point of this
   entire feature, now hopefully reachable after Bugs 7-16.
3. If the periodic pattern persists (even if shifted to a different period or phase), that would
   mean `MemoryPageSize` wasn't the whole story -- next place to look would be
   `MaxOutstandingTransactions` (currently 16) in combination with the corrected page size, since
   an outstanding-transaction count that's too aggressive relative to the (now-larger) page size
   could produce a similar-looking artifact at a different period.
4. If confirmed fixed: consider whether this same misconfiguration ever affected the original
   single-shot capture's practical use (informational only at this point, no action needed,
   since Camera_README.md's documented PIPE1 fix is otherwise unaffected by this specific bug)
   and whether `Camera_README.md`/`knowledge_archive.md` should note this as an additional,
   distinct bug from the original IPPlug-FIFO-depth issue they already document.

---

## 2026-09-28 (yet later) — Major pivot: this is a fixed physical-memory gap, not a capture bug

**Result of the warmup-loop dump -- the decisive experiment**: with ZERO USB/JPEG/RTOS load
whatsoever (this printed before ThreadX even starts):
```
row60:  03 19 03 19 ...   (real data)
row68:  FF FF FF FF ...   (garbage)
row76:  FF FF FF FF ...   (garbage)
row200: A2 10 82 10 ...   (real data again!)
row400: 82 10 82 10 ...   (real data again!)
```
**This changes the entire picture.** It is not "DCMIPP stops writing partway through the
frame" (which would mean everything from ~row 68 onward is bad). It is a **localized band of
bad memory** somewhere between row ~68 and row 200, with genuinely good data both before and
after it. This is present with no RTOS, no USB, no JPEG running at all -- ruling out system
load/AXI contention as the cause completely (the previous entry's controlled experiment did its
job).

**New hypothesis, and a strong one**: `camera_framebuffer` lives at a hardcoded fixed address,
`CAMERA_BUFFER_ADDR = 0x34200000` (`main.c`) -- not a linker-allocated array. Checked
`STM32N657X0HXQ_LRUN.ld`: the linker's own `RAM` region is `ORIGIN = 0x34000400, LENGTH =
2047K`, which ends at exactly `0x34200000`. So this buffer was deliberately placed in a
*different, separate physical RAM bank* from the one everything else (`.bss`, `.data`, all the
ThreadX/USBX/JPEG buffers) lives in -- almost certainly to avoid eating into that already-tight
2047K budget (confirmed: `main.c`'s DCMIPP MSP init explicitly enables clocks for
`AXISRAM2`/`AXISRAM3`/`AXISRAM4` as three distinct banks). The new working theory: this second
bank (or the boundary between it and the next one) is not as large, or not as contiguous, as
whoever chose this address assumed -- part of this 614400-byte buffer may fall into a genuine
gap between two physical SRAM blocks, and unbacked address space on this chip reads back as
`0xFF` rather than faulting. This would explain everything with no exotic behavior required:
DCMIPP writes the full frame correctly (matching every clean status register this whole
investigation found), but part of what it writes lands in memory that silently doesn't exist.

**This also reframes the entire debugging history**: this same corruption may have been present
since long before this session's UVC work even started -- `Camera_CheckFrameBuffer()`'s checks
(non-zero byte count, checksum, first-64-bytes print) can never distinguish valid varied pixel
data from a block of `0xFF`, since both count as "non-zero," and nothing ever printed deeper
into the buffer before now. The UVC pipeline didn't introduce this bug; it's the first thing
that ever visually displayed enough of the frame to make it obvious.

**Diagnostic added**: a full memory scan across all 480 rows (every 8th row, 2 bytes each) in
the same warmup-loop location, to empirically map the exact boundaries of the bad region instead
of guessing from remembered reference-manual bank sizes. Rebuilt clean. **Not yet re-tested on
real hardware as of this entry.**

### Next steps

1. **Flash and read the `[MEM_SCAN] row: first-byte ...` grid.** This will show, row by row,
   exactly where the transition from real data to `0xFF` happens, and where it transitions back
   to real data -- giving the precise byte-address boundaries of the gap (each row = 1280 bytes,
   so `bad_start_row * 1280` and `bad_end_row * 1280`, offset from `0x34200000`).
2. **Once the boundaries are known**, the fix is straightforward: either (a) shrink
   `CAMERA_BUFFER_ADDR`'s usable region to stay entirely within the first valid bank and put
   `video_buf[1]` (already a normal linker-placed array) somewhere safe, keeping the *cropped*
   640x360 region (which is what's actually streamed) away from the bad band if possible, or
   (b) find the correct start address for whatever bank begins after the gap and use *that* for
   the buffer instead of `0x34200000`, or (c) if the gap is small and known-fixed, avoid
   placing frame data across it entirely by adjusting the buffer's start address or size.
3. Consider whether this same gap affects `video_buf1` -- it's a plain linker-placed array
   inside the *known-good* 2047K region, so it should be unaffected, but worth confirming with
   the same row-scan technique once buf_idx cycles to it, if the fix doesn't make this moot.
4. This is a good example of why "first N bytes look fine" and "byte count is non-zero" are weak
   correctness checks for a large buffer -- worth remembering for any future large-buffer
   verification on this project (see knowledge_archive.md's general lessons, this is a strong
   fourth example of "success" that wasn't actually being tested rigorously enough to catch a
   real problem).

---

## 2026-09-28 (still later) — D-PHY error was one-time historical; controlled experiment added

**Result**: after clearing `hdcmipp.ErrorCode` each poll, it read `0x00000000` on every single
subsequent poll across thousands of frames -- the `DPHY_CTRL` bit was a one-time historical
event (most likely latched once during initial CSI lock-on, long before UVC streaming), **not**
a recurring physical-layer fault. Ruled out cleanly. `CSI->SR0`/`SR1` also stayed stable and
error-free throughout. So: no overrun, no pipe error state, no D-PHY fault, no error flag of any
kind, anywhere -- and the image is still corrupted identically. Every hypothesis grounded in "the
hardware is reporting a problem" has now been exhausted.

**Controlled experiment added** to isolate the one variable not yet tested: is this truncation
inherent to `DCMIPP_MODE_CONTINUOUS` capture itself, or specific to running under RTOS+USB+JPEG
system load? `main.c`'s pre-RTOS warmup loop already does dozens of continuous-mode PIPE1
captures into this exact same `camera_framebuffer` address, with **zero** USB/JPEG DMA activity
competing for AXI bandwidth (ThreadX/USBX haven't started yet at that point in `main()`). Added
the same row-60/68/76 dump there, plus row 200 and row 400 for a fuller picture, right after the
warmup loop exits. If row 68+ is already `0xFF` there too, AXI/USB/JPEG contention is not the
cause -- something about continuous-mode capture itself (independent of system load) truncates
the write, and the investigation needs to look at what's structurally different between
`DCMIPP_MODE_SNAPSHOT` (which reliably fills the whole 480-row buffer, confirmed every boot) and
`DCMIPP_MODE_CONTINUOUS` (which apparently doesn't, going by every UVC-side dump so far). If the
warmup loop's data is clean, contention during RTOS+USB+JPEG operation is confirmed as the
proximate cause, and the investigation moves to AXI bus arbitration/priority (checked in passing
this entry: `RIMC_MasterConfig_t` only has `MasterCID`/`SecPriv` fields, no priority/QoS knob --
RIF isn't where such a setting would live, if one exists at all on this chip). Rebuilt clean.
**Not yet re-tested on real hardware as of this entry.**

### Next steps

1. **Flash and read the new `[WARMUP_SRC] row60/68/76/200/400 ...` lines** (printed once, right
   after "BGP OK" or the warmup timeout message, well before "starting ThreadX/USBX UVC
   pipeline"). This is the decisive test:
   - All five rows show real, non-`FF` data → truncation is specific to running under
     RTOS+USB+JPEG load. Next: investigate AXI bus arbitration/priority mechanisms beyond RIF
     (possibly a NIC/bus-matrix QoS register elsewhere in the reference manual), or try
     deliberately reducing JPEG/USB activity (e.g. lower JPEG quality/resolution, or a slower
     UVC frame rate) to see if the corruption's extent changes proportionally with load.
   - Row 68+ is already `0xFF` even here, with no USB/JPEG contention at all → the bug is
     structural to continuous-mode capture on this pipe configuration, unrelated to system load.
     Next: diff every `HAL_DCMIPP_PIPE_SetConfig`/`SetIPPlugConfig`/ISP-related call between this
     project's snapshot path (works) and continuous path (doesn't) for anything that might only
     apply correctly to single-frame captures.
2. Either outcome is genuine forward progress -- this finally separates "software/RTOS-load
   problem" from "DCMIPP continuous-mode configuration problem," which every register poll so far
   has failed to distinguish.

---

## 2026-09-28 (later) — Overrun hypothesis ruled out; found a CSI D-PHY error bit instead

**Result of the register poll**: `PipeState[1]=2` = `HAL_DCMIPP_PIPE_STATE_BUSY` (normal --
NOT `HAL_DCMIPP_PIPE_STATE_ERROR=4`), and `P1SR`/`CMSR2` show no overrun flag set (`P1SR` reads
`0x00000007`/`0x00020007` -- just `LINEF|FRAMEF|VSYNCF` and occasionally `LSTFRM`, never `OVRF`
at `0x80`). **This rules out the previous entry's overrun-gets-permanently-masked hypothesis
cleanly** -- good, ruled out with real evidence rather than chased further on a guess.

**But `hdcmipp.ErrorCode = 0x00008000` is set, every single poll, and that bit is
`HAL_DCMIPP_CSI_ERROR_DPHY_CTRL`** ("Error Control on data line (0 OR 1)", from
`stm32n6xx_hal_dcmipp.h`) -- a CSI D-PHY *lane*-level control error, a completely different
subsystem from DCMIPP's pipe/buffering logic. Since `hdcmipp.ErrorCode` is only ever OR'd into
by the HAL driver and never cleared automatically, this raises an important possibility: **this
error may have been present since much earlier in this session, including during the
"successful" single-shot captures** -- `Camera_CheckFrameBuffer()`'s checks (non-zero byte
count, checksum) cannot distinguish valid varied pixel data from a block of `0xFF` filler, since
both count as "non-zero." The single-shot dump's own "first 64 bytes" print only ever showed row
0, never anything deeper into the frame, so this exact kind of partial corruption could have
gone completely unnoticed there too.

**Diagnostic added**: the periodic poll now also prints `CSI->SR0`/`SR1` (the live D-PHY lane
status registers, not a sticky latch) and explicitly resets `hdcmipp.ErrorCode` to
`HAL_DCMIPP_ERROR_NONE` after each read -- this will show whether `DPHY_CTRL` is a one-time
historical event (in which case it won't reappear on the next poll) or something actively
recurring during streaming (in which case it reappears every second, pointing at a real,
ongoing physical-layer signal integrity issue rather than anything buffering/software can fix).
Rebuilt clean. **Not yet re-tested on real hardware as of this entry.**

**Why this matters going forward**: if this turns out to be a genuine, recurring D-PHY lane
error, the investigation shifts from "software/buffering bug" (Bugs 7-14's territory) to
"physical-layer signal integrity" -- a materially different class of problem that may point at
CSI bitrate/clock margin (`DCMIPP_CSI_PHY_BT_220`, configured based on the IMX219's actual MIPI
clock -- see knowledge_archive.md §3.1-3.2), cable/connector integrity, or PHY calibration,
rather than something fixable with another register-configuration change in this project's own
code.

### Next steps

1. **Flash and read the new `[DCMIPP_DIAG] CSI SR0=... SR1=...` line, and watch across several
   consecutive 1-second polls whether the D-PHY_CTRL error bit (0x8000) comes back after being
   cleared.** If it keeps reappearing every single poll, treat this as a real, recurring D-PHY
   lane fault, not a one-time historical artifact.
2. If confirmed recurring: decode `CSI->SR0`/`SR1` bit-by-bit against `stm32n657xx.h` (same
   method used to root-cause Bug 4/D in the original camera bring-up, see
   knowledge_archive.md §3.2) to identify exactly which lane and what condition is erroring.
   Compare against `Camera_N6_AI_Test`'s CSI config once more, specifically the `PHYBitrate`
   setting and any D-PHY timing/calibration parameters, in case something about running under
   heavier system load (RTOS + USB + JPEG all active) is exposing a marginal timing
   configuration that happened to just barely work before.
3. If it does NOT reappear (confirmed one-time historical event): the corruption has some other
   cause after all, and the next step is re-examining whether the `[UVC_SRC]` dump's "row 60 has
   data, row 68+ is 0xFF" pattern lines up with some other DCMIPP register (crop, decimation,
   or line-count configuration) that might be limiting the actual captured height to something
   far short of 480 rows.

---

## 2026-09-28 — Bug 15 found via the source-byte dump; root cause narrowed to DCMIPP overrun

**Result of the raw-byte diagnostic**: conclusive, and points away from conversion/encoding
entirely. Across every single sample over minutes of continuous streaming: the crop's first row
(row 60 of the full frame) always shows real, *varying* pixel data (changes sample to sample --
genuinely live sensor content, e.g. `03 19 23 19 23 19...`), while **row 68 and row 76 (8 and 16
rows into the crop) are `FF FF FF FF...` -- every single byte, every single sample, no
exceptions.** DCMIPP is only writing the first handful of lines of each "completed" frame; the
rest of the buffer is untouched garbage. This rules out `CVT_CvtRgb565ToMcu422`/JPEG-encoding
bugs (there's no valid data downstream of row ~60-67 for it to mis-convert) and rules out a
buffering race (Bugs 13/14 were barking up the wrong tree the whole time -- not their fault
given the evidence available then, but confirmed moot now). The real bug is DCMIPP itself
failing to complete each frame's write.

**Working hypothesis, from reading the HAL driver source (not guessed from the reference
project)**: `stm32n6xx_hal_dcmipp.c`'s `HAL_DCMIPP_IRQHandler()` handles a PIPE1 overrun by
**permanently disabling `DCMIPP_IT_PIPE1_OVR`** (`__HAL_DCMIPP_DISABLE_IT(...)`) and setting
`hdcmipp->PipeState[1] = HAL_DCMIPP_PIPE_STATE_ERROR` -- the very first time it fires, for the
rest of the program's life (nothing ever re-enables it). Meanwhile `PIPE1_FRAME` (frame-complete)
is a *separate* interrupt source, checked independently, and keeps firing on its own schedule
regardless. `app_threadx.c`'s `HAL_DCMIPP_PIPE_ErrorCallback()` only increments `ovr_count` `if
(uvc_capture_active)` -- a flag not set to 1 until just after `HAL_DCMIPP_CSI_PIPE_Start()`
returns. If the very first overrun happens in that brief window (highly plausible: continuous
capture now competes for AXI bandwidth with USB HS's DMA and the JPEG hardware encoder's DMA,
neither of which existed when this project's original IPPlug/FIFO config -- from this session's
very first bug fix, months before UVC existed -- was tuned), `ovr_count` would never increment,
staying at 0 forever, exactly matching every log this entire debugging session. The
frame-complete interrupt would keep firing normally every ~32ms (matching the smoothly climbing
`frame_count`), while the actual DMA write silently fails to complete past the first few lines
each time, forever, because the pipe is stuck in an unrecovered error state that nothing ever
clears.

**Diagnostic added** (no fix yet -- this hypothesis needs direct register confirmation before
acting on it): `app_threadx.c`'s periodic 1Hz print now also reads and prints
`hdcmipp.PipeState[DCMIPP_PIPE1]`, `hdcmipp.ErrorCode`, `DCMIPP->P1SR`, `DCMIPP->CMSR2`, and
`DCMIPP->CMSR1` directly -- bypassing the interrupt/callback path (and its `uvc_capture_active`
guard) entirely, so this can't miss anything the callback-based counter could. Rebuilt clean.
**Not yet re-tested on real hardware as of this entry.**

### Next steps

1. **Flash and read the new `[DCMIPP_DIAG] ...` line.** If `PipeState[1]` reads `4`
   (`HAL_DCMIPP_PIPE_STATE_ERROR`, check the enum in `stm32n6xx_hal_dcmipp.h`) and/or
   `ErrorCode` has `HAL_DCMIPP_ERROR_PIPE1_OVR` set, this confirms the hypothesis above --
   overrun happened once, early, and was never recovered from.
2. **If confirmed**, the fix needs to do two things: (a) actually address the root AXI-bandwidth
   contention (revisit the IPPlug config from this project's very first bug fix -- burst size,
   outstanding transactions, FIFO pool -- now that USB+JPEG DMA are also competing for the same
   bus, not just DCMIPP alone), and (b) make the pipe self-recovering instead of permanently
   wedged after one overrun -- e.g. detect `PipeState==ERROR` in the capture thread and
   explicitly `HAL_DCMIPP_CSI_PIPE_Stop()` + re-`Start()` (which should reset `PipeState` back to
   `READY` and re-enable the OVR interrupt), rather than relying on the driver's own (apparently
   nonexistent) automatic recovery.
3. If `PipeState`/`ErrorCode` do NOT show an overrun/error condition, this hypothesis is wrong
   and the truncation has some other cause at the DCMIPP register level -- would need to look at
   `P1FCTCR`/`P1PPCR` and the IPPlug registers (`IPC2R1/2/3`) directly next, the same way Bug 9
   was root-caused.

---

## 2026-09-27 (still yet later) — Bug 14's fix confirmed insufficient; likely not a buffering bug at all

**Result after flashing Bug 14's mutual-exclusion fix**: USB/UVC streaming statistics are now
fully healthy and stable over thousands of frames (`[UVC] fps=32 payloads=1576 done_cb=1576
drops=0`, `[UVC] Activated: speed=HS`, clean `SET_CUR`/`GET` control exchanges, no more stalls
in `PayloadDone max_gap`). **But the displayed image is still striped, and looks visually
identical to before Bug 14's fix.**

**Re-assessing the symptom itself**: a real read/write race (which is what Bugs 13/14 targeted)
should produce a torn image that looks *different* from frame to frame, and worse under load —
not a perfectly regular, repeating band pattern that looks the same across thousands of
captures regardless of whether real mutual exclusion is in place or not. That regularity is a
strong signal this was likely never a buffering/timing bug in the first place — Bug 13 and 14
were reasonable hypotheses given the evidence at the time (an unverified assumption that transfer
timing was the cause), but the persistence of an unchanged pattern after fixing the actual race
means the real bug is elsewhere, most likely in the RGB565→YUV422 conversion
(`CVT_FormatRgb565ToYuv422Jpeg`/`CVT_CvtRgb565ToMcu422` in `app_cvt.c`) or JPEG MCU encoding
path -- both of which use a function (`CVT_CvtRgb565ToMcu422`) that existed unused and
*unexercised* in the reference project (it never had an RGB565 source to feed it), so it's
never actually been proven correct on real data, unlike the sibling YUV422/RGB888 converters.

Ruled out in passing while re-checking: `camera_framebuffer`'s fixed address
(`CAMERA_BUFFER_ADDR = 0x34200000`) sits exactly at the end of the linker's `RAM` region
(`ORIGIN 0x34000400, LENGTH 2047K` → ends at `0x34200000`) -- initially looked like a possible
memory-aliasing bug (a fixed-address buffer the linker doesn't know about, potentially
overlapping other statically-placed buffers like `video_buf1`/`mcu_buffer`). Confirmed this is
intentional, not a bug: `0x34200000` is a separate physical RAM bank (AXISRAM3/4, clocked
explicitly in `stm32n6xx_hal_msp.c`'s DCMIPP init) from the AXISRAM2 region the linker's `RAM`
covers, so there's no actual overlap with linker-placed `.bss`/`.data` buffers.

**Diagnostic added** (no fix yet -- need real evidence before guessing further): a throttled
(~once/2s) raw byte dump in `ux_device_video.c`'s `fill_uvc_payload()`, printing the first 32
bytes of three different MCU-row-aligned offsets (row 60, row 68, row 76 of the source buffer --
the start of the crop and the next two JPEG MCU rows) right before `JPG_Encode()` is called.
Comparing this against `main.c`'s existing single-shot `Camera_CheckFrameBuffer()` dump (which
showed a uniform `82 10` pattern, consistent with a genuinely dark/uniform scene) will show
whether the striping is already present in the raw RGB565 data DCMIPP/ISP wrote (upstream bug --
capture or crop-offset math) or only appears after conversion/encoding (downstream bug -- the
untested `CVT_CvtRgb565ToMcu422` function, or the HW JPEG encoder's MCU-format expectations).
Rebuilt clean. **Not yet re-tested on real hardware as of this entry.**

### Next steps

1. **Flash and read the new `[UVC_SRC] ...` lines.** If all three rows show the same repeating
   pattern (like the single-shot dump's uniform `82 10 82 10...`), the bug is downstream in
   conversion/encoding -- focus on `CVT_CvtRgb565ToMcu422`'s bit-unpacking (is
   `DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1`'s actual bit layout R:15-11/G:10-5/B:4-0 as assumed, or
   does the `_1` vs `_2` suffix indicate a byte-order difference this function doesn't account
   for?) or the generic `CVT_FormatToYuv422Jpeg` MCU-block layout HAL_JPEG_Encode expects. If the
   three rows already look different/patterned from each other in a way that doesn't match a
   uniform dark scene, the bug is upstream -- re-check the crop offset math and DCMIPP pipe
   config.
2. Do not keep iterating on buffering/timing theories for this specific symptom -- that avenue
   is now reasonably exhausted (Bugs 13 and 14 both addressed real, legitimate concerns, but
   neither changed this symptom at all).

---

## 2026-09-27 (yet later) — Bug 14: Bug 13's DBM fix didn't work; real mutual exclusion needed

**Symptom after flashing Bug 13's DCMIPP-DBM fix**: image tearing was "identical" — no visible
improvement. USB streaming itself remained solid and reliable (`[UVC] fps=6 payloads=294...`
repeating cleanly for thousands of frames, no more enumeration issues at all — Bugs 7-12 are
holding up fine under sustained use).

**Why Bug 13's fix didn't work — re-examining the reasoning, not just re-guessing**: DCMIPP's
own DBM hardware double-buffering alternates addresses on a fixed ~32ms cadence, completely
independent of the consumer. `ready_buf_idx` (set from `P1SR.LSTFRM` in `Capture_
OnFrameComplete()`) only ever reflects "the most recently completed buffer" — by the time
`ux_device_video.c`'s `fill_uvc_payload()` samples it to start a new JPEG encode (which happens
async, roughly every ~166ms, whenever the *previous* frame finishes transmitting over USB — see
the `fps=6` rate), that buffer could have anywhere from ~0ms to ~32ms left before DCMIPP cycles
back and starts overwriting it again, uniformly distributed, since nothing synchronizes the
encoder's start time to DCMIPP's own frame cadence. With a measured ~27ms encode time, the
encode's read window is longer than the remaining safety margin roughly 27/32 ≈ 84% of the
time. **DBM has no mechanism for software to tell hardware "wait, I'm still reading that
buffer"** — it just keeps swapping on its own fixed schedule regardless of consumer state. This
is why the fix, though logically sound as "real double buffering," didn't actually solve the
race: alternating on a timer isn't the same as mutual exclusion.

**Re-examining Bug 8, and finding it was based on a wrong conclusion**: this is the important
part. Bug 8 originally used exactly the right design (reactive `HAL_DCMIPP_PIPE_
SetMemoryAddress()` after each frame + a `uvc_locked_buf_idx` check to skip the redirect when
UVC has a buffer locked, ported faithfully from `Camera_N6_AI_Test`'s own working code) — real
mutual exclusion, the correct fix. It was abandoned because, at the time, it appeared to make
PIPE1 stop generating frame events after exactly 1 frame. **But Bug 9 — found and fixed
*afterward*, independently — was the actual cause of that exact same "stuck at 1 frame"
symptom** (`HAL_DCMIPP_CSI_PIPE_Start()`'s `P1FCTCR |= CaptureMode` never clearing the
`CPTMODE` bit main.c's single-shot SNAPSHOT verification leaves set). Bug 8's original
ping-pong design was never actually broken; it was tested at a time when a completely separate,
still-undiscovered bug (Bug 9) made it look broken. Once Bug 9 got fixed on its own merits
(for the single-fixed-buffer design), nobody went back to check whether the original ping-pong
design would now work too.

**Fix**: restored Bug 8's original reactive ping-pong + `uvc_locked_buf_idx` mutual-exclusion
design in `app_threadx.c` (single-address `HAL_DCMIPP_CSI_PIPE_Start()`, not DBM;
`Capture_OnFrameComplete()` redirects to the other buffer via `HAL_DCMIPP_PIPE_SetMemoryAddress()`
unless that buffer is UVC-locked, in which case it drops the frame and keeps writing the current
one) — now correctly combined with Bug 9's `CLEAR_BIT(DCMIPP->P1FCTCR, DCMIPP_P1FCTCR_CPTMODE)`
fix, which is what was missing the first time this design was tried. This gives genuine mutual
exclusion: UVC only ever reads the buffer DCMIPP is *not* currently targeting, for as long as it
needs, with no fixed timing assumption at all (unlike DBM's blind alternation). Also removed the
per-IRQ `GINTSTS` printf added for Bugs 11/12 (fires every SOF, ~1kHz, drowning out the rest of
the log) at the user's request, now that USB itself is confirmed stable — `usb_irq_count` alone
is kept as a cheap permanent counter. Rebuilt clean. **Not yet re-tested on real hardware as of
this entry.**

**General lesson for next time**: when a design is abandoned because it "didn't work," and a
*different*, later bug fix changes the conditions it was tested under, it's worth deliberately
re-testing the abandoned design rather than assuming the original conclusion still holds. Bug 8
was correctly diagnosed as "not the problem" only in hindsight, after Bug 9 was found — at the
time, there was no way to know the failure belonged to a different, unrelated fix. This is
distinct from the earlier "GET_DESCRIPTOR guess from the reference project's config" pattern
(Bugs 7/8 attempts on the enumeration problem) — this one is about revisiting a previously
abandoned *correct* design, not guessing a new one.

### Next steps

1. **Flash and re-test.** Check the UVC viewer for a clean, non-torn image, and finally evaluate
   actual color correctness — the original point of this entire feature.
2. If tearing somehow persists even with real mutual exclusion: the crop math in
   `ux_device_video.c` (`VIDEO_GetReadyBuffer() + 60*640*2`) or the `CVT_CvtRgb565ToMcu422`
   conversion itself would be the next things to scrutinize — at that point it would no longer be
   a buffering/timing bug, since mutual exclusion rules out read/write overlap by construction.
3. Frame rate (~6fps) is still unoptimized — see the previous entry's note; worth profiling once
   the image itself is confirmed correct.

---

## 2026-09-27 (later still) — USB fully working; Bug 13: torn image from single-buffer capture

**Huge milestone**: Bug 12's fix worked. Real hardware log shows `[UVC] fps=6 payloads=294
done_cb=294 drops=0 (jpeg_B=49816 ep_mps=1024)` repeating steadily, `[JPG] enc=49816B t=26ms`
lines, and no more `USBRST`/`ENUMDNE` retry cycling — **the device enumerates and streams MJPEG
over USB successfully for the first time.** Bugs 7 through 12 (IRQ priority, double-buffer
reactive addressing, CPTMODE never cleared, RIF for OTG1/OTG1HS/JPEG, D-Cache on the SETUP
buffer, D-Cache on every EP transfer) all needed fixing to get to this point.

**New symptom**: opening the stream in a UVC viewer showed a garbled image — solid horizontal
black/white stripes, on every single frame, not intermittently.

**Root cause**: Bug 8's accepted trade-off (single fixed capture buffer, no ping-pong, to avoid
Bug 8/9's manual-reprogramming traps) turned out to matter in practice, not just in theory.
DCMIPP writes into `video_buf[0]` continuously on its own ~32ms cycle (confirmed via the
~31fps warmup logs throughout this whole debugging session). `JPG_Encode()` takes ~26-27ms per
frame (per the new `[JPG] enc=...ms` diagnostic) to read sequentially through that same buffer.
Since the encoder's read sweep (~27ms) and DCMIPP's write sweep (~32ms) are two independent,
unsynchronized passes over the *same* memory, of *comparable* duration, they were essentially
guaranteed to catch each other mid-frame on every single frame -- not a rare race, a systematic
one. That matches "every frame is garbage" rather than "occasional glitches."

**Fix**: switched from manually managing one fixed buffer to DCMIPP's own hardware
double-buffering: `HAL_DCMIPP_CSI_PIPE_DoubleBufferStart()` (the `DBM` bit) instead of
`HAL_DCMIPP_CSI_PIPE_Start()`. Once started this way, DCMIPP itself alternates which of the two
programmed addresses (`video_buf[0]`/`video_buf[1]`) it writes to after each completed frame --
no software ever touches the memory-address or capture-mode registers again after the single
`DoubleBufferStart()` call, which is exactly what sidesteps both of this project's earlier
DCMIPP traps (Bug 8: reactively calling `HAL_DCMIPP_PIPE_SetMemoryAddress()` mid-stream stalled
the pipe; Bug 9: `HAL_DCMIPP_CSI_PIPE_Start()`'s OR-only capture-mode register). `Capture_
OnFrameComplete()` now reads `DCMIPP->P1SR`'s `LSTFRM` bit ("Last frame LSB bit, sampled at
frame capture complete event") to know which of the two buffers hardware just finished writing,
instead of assuming index 0. As long as the JPEG encoder finishes reading buffer N before
DCMIPP cycles back to it two frame periods later (~64ms, vs. the encoder's measured ~27ms —
comfortable margin), there is no read/write overlap at all, by construction, not by luck.
Rebuilt clean. **Not yet re-tested on real hardware as of this entry.**

**General lesson for next time**: "an occasional torn frame" and "every frame is garbage" are
different bugs with different causes even when they look superficially similar — the latter
usually means two operations of *comparable duration* are racing on a fixed schedule (not a rare
timing fluke), and the fix is structural (real double buffering) rather than making one side
faster. Also: this project tried two different manual approaches to DCMIPP addressing (Bug 8,
Bug 9) before reaching for the hardware's own purpose-built double-buffer mode — worth reaching
for `HAL_DCMIPP_CSI_PIPE_DoubleBufferStart()` first next time multi-buffer capture is needed on
this peripheral, now that its correct usage pattern is established here.

### Next steps

1. **Flash and re-test.** Check the UVC viewer for a clean (non-torn) image this time, and
   evaluate actual color correctness — the original point of this entire feature, now finally
   reachable after Bugs 7-13.
2. Frame rate is currently ~6 fps in the streaming logs, well below DCMIPP's own ~31fps capture
   rate — most captured frames are simply never picked up for encoding (the encoder only grabs a
   new frame once the previous JPEG has fully finished transmitting over USB). Not investigated
   yet; if higher frame rate matters, profile whether JPEG encode time, USB transfer time (50KB/
   frame over EP with 1024B max packet size), or the UVC payload-scheduling loop itself is the
   bottleneck.
3. If the image is still torn despite double-buffering: check that `LSTFRM`'s polarity/meaning
   is what this entry assumed (toggles with buffer index) rather than something else entirely --
   cross-check against `P1PPM0AR1`/`P1PPM0AR2`'s actual current values if available, or against
   ST's own DCMIPP double-buffer example/reference code.

---

## 2026-09-27 (still later) — Bug 12: same D-Cache gap on every EP transfer, not just SETUP

**Result after flashing Bug 11's fix**: big improvement — `usb_irq_count` jumped from 8 to 97
before plateauing, and decoding the `GINTSTS` dump showed `IEPINT` (IN-endpoint, i.e. the device
actually transmitting) firing 25 times, plus `RXFLVL` a few times too. So Bug 11's SETUP-buffer
cache fix was real and necessary — the device is now actually attempting to respond. But the
host still never enumerated successfully (repeated `USBRST`/`ENUMDNE` cycles in the trace = the
host kept resetting and retrying, consistent with receiving *something* back but rejecting it,
rather than a flat timeout as before).

**Root cause**: the exact same missing-cache-maintenance gap as Bug 11, just not fully fixed —
Bug 11 only patched the one specific case (`hpcd_USB_OTG_HS1.Setup[]`) at the call site in this
project's own `stm32n6xx_it.c`. But USBX's shared STM32 DCD driver,
`_ux_dcd_stm32_transfer_request()` (`Middlewares/ST/usbx/common/usbx_stm32_device_controllers/
ux_dcd_stm32_transfer_request.c`), calls `HAL_PCD_EP_Transmit()`/`HAL_PCD_EP_Receive()` for
*every* endpoint transfer on the controller — including the actual `GET_DESCRIPTOR` response
data itself — with zero cache maintenance anywhere in that file. Confirmed by grep: no
`CleanDCache`/`InvalidateDCache` calls exist in either `ux_dcd_stm32_transfer_request.c` or
`ux_dcd_stm32_callback.c`. So even with Bug 11's fix, the *outgoing* descriptor bytes USBX
builds and hands to `HAL_PCD_EP_Transmit()` could still be transmitted from stale cache content
if the CPU's write to that buffer hadn't been flushed to actual RAM yet — the OTG core's DMA
reads directly from RAM, bypassing cache. This is a write-direction counterpart to Bug 11's
read-direction issue, on a much larger scale (every transfer, not just the one 8-byte SETUP
packet).

**Fix**: added D-Cache maintenance directly in the shared driver function (affects every future
transfer on this controller, not just enumeration, so it belongs in the driver rather than
patched per call site):
- Before `HAL_PCD_EP_Transmit()`: `SCB_CleanDCache_by_Addr()` on the buffer being sent (CPU
  wrote it, DMA is about to read it — clean/flush, not invalidate).
- After a control-endpoint `HAL_PCD_EP_Receive()` completes (i.e. after its semaphore wait
  succeeds): `SCB_InvalidateDCache_by_Addr()` on the buffer that was received into (DMA wrote
  it, CPU is about to read it).
Added `#include "stm32n6xx_hal.h"` to the driver file to get `SCB_CleanDCache_by_Addr`/
`SCB_InvalidateDCache_by_Addr` (previously this file had no STM32-specific includes at all,
pure USBX). Rebuilt clean. **Not yet re-tested on real hardware as of this entry.**

**General lesson for next time**: when a missing-cache-maintenance bug is found in vendored
middleware, check whether the fix needs to go in the *driver* (affects all future use of that
peripheral) rather than just the one call site that happened to surface the bug first (Bug 11's
SETUP-buffer-only fix was correct but incomplete for exactly this reason). This project has now
hit the D-Cache-vs-DMA class of bug three times (`camera_framebuffer`/`video_buf`, the USB
SETUP buffer, and now every USB EP transfer) — see knowledge_archive.md §8's general lessons.

### Next steps

1. **Flash and re-test.** This is now the second consecutive fix at the exact same failure
   point (enumeration). Watch `dmesg`/`lsusb` for real success this time, and watch whether the
   `USBRST`/`ENUMDNE` retry cycling in the `[USB_IRQ #N]` log finally stops because enumeration
   succeeded, not because the host gave up.
2. Once enumerated: open the stream in a UVC viewer, check frame rate, and evaluate color
   correctness — the original point of this whole feature, and something that's been blocked on
   USB enumeration working at all through Bugs 7–12.
3. If it *still* fails: the video streaming data path itself (`ux_device_class_video_write_*`
   in `Middlewares/ST/usbx/common/usbx_device_classes/`) may have this exact same
   missing-cache-maintenance gap for its own bulk/isochronous payload buffers — check there next
   using the same method (grep for `CleanDCache`/`InvalidateDCache`, find none, add them at the
   same two points: clean before an IN transmit, invalidate after an OUT/SETUP receive).

---

## 2026-09-27 (yet later still) — Bug 11: stale D-Cache on the OTG core's DMA-written SETUP buffer

**Result of the GINTSTS dump**: decoded all 8 values against `USB_OTG_GINTSTS_*` bit
definitions. Sequence: `#1 SRQINT` (session start) → `#2 USBRST` (bus reset) → `#3 ENUMDNE`
(speed negotiation done, matches `dmesg`'s "new high-speed device") → `#4/7/8 OEPINT` (an OUT-
endpoint event — this is where a SETUP packet lands). **`IEPINT` (IN-endpoint interrupt — the
device actually transmitting a response) never once appears.** This lines up exactly with the
PC-side symptom: the host never receives any response at all to `GET_DESCRIPTOR`, not even a
NAK — the SETUP packet physically arrives (hardware-confirmed via `OEPINT`), but the device
never gets as far as sending anything back.

**Root cause**: `PCD_HandleTypeDef::Setup[12]` (`stm32n6xx_hal_pcd.h`) is where
`USB1_OTG_HS`'s own internal DMA (`dma_enable = ENABLE` in `MX_USB1_OTG_HS_PCD_Init()`) writes
the incoming SETUP packet's raw bytes. It's an ordinary cacheable field in a plain global
struct. Checked both `stm32n6xx_hal_pcd.c` and USBX's
`ux_dcd_stm32_callback.c`: **neither ever calls `SCB_InvalidateDCache_by_Addr()` on it** before
`HAL_PCD_IRQHandler()` reads it and hands it to USBX's request-parsing code. This project
enables D-Cache (`SCB_EnableDCache()` in `main()`), so the CPU can read back stale cached bytes
instead of what the DMA just wrote — **exactly the same class of bug this project's own camera
pipeline already needed explicit cache-invalidation for** (`camera_framebuffer`/`video_buf`,
documented at length in knowledge_archive.md). USBX silently failing to recognize a
stale/garbage SETUP request (rather than erroring loudly) is fully consistent with `OEPINT`
firing but no `IEPINT` ever following.

**How found**: the previous entry's raw `GINTSTS` diagnostic did the actual detective work —
this entry is the analysis of that data, not a new blind guess. Once "SETUP arrives but no
response is ever sent" was established as the precise failure point, this project's own
established pattern (D-Cache coherency bugs around DMA-written buffers) was the natural next
thing to check, and grepping the HAL/USBX source for `InvalidateDCache` near the PCD driver
confirmed it was simply never done there.

**Fix**: added `SCB_InvalidateDCache_by_Addr((uint32_t *)hpcd_USB_OTG_HS1.Setup,
sizeof(hpcd_USB_OTG_HS1.Setup))` at the top of `USB1_OTG_HS_IRQHandler()`
(`stm32n6xx_it.c`), before `HAL_PCD_IRQHandler()` processes the interrupt — the cheapest place
to apply the fix without patching vendored HAL/USBX source. Rebuilt clean. **Not yet re-tested
on real hardware as of this entry.**

**General lesson for next time**: any time a DMA-capable peripheral's driver is dropped into
this project without modification (HAL drivers, USBX/ThreadX middleware, or any other vendor
code), check whether it was written/validated with D-Cache assumptions that don't hold here —
this project deliberately enables D-Cache (`SCB_EnableDCache()`), and vendor drivers targeting
chips/configurations without it enabled by default will not invalidate cache around DMA-written
buffers themselves. This is now the *third* distinct place this same underlying class of issue
has come up in this project (`camera_framebuffer`/`video_buf` originally, now the USB SETUP
buffer) — worth checking proactively (not just reactively) before wiring up the next DMA-capable
peripheral (e.g. if JPEG's own DMA-facing buffers are ever found to misbehave, check this first).

### Next steps

1. **Flash and re-test.** Watch for `IEPINT` finally appearing in the `[USB_IRQ #N]
   GINTSTS=0x...` dump, and for actual enumeration on the PC (`lsusb`, `dmesg` showing a
   successful `NUCLEO-N65X0Q-ISP UVC` device instead of a descriptor-read timeout).
2. If enumeration succeeds this time: open the stream in a UVC viewer and check frame rate and
   — the original goal of this whole feature — whether the color now looks correct, since it's
   sourced from this project's own evision AWB/AE output rather than n6-ai-test's suspect
   hand-tuned path.
3. If `OEPINT`/`IEPINT` still don't show a full SETUP→response cycle: the next place to check is
   whether USBX's own EP0 IN-transfer *source* buffers (whatever holds the descriptor bytes it's
   trying to send) need a `SCB_CleanDCache_by_Addr()` (not invalidate — clean/flush, since this
   direction is CPU-writes-then-DMA-reads, the opposite direction from the SETUP-buffer case)
   before the OTG core's DMA reads them to actually transmit.

---

## 2026-09-27 (yet later) — usb_irq_count IS non-zero; added raw GINTSTS dump per IRQ

**Result of the previous entry's diagnostic**: `usb_irq_count` climbed (4 → 7 → 8) then
plateaued permanently at 8. This is a real, useful data point: **the interrupt does reach the
CPU** — ruling out the NVIC/TrustZone-interrupt-security-state theory entirely, no need to chase
that further. The plateau pattern (small increments, then flat) lines up with `dmesg`'s several
enumeration retry attempts (each host retry = one more USB bus reset = a couple of interrupts)
followed by the host giving up retrying — i.e. the plateau is the *host* stopping, not the
device breaking. So somewhere in each reset/enumeration attempt, a couple of interrupts fire
correctly, but the SETUP packet still never gets answered.

**Next diagnostic added**: `USB1_OTG_HS_IRQHandler()` now prints the raw `GINTSTS` (OTG core
Global Interrupt Status) register value on every single fire, before `HAL_PCD_IRQHandler()`
processes/clears it. This will show, per interrupt, exactly which hardware event it was —
`USBRST` (bus reset), `ENUMDNE` (speed enumeration done), `RXFLVL`/`OEPINT` (an actual SETUP
packet arrived on EP0), etc. If `RXFLVL`/`OEPINT` never appears across all ~8 fires, the SETUP
packet itself is never reaching the core/FIFO (a different, probably PHY/FIFO-config bug) —
very different from USBX receiving a real SETUP interrupt but failing to respond to it (a
USBX/DCD driver bug). Rebuilt clean. **Not yet re-tested on real hardware as of this entry.**

### Next steps

1. **Flash and read the new `[USB_IRQ #N] GINTSTS=0x...` lines** — there should be about 8 of
   them, matching `usb_irq_count`'s last value from the previous log. Decode the bits against
   `USB_OTG_GINTSTS_*` in `stm32n657xx.h` (`USBRST`, `ENUMDNE`, `RXFLVL`, `OEPINT`, `IEPINT`,
   etc.) to see which events actually occurred.
2. If `RXFLVL`/`OEPINT` (or `IEPINT` for EP0 IN, since `GET_DESCRIPTOR`'s response is an IN
   transfer) never appears: the problem is upstream of USBX entirely — re-examine
   `HAL_PCD_MspInit()`'s FIFO/PHY sequencing once more, this time looking specifically for
   what's needed to get SETUP-stage packets recognized at all (not just bus reset/speed
   negotiation, which already works).
3. If those bits DO appear: the interrupt chain is fully healthy at the hardware level, and the
   bug is specifically in `ux_dcd_stm32_callback.c`'s `HAL_PCD_SetupStageCallback` (or deeper in
   USBX's own control-transfer state machine) failing to actually queue/send the response —
   next step would be instrumenting that function directly.

---

## 2026-09-27 (even later still) — Bug 10's fix insufficient; added USB IRQ-fire diagnostic

**Symptom after flashing Bug 10's RIF fix**: identical `dmesg` behavior — `new high-speed USB
device` (chirp/speed negotiation still succeeds) then `device descriptor read/64, error -110`
every time, now confirmed on a direct root port (`3-5`, not through the hub chain seen in
earlier logs), ruling out a hub/cable signal-integrity explanation too. So Bug 10's RIF fix,
while a real and worthwhile fix in its own right (the OTG/JPEG RISC attributes were genuinely
missing), was not sufficient to fix enumeration by itself — there is at least one more problem.

**Next debugging step taken**: rather than guess again from the reference project's config (two
guesses from that source, Bugs 8/9-adjacent, already came up short on the *same* top-level USB
symptom), added a raw ISR-fire counter -- `usb_irq_count`, incremented at the very top of
`USB1_OTG_HS_IRQHandler()` (`stm32n6xx_it.c`), printed every second in the existing
`[UVC_CAP]` line (`app_threadx.c`). This directly answers the next diagnostic question: does
`USB1_OTG_HS_IRQn` fire *at all* once a host is attached and chirping? If `usb_irq_count` stays
at 0 forever, the problem is at the NVIC/PHY/clock level (interrupt never reaches the CPU) --
possibly a TrustZone interrupt-target-security-state issue (`TX_SINGLE_MODE_SECURE=1` plus CMSE
`-mcmse` build: worth checking whether `USB1_OTG_HS_IRQn`'s NVIC "Interrupt Target Non-Secure"
bit needs explicit configuration for this project, since it's not something DCMIPP/CSI's
already-working interrupts would surface if it's non-secure-specific). If `usb_irq_count`
climbs but enumeration still fails, the problem is downstream in the USBX DCD/device-stack
processing chain (`ux_dcd_stm32_callback.c`'s `HAL_PCD_SetupStageCallback`/etc., or USBX's own
control-transfer state machine) and further probing should add counters there instead.

**Not yet re-tested on real hardware as of this entry** — this entry only adds a diagnostic,
it does not claim to fix anything. Rebuilt clean.

### Next steps

1. **Flash and check the new `usb_irq=` field in the `[UVC_CAP]` log line.**
   - If it stays 0: investigate NVIC/TrustZone interrupt-security-state routing for
     `USB1_OTG_HS_IRQn` specifically (see knowledge_archive.md §6 for this project's TrustZone
     context) -- check whether it needs to be explicitly assigned to the security state
     ThreadX/the Appli actually runs in, unlike DCMIPP/CSI which happen to already work.
   - If it climbs (interrupt is firing): the problem is in the USBX DCD/device-stack processing
     itself, not hardware/NVIC -- add counters inside
     `Middlewares/ST/usbx/common/usbx_stm32_device_controllers/ux_dcd_stm32_callback.c`'s
     `HAL_PCD_ResetCallback`/`HAL_PCD_SetupStageCallback`/`HAL_PCD_ConnectCallback` next (these
     already exist and are non-weak, just uninstrumented) to see how far a SETUP packet actually
     gets processed before it goes silent.
2. Continue to hold off on the color-correctness check until enumeration itself works end to
   end -- no point evaluating image quality on a device the PC can't even see yet.

---

## 2026-09-27 (even later) — Bug 10: RIF never extended to OTG1/OTG1HS/JPEG for the UVC pipeline

**Symptom**: Bug 9's fix worked — `[UVC_CAP] fps=31 frames=...` climbed continuously on real
hardware, DCMIPP capture fully solved. But the PC never saw the camera as a USB device.
`lsusb` showed only the ST-Link probe (a completely separate USB connector/purpose, not the
board's own `USB1_OTG_HS` port). Confirmed the OTG cable *was* connected via `dmesg`:
```
usb 3-9.3.1: new high-speed USB device number ... using xhci_hcd     <- device WAS detected, HS negotiated
usb 3-9.3.1: device descriptor read/64, error -110                  <- but every actual data transfer times out
usb 3-9.3.1: unable to enumerate USB device
```
So the device successfully did the electrical/chirp-level speed negotiation (proves the cable,
PHY clock config, and `MX_USB1_OTG_HS_PCD_Init()` FIFO/PHY setup from the UVC port are all
correct) but timed out on the very first real data transfer (`GET_DESCRIPTOR`).

**Root cause**: exactly Bug 2's pattern (RIF silently drops disallowed AXI writes instead of
erroring), on a different peripheral. `SystemIsolation_Config()` (`main.c`) was written back
when only the camera existed and only configures RIF for `RIF_MASTER_INDEX_DCMIPP` (RIMC) and
`RIF_RISC_PERIPH_INDEX_CSI`/`RIF_RISC_PERIPH_INDEX_DCMIPP` (RISC). It was **never revisited**
when the UVC port added `USB1_OTG_HS` (with `dma_enable=ENABLE` — its internal DMA moves
control/bulk/iso transfer data over AXI) and the hardware JPEG encoder. Both are RIF-governed
AXI masters/slaves that were left unconfigured — meaning their DMA transactions were silently
dropped, exactly matching the symptom: the initial USB chirp/speed-negotiation (control-plane
signaling, no AXI DMA involved) succeeded, but the actual `GET_DESCRIPTOR` response data
transfer (which needs the OTG core's DMA to move bytes from RAM to the USB FIFO) never
completed.

**How found**: this project's own `SystemIsolation_Config()` comment (written during Bug 2's
fix, months before UVC existed) already documented that the reference project's
`Security_Config()` sets RIMC for `DCMIPP`/**`OTG1`** and RISC for `CSI`/`DCMIPP`/**`OTG1HS`**/
**`JPEG`** — but at the time only the DCMIPP/CSI half of that was ever acted on, since USB/JPEG
weren't in the picture yet. Confirmed against `Camera_N6_AI_Test`'s actual `Security_Config()`
source (not just the paraphrased comment) to get the exact calls/macro names right. Also ruled
out a cable/hub issue first: compared `MX_USB1_OTG_HS_PCD_Init()`, `HAL_PCD_MspInit()`, and
`ux_device_descriptors.c` byte-for-byte against the reference (all identical or only
cosmetically different), which is what pointed the investigation at configuration state
*outside* those files instead — i.e. RIF, the thing that bit this exact project once already.

**Fix**: added to `SystemIsolation_Config()`:
```c
HAL_RIF_RIMC_ConfigMasterAttributes(RIF_MASTER_INDEX_OTG1, &RIMC_master);
HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_OTG1HS, RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
HAL_RIF_RISC_SetSlaveSecureAttributes(RIF_RISC_PERIPH_INDEX_JPEG,   RIF_ATTRIBUTE_SEC | RIF_ATTRIBUTE_PRIV);
```
Rebuilt clean (macros exist, no errors, RAM usage unchanged). **Not yet re-tested on real
hardware as of this entry.**

**General lesson for next time**: RIF configuration must be revisited every time a new
peripheral that does its own AXI bus mastering (DMA-capable peripherals especially) is added to
the project, not just set once and forgotten. This project has now hit this exact trap twice
(I2C2's GPIO pins via a leftover unrelated master, and now USB/JPEG via a config that was simply
never extended) — before wiring up any new DMA-capable peripheral on this chip, check
`SystemIsolation_Config()`/`Security_Config()` first, don't wait for a silent-failure symptom to
rediscover it. See knowledge_archive.md §5 for the general RIF explanation.

### Next steps

1. **Flash and re-test.** Watch `dmesg`/`lsusb` on the PC for successful enumeration (a
   `NUCLEO-N65X0Q-ISP UVC` device appearing, not just descriptor-read timeouts), and the
   firmware log for `[UVC] Activated`/`[UVC] Stream ON` lines from `ux_device_video.c` that
   never appeared before (enumeration never got that far).
2. Once enumerated, open the stream in a UVC viewer (VLC, guvcview, Windows Camera app) and
   check: is there a valid MJPEG picture at all, what's the actual frame rate, and — the
   original point of this whole feature — does the color look correct now that it's sourced
   from this project's own evision AWB/AE output instead of n6-ai-test's suspect hand-tuned
   path.

---

## 2026-09-27 (later) — Bug 9 (the real cause of Bug 8's symptom): CPTMODE bit never cleared

**Symptom**: identical to Bug 8's, byte-for-byte, even after Bug 8's fix (removing the reactive
`HAL_DCMIPP_PIPE_SetMemoryAddress()` ping-pong) was flashed. `[UVC_CAP] fps=1 frames=1` once,
then `fps=0 frames=1` forever. This proved Bug 8's fix, while a reasonable simplification worth
keeping, was not the actual cause.

**Root cause, found by reading the HAL driver source directly** (`stm32n6xx_hal_dcmipp.c`):
`HAL_DCMIPP_CSI_PIPE_Start()` calls a static helper `DCMIPP_SetConfig()` which does:
```c
hdcmipp->Instance->P1FCTCR |= CaptureMode;   /* OR, not assignment -- never cleared */
```
`DCMIPP_MODE_CONTINUOUS` is defined as `0`; `DCMIPP_MODE_SNAPSHOT` is the `P1FCTCR_CPTMODE` bit
(bit 2). `main.c`'s own confirmed-working sequence is: Start `CONTINUOUS` (warmup) → `Stop()` →
Start `SNAPSHOT` (single-shot verification) — that last call ORs the `CPTMODE` bit into
`P1FCTCR`. Checked `HAL_DCMIPP_CSI_PIPE_Stop()`'s implementation too: it only clears
`CPTREQ`/`PIPEN`, **never `CPTMODE`**. So by the time `CaptureUVC_Thread()` (started later, once
ThreadX takes over) calls `Start(..., DCMIPP_MODE_CONTINUOUS)`, that call does `P1FCTCR |= 0` —
a complete no-op on the mode bits. The hardware was, this whole time, still configured for
**SNAPSHOT** mode from main.c's earlier single-shot verification. It wasn't malfunctioning or
stalling at all: it did exactly what SNAPSHOT mode does — captured exactly one frame, then
stopped cleanly, with no overrun/AXI-error flags because nothing actually went wrong at the
peripheral level. This fully explains why Bug 8's fix (which addressed a real, separate
double-buffering risk) had zero effect on this particular symptom.

**Fix**: in `CaptureUVC_Thread()` (`app_threadx.c`), explicitly
`CLEAR_BIT(DCMIPP->P1FCTCR, DCMIPP_P1FCTCR_CPTMODE)` immediately before calling
`HAL_DCMIPP_CSI_PIPE_Start(..., DCMIPP_MODE_CONTINUOUS)` — this HAL driver never clears that bit
for you, by design or oversight, so any code that starts SNAPSHOT mode even once must clear
`CPTMODE` itself before ever starting CONTINUOUS mode again. Rebuilt clean (RAM usage
unchanged). **Not yet re-tested on real hardware as of this entry.**

**General lesson for next time**: this project's `HAL_DCMIPP_CSI_PIPE_Start()`/`_Stop()` pair is
**not** a clean state machine — `Start()`'s mode-setting register write is OR-only and `Stop()`
doesn't clean up the mode bit it set. Any code path that calls `Start()` with different
`CaptureMode` values across the lifetime of the program (as this project now does: `CONTINUOUS`
→ `SNAPSHOT` → `CONTINUOUS` again) must manually clear the relevant `P1FCTCR`/`P0FCTCR`/
`P2FCTCR` mode bit itself between calls — never assume `Stop()` resets a pipe to a truly blank
state. This is a second instance (after Bug 5's stale MSP-init RCC block) of "a `HAL_OK` return
and no error flags does not mean the hardware ended up the way you expect" — see
knowledge_archive.md §8's general lessons list, both entries are the same underlying pattern.

### Next steps

1. **Flash and re-test.** This is now the third fix attempt for the same top-level symptom
   (frames stuck at 1) — Bug 7's IRQ-priority fix was real but not the cause, Bug 8's
   double-buffering removal was a reasonable simplification but not the cause either, and Bug 9
   (this entry) is the first fix backed by reading the actual HAL source and finding the
   specific register bit responsible, not just pattern-matching against the working reference
   project. Watch for `[UVC_CAP] fps=...` finally climbing continuously.
2. If it still doesn't work, do NOT reach for another guess from the reference project's
   pattern — that approach has now been tried twice (Bugs 7 and 8) without success. Instead,
   add a raw register dump (`P1FCTCR`, `P1FSCR`, `CMSR1`, `P1SR`) to the `[UVC_CAP]` periodic
   print itself, so the next log shows the actual hardware capture-mode/active state directly
   instead of requiring another round of source-reading to hypothesize from.
3. Once frames are flowing continuously, move to the original UVC checklist: USB enumeration on
   the PC, valid MJPEG stream in a UVC viewer, frame rate, and color correctness.

---

## 2026-09-27 — Bug 8: reactive HAL_DCMIPP_PIPE_SetMemoryAddress() stalls PIPE1 after frame 1

**Symptom (real hardware log, after Bug 7's IRQ-priority fix)**: single-shot verification still
100% correct as always. Bug 7's fix did NOT change the failure mode at all: `[UVC_CAP]` still
printed `fps=1 frames=1` once, then `fps=0 frames=1` forever, byte-for-byte identical to the
pre-fix log. This ruled out the IRQ-priority theory: if the kernel itself were corrupted, the
capture thread's own 1 Hz print loop (which itself depends on ThreadX's timer/scheduler) could
not have kept running perfectly on schedule indefinitely, exactly as observed — so Bug 7's fix
was real and worth keeping (correct per the reference project and the documented ThreadX
priority requirement), but it was fixing a latent risk, not this particular symptom.

**Root cause**: `Capture_OnFrameComplete()` (`app_threadx.c`) called
`HAL_DCMIPP_PIPE_SetMemoryAddress()` on every completed frame, to ping-pong DCMIPP's DMA target
between `video_buf[0]`/`video_buf[1]` (double-buffering, ported from `Camera_N6_AI_Test`'s
`app_threadx.c`, which does the exact same thing and streams fine on its own hardware). On
*this* project's hardware/pipeline it made PIPE1 stop generating frame-complete interrupts
after exactly one frame — permanently, with `P1SR`'s `OVRF` and the AXI-error counter both
staying 0 the whole time (i.e. not a DCMIPP-reported error; the pipe just silently never
re-armed capture after that one register write).

**How found**: this project's own pre-RTOS single-shot warmup loop in `main.c` is direct proof
that `DCMIPP_MODE_CONTINUOUS` re-arms correctly by itself, repeatedly, for many frames in a row
(`frame_count` climbed past 50 in every log so far) — as long as nothing ever calls
`HAL_DCMIPP_PIPE_SetMemoryAddress()` after `Start()`. The only difference between that
proven-working loop and the new UVC capture thread was exactly this one reactive register
write. (Why it works in `Camera_N6_AI_Test` but not here is unconfirmed — plausibly an
interaction with this project's `ISP_MW/evision` Bayer2RGB/statistics processing on PIPE1,
which the reference project's simpler hardware-only pipeline doesn't have — but the practical
fix doesn't require knowing why, only that the working pattern is well-established in this
exact codebase.)

**Fix**: removed the ping-pong entirely. DCMIPP now targets a single fixed buffer
(`video_buf[0]`, aliasing `camera_framebuffer`) for the whole continuous-capture lifetime,
exactly matching the proven single-shot warmup pattern. `Capture_OnFrameComplete()` now only
invalidates DCache and signals the semaphore — it never touches DCMIPP registers again after
the initial `Start()`. `video_buf[1]`/`video_buf1` are left allocated but unused (harmless,
kept in case real hardware double-buffering via DCMIPP's own DBM bit /
`HAL_DCMIPP_CSI_PIPE_DoubleBufferStart()` — not manual re-addressing — is revisited later).
Rebuilt clean (RAM usage unchanged at ~74.2%). **Trade-off accepted**: without a second live
DMA target, there's a small window where UVC's JPEG encode (which locks the buffer for its
whole encode duration) can read `video_buf[0]` while the *next* frame is already being written
into it — an occasional torn frame in the stream. Acceptable for this project's actual goal
(verify the capture pipeline and color are basically correct); revisit only if tearing turns
out to matter once streaming itself works.

**Not yet re-tested on real hardware as of this entry.**

### Next steps

1. **Flash and re-test.** Watch for `[UVC_CAP] fps=... frames=...` climbing continuously this
   time instead of freezing at 1.
2. Once frames are flowing continuously, move to the original UVC checklist: USB enumeration on
   the PC, valid MJPEG stream in a UVC viewer, frame rate, and color correctness. If frames
   still stop after some other fixed small number (not 1) this time, suspect a different
   resource contention (e.g. USBX's own use of the frame data during `fill_uvc_payload()`)
   rather than the DCMIPP register-write issue this entry fixed.

---

## 2026-09-26 (even later) — Bug 7: DCMIPP/CSI IRQ priority 0 corrupting ThreadX kernel

**Symptom (real hardware log)**: after the UVC port below, single-shot verification still
passed (100% frame, checksum OK), ThreadX/USBX bootstrap and DCMIPP continuous-mode start all
printed OK, but the capture thread's periodic fps line got stuck forever at `fps=0 frames=1` —
exactly one frame captured, then total silence. `ovr=0 axierr=0` the whole time (DCMIPP's own
overrun/AXI-error flags never set) — ruling out a peripheral/hardware capture problem.

**Root cause**: `stm32n6xx_hal_msp.c`'s `HAL_DCMIPP_MspInit()` still had `DCMIPP_IRQn` and
`CSI_IRQn` at **NVIC priority 0** — a leftover CubeMX default from when this project was
bare-metal (no RTOS, no conflict possible). Once continuous UVC capture started, the DCMIPP
frame-complete ISR chain (`HAL_DCMIPP_PIPE_FrameEventCallback` → `Capture_OnFrameComplete()` in
`app_threadx.c`) started calling `tx_semaphore_put()` — a ThreadX kernel API. ThreadX's
critical sections only protect against interrupts at or below the priority level its port
actually manages; an interrupt left at priority 0 can preempt the kernel mid-update and corrupt
its internal state. The very first ISR call did exactly that: it succeeded (frame 1 delivered,
`tx_semaphore_put()` ran once), but corrupted enough kernel state that nothing after it worked
— consistent with silence and no error flags, since the peripheral itself was never at fault.

**How found**: compared against `../Camera_N6_AI_Test` (this session's whole methodology, see
knowledge_archive.md §4) — its `stm32n6xx_hal_msp.c`, which also runs ThreadX+DCMIPP together
and is confirmed to stream continuously on real hardware, sets both `DCMIPP_IRQn` and
`CSI_IRQn` to priority **7**, not 0.

**Fix**: changed both `HAL_NVIC_SetPriority(DCMIPP_IRQn/CSI_IRQn, ...)` calls in
`stm32n6xx_hal_msp.c` from priority 0 to priority 7, matching the reference. Rebuilt clean
(`cmake --build --preset Debug`, RAM usage unchanged at 74.2%). **Not yet re-tested on real
hardware as of this entry** — this fix is reasoned from a well-documented ThreadX/Cortex-M
interrupt-priority requirement plus a confirmed-working reference project using the exact same
priority value for the exact same interrupts, but needs a fresh hardware log to confirm frames
now keep flowing past 1.

**General lesson for next time**: when mixing a bare-metal peripheral driver (originally tuned
with `HAL_NVIC_SetPriority(..., 0, 0)` for lowest possible interrupt latency) into a project
that later gains an RTOS, always re-check that peripheral's interrupt priority against whatever
the RTOS port requires for interrupts that call its APIs — "worked fine before the RTOS was
added" is not evidence the priority is RTOS-safe, and the failure mode (silent kernel
corruption, not a crash) can look identical to a peripheral-level bug.

### Next steps

1. **Flash and re-test.** Watch for the `[UVC_CAP] fps=... frames=...` line climbing steadily
   instead of freezing at 1. If it still freezes, the next thing to check is whether `USB1_OTG_HS_IRQn`
   (already at priority 7, unaffected by this fix) or any other newly-added ISR also calls a
   ThreadX API at an unsafe priority — audit every `HAL_NVIC_SetPriority` call across the
   project the same way this one was found.
2. Once frames are flowing, check the rest of the original UVC checklist: USB enumeration on
   the PC, valid MJPEG stream in a UVC viewer, frame rate, and color correctness.

---

## 2026-09-26 (later) — Added USB Video Class (UVC) live streaming (ThreadX + USBX)

### Goal

Let a PC view the camera feed live over USB (any UVC viewer: VLC, guvcview, Windows Camera app),
on top of the already-working single-shot PIPE1 capture (previous entry below) — without
touching that confirmed-working bring-up sequence, and without copying the color bug from the
sibling reference project's own UVC pipeline.

### Color-path decision

`../Camera_N6_AI_Test` (same board, confirmed enumerating/streaming UVC on real hardware) drives
DCMIPP's own hardware Bayer2RGB with hand-tuned `DCMIPP_ExposureConfTypeDef` white-balance
multiplier constants (`MultiplierRed=195`, `MultiplierBlue=185`, etc.) plus a hand-tuned RGB→YUV
matrix, output as YUV422, JPEG-encoded via `JPG_SRC_YUV422`. The user explicitly said this
project's color is wrong and not to copy that path. Instead, this project's own PIPE1 already
outputs `DCMIPP_PIXEL_PACKER_FORMAT_RGB565_1` through the real `ISP_MW/evision` AWB/AE algorithms
(proper 3A, not guessed constants) — confirmed capturing complete, correct 614400-byte frames on
hardware in the previous entry. The UVC pipeline encodes *that* RGB565 output directly:
`app_cvt.c` already contained an unused `CVT_FormatRgb565ToYuv422Jpeg()` (RGB565→YUV422-JPEG-MCU,
standard bit layout R:15-11 G:10-5 B:4-0) in `Camera_N6_AI_Test` — it was simply never wired up
because that project's own pipeline never produced RGB565. Wiring it up was the only piece of
new conversion logic needed; the JPEG HW encoder wrapper (`app_jpg.c`, polling-mode
`HAL_JPEG_Encode`) is otherwise untouched — it has no dependency on the color path.

### Files added (ported from `Camera_N6_AI_Test`, essentially as-is unless noted)

- `Middlewares/ST/threadx/` and `Middlewares/ST/usbx/` — whole vendor middleware trees, copied
  wholesale (~700 files).
- `Appli/Core/Inc/tx_user.h`, `ux_user.h`, `app_azure_rtos.h`, `app_azure_rtos_config.h`,
  `app_usbx.h`, `app_usbx_device.h`, `ux_device_descriptors.h`, `ux_stm32_config.h`,
  `ux_device_video.h`, `ux_device_cdc_acm.h`.
- `Appli/Core/Src/tx_initialize_low_level.S`, `stm32n6xx_hal_timebase_tim.c`, `app_azure_rtos.c`,
  `app_usbx.c`, `app_usbx_device.c`, `ux_device_descriptors.c`, `ux_device_cdc_acm.c`,
  `ux_device_video.c`, `app_jpg.c`, `app_cvt.c`.
- HAL drivers this project didn't have yet: `stm32n6xx_hal_pcd.c/.h`, `stm32n6xx_hal_pcd_ex.c/.h`,
  `stm32n6xx_ll_usb.c/.h`, `stm32n6xx_hal_jpeg.c/.h`, `stm32n6xx_hal_tim.c/.h`,
  `stm32n6xx_hal_tim_ex.c/.h` (copied from the reference project's `Drivers/` — same HAL package
  version, just a smaller subset was vendored here originally).
- Kept CDC-ACM registered alongside the Video class (simpler than stripping descriptor entries;
  this project doesn't use the serial channel but it doesn't cost anything either).
- Product string changed from the reference's generic `"STM32 USB Device"` to
  `"NUCLEO-N65X0Q-ISP UVC"` (`ux_device_descriptors.h`). Resolution left at the reference's
  640x360 UVC frame descriptor (`UVC_FRAME_WIDTH`/`UVC_FRAME_HEIGHT` already matched what this
  project's own crop produces — see below).

### Files adapted (small, targeted changes on top of the copy)

- `app_jpg.h`/`app_jpg.c`: added `JPG_SRC_RGB565` and a `JPG_Encode()` switch case calling
  `CVT_FormatRgb565ToYuv422Jpeg()` — the only color-path change; everything else (RAMFUNC tricks,
  DWT timing, polling-mode `HAL_JPEG_Encode`) is untouched.
- `ux_device_video.c`: no code changes needed beyond what the copy already had — its
  `VIDEO_GetReadyBuffer()`/`VIDEO_GetReadyBufferIdx()`/`uvc_locked_buf_idx` externs match what
  `app_threadx.c` (below) provides, and its center-crop math (`+ 60*640*2`, 640x480→640x360,
  2 bytes/pixel) is equally valid for RGB565 (also 2 B/px) as it was for YUV422 — kept as-is
  rather than switching to a full-frame encode, since it was already correct and tested logic.

### New file: `Appli/Core/Src/app_threadx.c` (NOT a copy of the reference's)

`Camera_N6_AI_Test`'s own `app_threadx.c` re-does camera/sensor bring-up via DCMIPP's hardware
Bayer2RGB — exactly the color path this project avoids — so it was written from scratch instead,
modeled on that file's *structure* (semaphore + capture thread + double-buffer swap pattern) but
not its camera init:

- Does **not** repeat sensor/GPIO/I2C/RIF/clock init — all of that already ran in `main.c` before
  `MX_ThreadX_Init()`/`tx_kernel_enter()` is ever called.
- Starts PIPE1 in `DCMIPP_MODE_CONTINUOUS` (main.c's own pre-RTOS code only ever used
  `DCMIPP_MODE_SNAPSHOT`, once, for the single-shot verification).
- Keeps `ISP_BackgroundProcess(&hcamera_isp)` pumped in the capture thread's loop so AWB/AE keeps
  converging during continuous streaming (previously only ever exercised for the ~60-frame
  warmup).
- Two RGB565 640x480 buffers (614400B each) for the double-buffer/ping-pong swap, mirroring the
  reference's race-condition fix faithfully (`uvc_locked_buf_idx` contract preserved exactly).
  **RAM-budget decision**: `video_buf[0]` reuses the exact same fixed-address memory (
  `camera_framebuffer`, `CAMERA_BUFFER_ADDR = 0x34200000`) that `main.c`'s single-shot
  verification used — free again by the time this thread runs, since PIPE1 is stopped after that
  verification completes. Only `video_buf[1]` is a newly allocated 614400-byte buffer. This
  project has ~2MB internal RAM and no PSRAM (confirmed by grepping the whole reference project:
  no external RAM is used there either), so avoiding a second full-size allocation mattered.
- **DCache**: mirrors this project's own confirmed-working single-shot pattern
  (`SCB_InvalidateDCache_by_Addr` once per completed frame) rather than the reference's
  "`.noncacheable` section managed by MPU" comment. That comment turned out to be aspirational:
  grepping the *entire* reference project (`grep -rn "MPU_Config\|noncacheable\|HAL_MPU"`) found
  no `HAL_MPU_ConfigRegion()` call anywhere and no variable ever placed in the linker's
  `.noncacheable` section — and the reference project never calls `SCB_EnableDCache()` at all
  (D-cache is simply off there, so there's no coherency problem to manage in the first place).
  This project's `main.c` *does* enable D-cache, and already has a proven, working invalidate
  pattern for exactly this DCMIPP-DMA-vs-CPU-cache scenario — reusing it here was the correct
  choice, not a guess.
- **Callback conflict with `main.c`'s existing code**: `main.c` already hard-defines (non-weak)
  both `HAL_DCMIPP_PIPE_FrameEventCallback` (frame_count/frame_received bookkeeping for the
  single-shot warmup) and `HAL_DCMIPP_PIPE_VsyncEventCallback` (calls `ISP_GatherStatistics()` —
  required for AWB/AE, must keep running during continuous streaming too). A HAL callback can
  only be defined once in the link, so `app_threadx.c` does **not** redefine either. Instead:
  - `HAL_DCMIPP_PIPE_VsyncEventCallback` is left completely untouched — it already does exactly
    what continuous streaming needs (ISP statistics gathering), regardless of snapshot vs.
    continuous mode.
  - `HAL_DCMIPP_PIPE_FrameEventCallback` in `main.c` gained one small addition: after its
    existing `frame_count++`/`frame_received = 1U` lines (untouched), it now calls
    `Capture_OnFrameComplete()` (defined in `app_threadx.c`) if-and-only-if
    `uvc_capture_active` is set — a flag `app_threadx.c`'s capture thread sets only after it has
    restarted PIPE1 in continuous mode. The two capture modes are time-disjoint (PIPE1 is
    stopped between the snapshot verification and the continuous restart), so this is safe and
    is a strict addition to the existing function, not a rewrite.
- `HAL_DCMIPP_PIPE_ErrorCallback` (pipe-specific overrun) *is* defined fresh in `app_threadx.c`
  — `main.c` only defines the different, global `HAL_DCMIPP_ErrorCallback`, so no conflict there.

### `main.c` changes (strict additions, single-shot verification untouched)

- Added `#include "app_threadx.h"`.
- Added `PCD_HandleTypeDef hpcd_USB_OTG_HS1;` global and `MX_USB1_OTG_HS_PCD_Init()` (ported
  as-is from the reference's `main.c` — FIFO sizing: RX=256w, EP0=64B, EP1=64B, EP2=512B,
  EP4=1024B).
- Added `HAL_TIM_PeriodElapsedCallback()` — needed because ThreadX's `tx_initialize_low_level.S`
  claims `SysTick_Handler`/`PendSV_Handler` for its own RTOS tick/context-switch once
  `tx_kernel_enter()` runs, so HAL's own tick source is moved to TIM6 instead (see
  `stm32n6xx_hal_timebase_tim.c`, ported from the reference, which overrides the weak
  `HAL_InitTick()`/`HAL_SuspendTick()`/`HAL_ResumeTick()`). Since that override is linked in from
  `HAL_Init()` onward, TIM6 is actually the tick source for the *entire* program including the
  pre-RTOS bring-up — `HAL_Delay()` in the existing camera init code keeps working exactly as
  before, just driven by TIM6 instead of SysTick.
- Replaced the old LED-blink `while(1)` idle loop (after the single-shot verification) with a
  call to `MX_ThreadX_Init()` (which calls `tx_kernel_enter()` and never returns). The single-shot
  capture/verification code above it is completely untouched.

### `stm32n6xx_it.c` changes

- Added `TIM6_IRQHandler()` (calls `HAL_TIM_IRQHandler(&htim6)`) and `USB1_OTG_HS_IRQHandler()`
  (calls `HAL_PCD_IRQHandler(&hpcd_USB_OTG_HS1)`).
- **Removed** the CubeMX-default `PendSV_Handler()` and `SysTick_Handler()` definitions — both
  are now provided by ThreadX's `tx_initialize_low_level.S` (`PendSV` for context switching,
  `SysTick` for the RTOS tick), and having both defined would be a duplicate-symbol link error
  (confirmed: this is exactly the error the build produced before removing them). Verified
  `SVC_Handler` does *not* conflict — ThreadX's own `SVC_Handler` in
  `tx_thread_schedule.S` is compiled out when `TX_SINGLE_MODE_SECURE` is defined (which this
  project's `mx-generated.cmake` now does), so this project's existing `SVC_Handler` stays.
  Did not touch the existing CSI/DCMIPP IRQ handlers or diagnostic counters.

### `stm32n6xx_hal_msp.c` changes

- Added `HAL_PCD_MspInit()`/`HAL_PCD_MspDeInit()` (ported as-is from the reference — HSE enable,
  VDDUSB, USB OTG/PHY clock config, USB1_HS_PHYC bring-up, NVIC). Added as new functions; did
  **not** touch the existing `HAL_DCMIPP_MspInit()` (which must keep the stale CubeMX RCC block
  removed — see `knowledge_archive.md` §4 Bug 5).
- `HAL_JPEG_MspInit()`/`HAL_JPEG_MspDeInit()` did **not** need adding here — they live in the
  ported `app_jpg.c` itself (just `__HAL_RCC_JPEG_CLK_ENABLE()`/`_DISABLE()`), matching where the
  reference project puts them.

### `secure_nsc.c`

Diffed against the reference project's version: **identical already**, no ThreadX-related NSC
additions were needed (the file already had whatever this configuration requires).

### CMake changes

- `Appli/mx-generated.cmake`: added `TX_INCLUDE_USER_DEFINE_FILE`, `TX_SINGLE_MODE_SECURE=1`,
  `UX_INCLUDE_USER_DEFINE_FILE` to `MX_Defines_Syms`; added threadx/usbx include dirs; added a new
  `App_UVC_Src` source list (the ported/new app_*.c and ux_device_*.c files); added the full
  `usbx_Src`/`threadx_Src` source lists (mirrored from the reference, `${CMAKE_SOURCE_DIR}`
  swapped for `${CMAKE_CURRENT_SOURCE_DIR}` to match this project's existing style); added
  `stm32n6xx_hal_pcd.c`, `stm32n6xx_hal_pcd_ex.c`, `stm32n6xx_ll_usb.c`, `stm32n6xx_hal_jpeg.c`,
  `stm32n6xx_hal_tim.c`, `stm32n6xx_hal_tim_ex.c` to `STM32_Drivers_Src`; added `usbx`/`threadx`
  OBJECT libraries (mirroring the reference) and linked them into `MX_LINK_LIBS`.
- `Appli/CMakeLists.txt`: added `set_source_files_properties(... app_cvt.c app_jpg.c
  ux_device_video.c PROPERTIES COMPILE_FLAGS "-O2")` — these are the UVC streaming hot path
  (pixel conversion, JPEG encode, payload assembly) and Debug builds are otherwise `-O0`.

### Build verification (REQUIRED per task — actually done, not just claimed)

Ran, from the project root: `cmake --preset Debug` then `cmake --build --preset Debug`. **Both
Appli and FSBL build cleanly** (ExternalProject_Add drives both from the top-level preset).
Iterated through several real errors before reaching a clean build:
1. Missing `ux_device_cdc_acm.h`/`.c` (needed by `app_usbx_device.h` since CDC-ACM was kept
   registered) — ported from the reference, added to `App_UVC_Src`.
2. `app_threadx.c` needed `isp_api.h`/`isp_core.h` for `ISP_HandleTypeDef`/
   `ISP_BackgroundProcess()`.
3. Duplicate-symbol link errors for `PendSV_Handler` and `SysTick_Handler` (see
   `stm32n6xx_it.c` changes above) — removed the CubeMX-default definitions once ThreadX's port
   claims both.

**Final result**: Appli links cleanly. `arm-none-eabi-size`: `text=181620 data=520 bss=1373152`
(RAM used: 1,555,328 / 2,096,128 bytes = 74.2%, per the linker's own `--print-memory-usage`
report). `Appli.bin` (via `objcopy -O binary`) = **182,176 bytes**.
`EXTMEM_LRUN_SOURCE_SIZE` (`FSBL/Core/Inc/stm32_extmem_conf.h`) is currently `0x40000` (256KB,
bumped in the earlier session entry below) — 182,176 bytes leaves ~78KB (≈30%) of headroom, not
"close to or over" the limit, so it was left unchanged. FSBL itself builds unaffected
(`text=61176`, well within its own budget).

### Needs real hardware testing next (cannot be verified without hardware)

See `Camera_README.md`'s new "USB Video Class (UVC) streaming" section for the checklist: USB
enumeration, whether a UVC viewer shows a valid MJPEG stream, achieved frame rate, whether the
color is now correct, and whether AWB/AE keeps converging during sustained continuous capture
(only ever exercised for ~60 warmup frames before this change).

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
