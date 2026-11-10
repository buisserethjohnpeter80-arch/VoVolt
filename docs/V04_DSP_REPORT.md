# VoVolt DSP v0.4: engineering change report

## Source audited

The uploaded ZIP was the v0.1 project (CMake `VERSION 0.1.0`), not v0.3. This v0.4 folder is a **new proposed consolidated replacement**, built on the uploaded source. Do not cherry-pick hypothetical v0.3 files.

## Changes

1. `VoltCore.h`: refactored state to be per channel AND per mode; exact nonlinear identity when Drive=0. Soft uses an input-normalized tanh blend. Hard uses input gain, Drive-linked threshold and a continuous-derivative quadratic knee. Tube uses asymmetric bias with signal-dependent envelope. Tape uses sample-rate-calibrated pre-emphasis and a magnetisation feedback/memory term. Both are **stylized** originals, not hardware-measured models.
2. `PluginProcessor.h/.cpp`: preallocate separate mode state and wet post-processing state; fixed 4x oversampling maintained; DC blocker added; sample-by-sample smoothed mode weights prevent abrupt mode switches, including rapid toggles; process unusually large host blocks in prepared-size chunks; preserve APVTS parameter IDs, state serialization and output routing.
3. `Tests/VoltCoreTests.cpp`: nine groups with failures that remain active even under `NDEBUG`: zero-drive transfer, finite output at multiple rates/drives, odd transfer for Soft/Hard, smooth Hard knee, Tube asymmetry, Tape state/reset, processing segmentation, harmonic content at 1 kHz, and recovery from non-finite input.
4. `CMakeLists.txt`: version advanced from 0.1.0 to 0.4.0. JUCE pin 8.0.8 and Windows workflow retained.
5. `Source/PluginEditor.*`, `Source/VoVoltLookAndFeel.h`: **no edits**.

## Confirmed local tests

- GCC C++20, `-O2 -Wall -Wextra -Wpedantic`: 9/9 DSP test groups passed.
- GCC C++20, `-O3 -DNDEBUG`: 9/9 DSP test groups passed.
- AddressSanitizer + UndefinedBehaviorSanitizer: core tests passed (rerun after final edits).

The above tests cover standalone JUCE-independent math only. They do **not** establish that the full VST3 compiled on Windows or was tested in FL Studio.

## Must validate on Windows before replacing working installs

- GitHub Actions Visual Studio 2022 Release build and artifact upload.
- JUCE VST3 scanner and FL Studio plugin loading, multiple DAW sessions.
- Delay compensation and dry/wet alignment by impulse/phase tests.
- Automation and fast character toggling while playing; CPU under load.
- Large blocks, mono/stereo, sample rates and preset serialization.
- Spectrum/aliasing measurements, clipping, DC drift, peak/loudness matching.

## Next phase after verified compilation

A. Measure 4 character harmonic spectra and CPU at equal loudness.
B. Tune Soft and Hard response without increasing UI scope.
C. Consider physical Tube/tape reference measurements or mathematical hysteresis if a closer analog emulation is desired.
D. Only then consider extra controls (threshold/knee exposure, selectable oversampling, and auto gain) as a separate versioned feature step.
