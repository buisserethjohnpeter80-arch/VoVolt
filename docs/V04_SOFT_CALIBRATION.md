# VoVolt v0.4 — SOFT calibration, engineering note

## Scope

This is the first DSP calibration step. It changes only the SOFT transfer curve and its standalone regression tests. The UI, parameter IDs/ranges/defaults, JUCE processor, oversampling configuration, and other three character algorithms are unchanged.

## Finding in the previous SOFT curve

The previous transfer was:

`y = x + drive * (tanh(gain * x) / tanh(gain) - x)`

Because `tanh(gain)` approaches 1 while the derivative of `tanh(gain*x)` at zero is `gain`, this normalization caused a large small-signal gain increase as Drive rose. At Drive=0.75, gain was 9.85. For a static input sample of 0.05, the previous curve produced approximately 0.35465, or +17.02 dB relative to the input. At 0.45, it produced approximately 0.86229, or +5.65 dB.

That gain increase can make an un-level-matched SOFT/HARD comparison misleading. It is consistent with the loudness imbalance reported during the FL Studio test, though the exact +3 dB observation still needs a DAW re-test using program material and RMS/loudness matching.

## Updated candidate transfer

The candidate curve is:

`g = 1 + 3.5 * drive`

`soft = tanh(g * x) / g`

`y = x + drive * (soft - x)`

`tanh(g*x)/g` has a derivative of 1 at x=0. This keeps small-signal gain approximately unity while increasing smooth peak rounding as Drive rises. Drive=0 remains an exact identity path. The curve remains odd-symmetric and memoryless.

This is a calibration candidate, not a final tonal approval. Output compensation remains available through the existing Output parameter; no automatic gain or new UI control was introduced.

## Measured core response

Conditions: 1 kHz sine, peak input 0.45, 192 kHz sample rate, 19,200 samples (100 cycles), static SOFT core only, no JUCE oversampler, Tone, Mix, Output, or DAW processing. THD is the RMS sum of harmonics 2–15 divided by the fundamental.

| Drive | Fundamental peak | H3 peak | THD 2–15 | Output RMS | Output peak |
|---:|---:|---:|---:|---:|---:|
| 0.25 | 0.433777 | 0.004856 | 1.122% | 0.306746 | 0.429205 |
| 0.50 | 0.392487 | 0.015528 | 3.984% | 0.277751 | 0.378589 |
| 0.75 | 0.328876 | 0.029071 | 8.979% | 0.233486 | 0.304136 |
| 1.00 | 0.249272 | 0.042618 | 17.575% | 0.178963 | 0.214612 |

The measured curve produces progressively more third-harmonic content and peak compression. These figures describe only the standalone transfer function, not the complete VST3 output.

## Regression checks added

- Small-signal gain remains within 0.1% of unity at tested Drive settings.
- Positive input transfer remains monotonic.
- Larger peaks are compressed rather than boosted.
- Increasing Drive increases compression for the tested signal level.
- Existing odd symmetry, zero-drive identity, harmonic, finite-value, Tape-state, Hard-knee, and segmentation checks remain active.

## Test results

- GCC C++20 optimized build: 10/10 groups passed.
- GCC C++20 `-O3 -DNDEBUG`: 10/10 groups passed.
- AddressSanitizer + UndefinedBehaviorSanitizer: 10/10 groups passed.

## Required next validation

1. Build the Windows VST3 with the repository's Visual Studio 2022 workflow.
2. In FL Studio, compare old/new SOFT using the same audio clip and fixed Drive values.
3. Level-match with Output using RMS or short-term loudness, not peak alone.
4. Listen to vocals, bass, drums, and a full mix. Check low-level detail, consonants/transients, harshness, and noise lift.
5. Only approve the curve after listening. Then freeze SOFT as the reference before calibrating HARD.

No claim is made here that the full VST3, 4x oversampling alias rejection, host latency, or CPU has been validated.
