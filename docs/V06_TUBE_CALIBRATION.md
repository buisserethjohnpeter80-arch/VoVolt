# V06 — TUBE Calibration (Koren 12AX7 Triode)

Date: 2026-11-11. Head: (see commit message). Status: implemented, 15 test groups
pass locally (GCC C++20, optimized); Windows CI is the final proof for the
VST3 build. Reference: Soundtoys Decapitator style **T** (Thermionic Culture
Culture Vulture, triode) — even harmonics, warm/punchy. Full prep spec:
`~/workspace/goals/vovolt-vocal-saturation-vst3/files/tube-logic-prep-2026-11-11.md`.

## What was wrong with the old Tube

The old Tube was a biased tanh (`tanh(gain*x + bias)`, gain 1+6d, bias
d*(0.14+0.12*envelope), normalized to ±1). Measured 2026-11-11 (1 kHz sine,
fs 192 kHz, head 3d5c157):

- **Harmonics were pentode, not triode:** Drive 0.9, amp 0.45: H3 21.8%
  dominated H2 4.9%. A triode must have H2 dominant.
- **Level ran away:** Drive 0.75, amp 0.45: Tube RMS 0.8195 vs Soft 0.2335
  (+10.9 dB). Drive 1.0: Tube RMS 1.0984 — above full scale.
- **Small-signal gain:** 2.62x at Drive 0.5 — the same class of error the old
  Soft curve had (boosting quiet material).

## The new Tube: Koren 12AX7

Koren's phenomenological triode equations (1996), 12AX7 published set
(mu=100, Ex=1.4, Kg1=1060, Kp=600, Kvb=300), plate voltage fixed at 250 V:

- E1 = (Vp/Kp) * ln[1 + exp(Kp * (1/mu + Vg / sqrt(Kvb + Vp^2)))]
- Ip = (E1^Ex / Kg1) * (1 + sgn E1)  (E1 >= 0 here, so the factor is 2)
- Grid voltage: Vg = biasEff + (2.0 + 6.0*d) * x
- Operating point: bias = -2.0 V; output y = (Ip(Vg) - Ip0) / 0.012, where
  Ip0 = Ip(-2.0 V) removes the DC at zero input.
- Supply-rail soft ceiling: y / (1 + |y| / 1.2) — bounds loud inputs
  without a hard clip.
- Grid-current bias shift (the triode "breathes"): biasEff =
  -2.0 - 0.8 * min(envelope, 1.5); envelope follower attack 4 ms,
  release 100 ms (was 60 ms; 100 ms matches the grid-blocking discharge
  in Danyuk, AES Paper 9124). Loud signals push the operating point
  negative, so the curve itself moves with the signal.

Why Koren and not a bigger-bias tanh: the 3/2-law curvature produces the
asymmetry naturally (positive half conducts, negative half cuts off) — it
is the physics, not a tuned waveshape. A tanh stays a generic curve no
matter how large the bias is.

## Measured verification (prototype, before the plugin change)

Koren prototype, dynamic (bias shift on), 1 kHz sine, amp 0.45, fs 192 kHz:

| Drive | H2 | H3 | H4 | RMS (x Soft) | peak |
|---|---|---|---|---|---|
| 0.25 | 3.5% | 0.1% | 0.4% | 0.86x | 0.39 |
| 0.50 | 12.2% | 0.1% | 2.0% | 0.85x | 0.39 |
| 0.75 | 24.8% | 1.2% | 5.1% | 1.01x | 0.44 |
| 0.90 | 33.1% | 2.3% | 7.6% | 1.25x | 0.49 |
| 1.00 | 38.5% | 3.4% | 9.3% | 1.50x | 0.52 |

- H2 > H3 at every Drive; H2 present from low Drive (3.5% at 0.25);
  H4 present and growing (even-harmonic family).
- The bias shift improves the profile: H3 falls vs the static curve
  (1.2–3.4% vs 4.1–7.3%) while H2 grows.
- Small-signal gain: 0.85 at Drive 0.5, 1.12 at Drive 1.0 (near unity).
- Full-scale input (amp 1.0, Drive 1.0): RMS 0.485, peak 0.86 — bounded.

## Tests added (3 groups, 15 total)

- `testTubeEvenHarmonicOrder`: H2 > H3 at Drive 0.25/0.5/0.75/0.9/1.0;
  H2 > 0.01 at Drive 0.5; H4 > 0.001 at Drive 0.75.
- `testTubeLevelRelationship`: Tube RMS within 0.5x–2.0x of Soft at
  Drive 0.75/1.0 (measured 1.01x / 1.50x).
- `testTubeSmallSignalGain`: gain within 0.7–1.2 at Drive 0.5/1.0
  (measured 0.85 / 1.12).

Existing Tube tests still pass unchanged: `testTubeAsymmetry`
(|pos + neg| > 0.002) and `testHarmonicContent` (H2 > 0.003 at Drive 0.9).

## What is NOT changed

Chain (Low Cut, Thump, Punish 3.5x, Tone post-saturation, High Cut,
Steep, Auto Gain), other modes, parameter IDs, UI. Remaining Decapitator
gaps (decisions, not done): Punish 10x (global), Tone pre-saturation
tilt, Steep 30 dB/oct. FL listening test still required: same vocal,
Drive 0.25/0.5/0.75, level-matched (RMS, Wave Candy) — Tube must sound
warmer/fatter than Soft without being louder, and smoother than Hard
(H2 instead of H3).

Sources: Soundtoys Decapitator User's Guide v5; Sound on Sound (Culture
Vulture review); Danyuk, AES Paper 9124; Koren 1996; Thermionic Culture
Ultra Vulture manual; Sphinx 101 manual.
