# Prompt for Muse

Work ONLY in the new VoVolt repository. This is an independent C++20 / JUCE 8 VST3 project, unrelated to MELOTHÉON.

1. Inspect every source file, CMakeLists.txt, and the Windows GitHub Actions workflow.
2. Compile with Visual Studio 2022 on Windows, fix all compile and link errors with minimal targeted edits. Do not claim a successful build without the actual logs.
3. Run CTest. Inspect and fix any failing tests without removing them.
4. Check the oversampling dry/wet latency alignment using impulses and 50% mix. Verify phase behavior and no comb filtering beyond the expected nonlinear processing.
5. Verify mono and stereo, zero input, changing Drive/Mix/Tone/Output during playback, automation, preset save/reload, bypass and FL Studio VST3 scanning.
6. Measure aliasing on high-frequency sine waves, CPU usage and signal clipping. Report results, not assumptions.
7. Keep the approved minimal light-blue VoVolt design. Do not redesign it into a dark/celestial interface.
8. Do not add new effects or modify MELOTHÉON.
9. Report changed files, actual build/test status, known limitations, and next recommendations. Stop after the v0.1 verification.
