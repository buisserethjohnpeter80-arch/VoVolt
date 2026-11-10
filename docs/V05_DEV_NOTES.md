# VoVolt v0.5-dev — Decapitator-inspired control architecture

## Direction

VoVolt uses the broad workflow concepts of professional saturation processors as product-design references. It does not copy proprietary Soundtoys algorithms, circuit models, or branding. The four VoVolt character algorithms remain original: Soft, Tube, Tape and Hard.

## Signal path in this development build

1. Input and finite-sample protection.
2. 4x oversampling.
3. Low Cut high-pass.
4. Thump low-frequency branch and blend.
5. Punish input-stage gain.
6. Selected saturation character with smoothed character transition.
7. Existing Tone low-pass and DC blocker.
8. Downsampling.
9. High Cut, optionally cascaded for Steep.
10. Linked-channel RMS Auto Gain.
11. Latency-aligned Dry/Wet Mix, Output and Bypass.

## Important implementation notes

- Low Cut uses a first-order high-pass recurrence. The 20 Hz setting is treated as near-bypass to avoid unintended low-frequency attenuation at the default setting.
- Thump is currently a simple 115 Hz low-pass-derived low-frequency emphasis. It is not yet a resonant low-shelf or analog transformer model. Listening and frequency-response measurements must determine whether this is the right final behavior.
- Punish multiplies the pre-saturation signal by 1 to 3.5. It is intentionally not an output boost.
- High Cut uses a first-order low-pass. Steep cascades a second pole. Both poles continue updating so toggling Steep does not expose stale filter state.
- Auto Gain compares smoothed mean-square dry and wet levels. It uses one shared gain for all channels, preserving stereo balance. Correction is bounded to 0.25x–4x. This is an engineering starting point, not a claim of perceptual loudness matching.
- All new parameters are appended. Existing v0.1 parameter IDs, ranges, defaults and order are unchanged.

## Validation status

- Standalone nonlinear core regression: 10 test groups pass with GCC C++20 optimized build.
- Full JUCE processor and editor compilation: not run in this environment because JUCE is fetched by CMake and no local JUCE checkout is available.
- Windows VST3, FL Studio listening, filter response plots, stereo correlation, Auto Gain pumping, latency, CPU and automation stress: not yet validated.

## Next engineering gates

1. Run Windows GitHub Actions and resolve all compile errors before installation.
2. Verify that all new APVTS parameters appear and automate correctly in FL Studio.
3. Measure Low Cut / High Cut frequency response at 44.1, 48 and 96 kHz.
4. Verify Thump does not create excessive low-frequency buildup.
5. Compare Auto Gain OFF/ON using identical vocal, bass, drum and mix passages. Check RMS and short-term loudness, pumping, noise lift and stereo image.
6. Only after these checks, refine the controls and calibrate the four saturation characters.
