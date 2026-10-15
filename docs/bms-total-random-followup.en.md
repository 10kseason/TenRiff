# BMS TOTAL, authored branches, result colors and 10 mark

Status: included in the user-authorized **1.9.1 release**, along with the inherited audio/input/outro fixes and saved Home language button. The user-pinned baseline remains 1.9.0. Publication and final verification are tracked by the [1.9.1 release gate](release-1.9.1-gate.md); the evidence below records the earlier local BMS/visual stage and does not establish final 1.9.1 delivery.

Localized summaries: [한국어](current-state.md), [English](current-state.en.md), [日本語](current-state.ja.md), [简体中文](current-state.zh-CN.md).

## Scope and rationale

Previously, ordinary BMS gauge changes used fixed TenRiff per-judgement values without the chart's `#TOTAL`. Authored control flow already existed, but branch selection was coupled to the lane/conversion seed and some annotated or incompletely terminated authored patterns were not accepted. Result levels also had a separate purple display path.

The follow-up adds a versioned, BMS-only gauge delta policy and independent authored-branch seed. It shares the song-lobby level presentation with Result and replaces the native menu mark with the user-supplied 10 motif. Optional replay/result metadata preserves historical decisions without changing the existing R5/R6 timing/score identities. These are separate compatibility dimensions: a replay can use a historical timing version with the new gauge version, or the reverse.

## Gauge delta policy

Only new ordinary BMS play uses gauge policy v2. Courses retain their existing course gauge. Protocol v6 peer battle retains gauge v1 because the network protocol does not negotiate the new gauge/branch policies. Replays choose their saved policy; absence means v1.

Let `N` be the valid playable note-head count of the chart presented to the engine after mode transforms. Each tap or LN counts once. LN tails, mines, BGM events and invalid lanes do not add to `N`. A complete ordinary LN still applies TenRiff's existing half-weight head and half-weight tail, so it receives one note's worth of recovery in total. This does not adopt LR2's different LN judgement timing.

Let `T` be a finite positive authored `#TOTAL`. Missing, malformed, nonpositive or nonfinite values use:

```text
T = 160 + (N + clamp(N - 400, 0, 200)) × 0.16
R = T / N
```

An empty chart earns no recovery and never divides by zero. For example, omitted TOTAL gives `T=224` at 400 notes, `256` at 500 notes, `288` at 600 notes and `352` at 1,000 notes.

The full-weight delta table is in percentage points:

| Gauge | PG | GR | GD | BD | Consumed PR | Empty PR |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| Normal | R | R | 0.5R | -4 | -6 | -2 |
| Easy | 1.2R | 1.2R | 0.6R | -3.2 | -4.8 | -1.6 |
| Hard | 0.1 | 0.1 | 0.05 | -6D | -10D | -2D |
| EX | 0.1 | 0.1 | 0.05 | -12D | -20D | -2D |

`D` is the larger of the TOTAL and short-chart damage corrections, not their product:

```text
Dtotal = 10 / clamp(floor(T / 16) - 5, 1, 10)
D = max(Dtotal, Dnotes)
```

| Note count | Dnotes |
| --- | --- |
| N ≤ 20 | 10 |
| 20 < N < 30 | 8 + 0.2 × (30 - N) |
| 30 ≤ N < 60 | 5 + 0.2 × (60 - N) / 3 |
| 60 ≤ N < 125 | 4 + (125 - N) / 65 |
| 125 ≤ N < 250 | 3 + 0.008 × (250 - N) |
| 250 ≤ N < 500 | 2 + 0.004 × (500 - N) |
| 500 ≤ N < 1000 | 1 + 0.002 × (1000 - N) |
| N ≥ 1000 | 1 |

At `T=300,N=1000`, Normal PG is +0.3, Easy PG is +0.36, Hard BD is -6 and EX BD is -12. At `T=200,N=1000`, Hard BD is `-6×10/7`; at `T=300,N=500`, it is -12. Hard applies another ×0.6 to every negative judgement, including empty PR, only when its current value is strictly below 32%. EX has no low-gauge softening. Authored mine damage bypasses these judgement corrections.

TenRiff retains the following behavior:

- All eligible gauges start at 100%; zero is the failure boundary.
- Parallel gauge shift preserves each lower gauge's own accumulated value.
- Judge mode still determines whether an automatic miss is BD or consumed PR. This change does not reclassify notes or replace the judge windows.
- Standard LN head/tail weights, course LN behavior, practice and Sudden Death remain their existing contracts.
- Courses take precedence over the BMS policy. The configured historical `gauge.delta` values remain available for v1 and do not tune BMS v2.

Thus this is an LR2-derived **delta** policy within TenRiff. It is not full LR2 gameplay parity: LR2-style Normal/Easy initial values and clear threshold, Hard failure below 2%, and different LN judging are not adopted for ordinary play.

References checked for the formulas:

