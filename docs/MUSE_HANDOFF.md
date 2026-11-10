# VoVolt DSP v0.4 | Muse handoff

This repository is VoVolt ONLY (C++20, JUCE 8.0.8, Windows VST3), not MELOTHÉON.

## Scope

1. Inspect every source file, CMakeLists.txt and `.github/workflows/windows.yml` BEFORE changing anything.
2. Preserve `PluginEditor.cpp`, `PluginEditor.h` and `VoVoltLookAndFeel.h` exactly unless a verified **compile error** absolutely requires a minimal fix. Preserve the light-blue approved UI, controls and layout.
3. Preserve the existing JUCE parameter IDs, their order, ranges and defaults, and `PLUGIN_MANUFACTURER_CODE`, `PLUGIN_CODE` and `BUNDLE_ID`. Existing DAW projects must retain mappings.
4. Run the Windows GitHub Actions workflow. Build VST3 + Standalone, then run CTest in Release mode. Fix errors with minimal changes, never suppress compiler diagnostics by removing functional code or deleting tests.
5. Verify project host behavior in FL Studio: mono/stereo, host sample rates 44.1/48/96 kHz, buffer sizes 64/128/512/1024/4096, zero input, block sizes larger than prepareToPlay(), and repeated DAW automation for Character, Drive, Tone, Mix, Output and Bypass.
6. Verify the `useIntegerLatency=true` oversampler and dry-delay ring with impulses. At `Mix=0` compare the processed plugin output to dry audio shifted by exactly the reported latency. At `Mix=50%`, measure phasing/comb filtering at `Drive=0`, accounting for the current wet Tone/DC processing.
7. Measure aliasing with high-frequency sinusoidal inputs (4x oversampling enabled), level-normalized harmonics, DC offset, CPU and denormals. Do not report subjective quality as objectively verified without listening tests.
8. Audit for real-time allocations, threading hazards, NaN/Inf propagation, large block handling and crashes on transport/seek/stop. Use a debug/ASan build as applicable.
9. Report actual build logs, errors, compiler warnings, host test results and any unverified areas. Do not claim Windows VST3 verified until CI passes. Do not introduce new product features, clone Decapitator or modify UI by preference.

## Expected output

Working Windows Release VST3 artifact, CTest results, a short list of changed files, and a scoped list of remaining DSP tasks (including hardware-style Tube/Tape research and loudness/aliasing measurements).
