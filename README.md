# VoVolt DSP v0.5-dev | Saturation VST3

VoVolt is an independent C++20 / JUCE 8.0.8 saturation plugin, **not part of MELOTHÉON**.

## What this development build contains

- Four *independent* original DSP character models: Soft, Hard (quadratic knee), Tube (asymmetric/envelope-responsive) and Tape (stateful magnetic-memory approximation).
- All original v0.1 host parameter IDs, defaults and order maintained: `character`, `drive`, `tone`, `mix`, `output`, `bypass`.
- Existing light-blue UI style retained and extended with Low Cut, Thump, Punish, High Cut, Steep and Auto Gain controls.
- Per-channel/per-character DSP state; smoothed Drive/Tone/Output/Mix and new controls; one-pole smoothed character weights for rapid switching.
- Fixed 4x oversampling using JUCE's polyphase IIR, host-reported integer latency, preallocated aligned dry delay and DC removal on wet path.
- Process blocks larger than the prepared block size via **chunking**, without realtime allocations.
- JUCE APVTS DAW parameter automation and state recall, mono/stereo routing and level meters.
- Ten standalone C++ DSP test groups for the nonlinear core, plus Windows GitHub Actions build and CTest.

## Build (Windows)

Prerequisites: Git, CMake 3.22+, Visual Studio 2022 with C++ desktop workload, and internet for initial JUCE checkout.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DVOVOLT_BUILD_TESTS=ON
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

VST3 path: `build/VoVolt_artefacts/Release/VST3/`.
Install the `.vst3` bundle to `C:\Program Files\Common Files\VST3`, then rescan in FL Studio.
You can also use GitHub Actions: `.github/workflows/windows.yml` uploads the VST3 artifact after a successful build.

## New v0.5 development controls

- **Low Cut:** first-order pre-saturation high-pass, adjustable from 20 to 500 Hz. 20 Hz is the near-bypass position.
- **Thump:** adds a controlled low-frequency component from a 115 Hz low-pass branch before saturation. This is an original simplified enhancement, not a hardware model.
- **Punish:** adds up to 2.5x extra input-stage gain before the selected character model. It increases saturation intensity rather than simply turning up the output.
- **High Cut:** post-saturation one-pole low-pass, adjustable from 4 to 20 kHz.
- **Steep:** engages a second cascaded low-pass pole for a steeper high-frequency roll-off.
- **Auto Gain:** linked-channel RMS matching with 50 ms attack, 500 ms release, 100 ms gain smoothing and a bounded 0.25x–4x correction. Stereo channels share the same correction to avoid shifting the stereo image.

New controls are appended to the APVTS layout. Existing v0.1 parameter IDs, ranges, defaults and ordering are preserved. The existing Tone control is retained unchanged for preset compatibility. Auto Gain is an adaptive RMS estimate, not perceptual LUFS matching, and may need tuning after host listening tests.

## What is tested vs. untested

**Actually tested in the supplied environment:** JUCE-independent DSP core, 16 groups, with GCC C++20 in both optimized and `-DNDEBUG` builds. Additional AddressSanitizer/UndefinedBehaviorSanitizer run is performed for core tests.

**NOT tested here:** actual JUCE/VST3 compilation on Windows, sound quality in FL Studio, filter responses in the full processor, Auto Gain behavior on real program material, oversampler dry/wet latency impulse alignment, CPU under host load, extreme automation, and plugin scan/installation. Run Windows CI and host verification before any release claim.

## Intentional limitations and decisions

- v0.4 models are original perceptual *characters*, not replicas of Soundtoys or calibrated hardware. In particular Tape is a **simplified stateful model**, not a physical Jiles-Atherton/hysteresis simulation. Tube is not a component-level tube circuit simulation.
- Hard threshold and knee are currently **internal curves linked to Drive**. No new UI controls or automation IDs were added.
- Oversampling fixed at 4x, no mode selection yet. High-frequency aliasing measurements still required.
- Auto Gain is now present as a bounded linked-RMS estimate; it is not a perceptual loudness meter or final equal-loudness calibration. No advanced spectral analyzer is included yet.
- `Drive=0` is mathematically transparent **inside each nonlinear character**. Tone filtering, the wet DC blocker and the oversampling path still act on wet audio. `Mix=0` or Bypass routes the latency-aligned dry signal instead.
- When Bypass is enabled, Output is not applied to the bypassed dry path (same behaviour as v0.1).
- Host latency is held constant by JUCE's `useIntegerLatency` option; verify with impulses/phase cancellation before shipping.

Read `docs/V05_DEV_NOTES.md` for the new control-stage design. Read `docs/V04_DSP_REPORT.md` for change notes and test checklist, `docs/V04_SOFT_CALIBRATION.md` for the first SOFT calibration, and `docs/MUSE_HANDOFF.md` for the next CI and FL Studio validation steps.