- [LR2oraja developer source, GaugeProperty LR2 table](https://github.com/wcko87/lr2oraja/blob/lr2oraja/src/bms/player/beatoraja/play/GaugeProperty.java#L110-L113).
- [LR2oraja developer source, TOTAL and MODIFY_DAMAGE](https://github.com/wcko87/lr2oraja/blob/lr2oraja/src/bms/player/beatoraja/play/GrooveGauge.java#L253-L292).
- [LR2oraja developer README, default TOTAL and Hard threshold](https://github.com/wcko87/lr2oraja).

LR2oraja is an independently maintained compatible player. These references establish its published implementation; they are not a new measurement against the original LR2 executable.

## Authored RANDOM control flow

`#RANDOM N` selects one integer from 1 through N. Its corresponding `#IF` section decides which authored notes, sound definitions and timing commands enter the chart. It does not create N overlaid patterns or force a different branch on every attempt. Supported existing `#SETRANDOM`, nested conditions and switch controls retain their scoped meaning.

```bms
#RANDOM 3 // three authored alternatives
#IF 1
#00111:0100
#ELSEIF 2
#00112:0100
#ELSE
#00113:0100
#ENDIF
#ENDRANDOM
```

New ordinary sessions draw an authored-branch seed independently from lane Random/RR/SR, conversion and LN Mix seeds. Device-rate renegotiation reuses it so opening audio cannot silently select another pattern. Replays load the saved seed. Courses use the new branch parser while retaining their course gauge. Protocol v6 peer battle uses control-flow v1 and branch seed 0 so all existing peers have the same policy without a protocol change.

Control-flow v2 accepts `//` annotations on numeric control commands. It also performs bounded recovery for sibling `#IF` sections with missing `#ENDIF`: recovery is limited to the enclosing RANDOM scope and only applies when the remaining terminators cannot represent valid nesting. Properly terminated nested conditions retain their semantics. Malformed controls produce warnings; this is not a promise to interpret every broken chart. Comment handling applies to control numbers, not URL or asset paths. Historical control-flow v1 retains the old parsing behavior.

## Optional replay and result metadata

The user approved three optional compatibility fields. They are evidence fields, not new profile settings or a migration of old files:

| Field | Meaning when present | If absent |
| --- | --- | --- |
| `bms_gauge_version` | 1: historical deltas; 2: BMS TOTAL delta policy | 1 |
| `bms_control_flow_version` | 1: historical parser; 2: repaired authored control flow | 1 |
| `bms_random_seed` | Unsigned 32-bit authored-branch seed | Historical `random_seed`, then 0 |

Replay stores them inside `mode`; result evidence stores the corresponding top-level fields. New ordinary BMS uses gauge v2. Charts with authored branching also preserve control-flow v2 and the seed. Charts without branching retain control-flow identity v1 and omit the branch seed. Unknown versions or invalid seed values must not silently fall through to a different policy.

Existing replay-format and R5/R6 score IDs remain unchanged. Older readers are not claimed to reproduce the new policy merely because the JSON fields are optional. Replay verification, ghost selection and result comparison must check the applicable versions and branch seed. Local BEST prefers the policy-version tuple instead of letting a historical gauge record automatically win by score alone; result comparisons require matching policy and branch evidence.

New gauge/branch records remain local. Both Sites and ranked upload paths reject these conditions until the server/board verifier supports them. Existing public boards, accounts and stored records are not migrated or deployed by this follow-up.

## Result display and logo

Result level text uses the same level display policy as song selection: red level suffixes, the green circled-five badge only for the selected built-in Aery5 table, and registered 4/6/8/10-key prefix colors. Plain native Lv keeps no Aery badge. This changes presentation, not chart level values or score calculation.

The native menu mark uses the supplied 10-shaped motif. It is adapted for the existing dark UI and small icon sizes. This local change does not update a website or an already installed package.

## Recorded development verification and remaining limits

Before the 1.9.1 Home language addition, Windows Release build passed. The local candidate CTest passed 3/3 with 1,279 cases and zero integration skips; targeted BMS coverage passed 108/108. The offline editor passed 35/35, and the generated vector atlas check passed. Eight native Result/title fixtures presented 480/480 frames and passed 50 geometry checks. Captured level regions contain the requested red and all five table-prefix colors; native Lv contains no green circled-five pixels. Geometry checks are not native input-dispatch proof: these captures did not obtain foreground focus.

The supplied Cultivator chart was checked read-only: seeds 0–3 select all four authored branches. Historical parsing produced 0/17/0/0 notes; v2 retains 657 notes in every branch and preserves the authored tail. Old/new gauge and branch replays reconstruct their own schedules and final gauge values. Package hashes/readback and app-session results are supplied in the accompanying local verification record.

Initial evidence is retained: a restricted build stalled, one test expected a source channel number instead of the existing compact lane, an initial GPU test process exited abnormally, a capture helper used the Windows default text encoding, and an overlapping preview/build caused an asset-copy error. The expectation/UTF-8 helper were corrected; the preview was closed before the successful final build. The final BMS and full CTest runs passed in the normal Windows environment; the first GPU process failure's cause is unconfirmed.

Focused coverage includes the four gauge tables, invalid/default TOTAL, every damage-factor boundary, low-gauge threshold, zero failure and mine isolation, LN divisor/weights, course precedence, parallel shifts, historical opt-out, parser compatibility, independent branch evidence, replay/result matching and upload refusal. Render verification must compare Result and lobby colors and inspect the 10 mark at large and small sizes.

No claim is made here about physical input/audio latency, long sessions, all malformed BMS dialects, other PCs, original-LR2 binary equivalence or online support. Prior release assets, installed profiles, original charts and the audio-controls package are preserved.
