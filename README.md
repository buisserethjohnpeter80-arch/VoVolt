# VoVolt v0.1 | Vocal Saturation VST3

Independent saturation plugin. **Not a MELOTHÉON module.**

## Included
- Soft, Tube, Tape and Hard nonlinear character modes
- Drive, Tone, Mix, Output, Bypass
- Mono/stereo processing and level meters
- Fixed 4x oversampling, parameter smoothing, host latency reporting
- JUCE APVTS state persistence and DAW automation
- Simple light-blue UI inspired by the approved VoVolt mockup (not a pixel-perfect copy)
- Windows GitHub Actions build, portable standalone DSP smoke test

## Build (Windows)
Prerequisites: Git, CMake 3.22+, Visual Studio 2022 with C++ desktop workload, internet for first JUCE checkout.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --parallel 4
ctest --test-dir build -C Release --output-on-failure
```

VST3 bundle: `build/VoVolt_artefacts/Release/VST3/`.
Copy the `.vst3` bundle into `C:\Program Files\Common Files\VST3`, then rescan in FL Studio.

## Important engineering status
This is a **first functional source implementation**, not a certified commercial release. The code has not been built against JUCE in this environment. Run the Windows CI build and test it in FL Studio before distribution.

### Known scope and limitations
- Oversampling fixed at 4x. No selectable 1x/2x/8x yet.
- No Vocal Focus, auto gain, A/B presets, harmonic analyzer or advanced panel yet.
- The Tape mode is an original stateful waveshaper, **not** a measured analog tape emulation.
- No loudness matching. Do not mistake output level for subjective sound quality.
- The current wet/dry latency alignment uses integer host latency. Validate impulse timing and phase cancellation before production release.
- Bypass is internally smoothed and latency compensated; it is not JUCE host hard-bypass.
- Large host blocks beyond the prepared maximum are safely passed through unchanged; robust arbitrary block-size chunking is a future improvement.
- GUI uses JUCE native components and a responsive layout; exact font/rendering varies by OS.

## Muse handoff
Upload this project to a **new VoVolt repository**, not the MELOTHÉON repository. Ask Muse to run Windows Actions, fix compile errors only, preserve audio and UI behavior, then test in FL Studio.
