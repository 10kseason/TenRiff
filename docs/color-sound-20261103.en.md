> Historical local-candidate design/evidence. The included release behavior and launch instructions are in [1.9.9.9](release-1.9.9.9.en.md); local-only/profile notices below describe that earlier candidate. The final Result layout is the restored layout.

# Lobby semantic colors, original result fonts and bundled UI sounds

Local follow-up, 2026-11-03. Retains all earlier feedback features, fixed baseline 1.9.6 and previous releases/installations/profiles. No upload or deployment.

Lobby score numerals use the displayed record's clear palette: existing static EX prism, HD red, NM yellow, EZ green. Neutral states stay neutral. Font objects, coordinates and ordinary white UI remain unchanged. The supplied reference images do not replace the UI structure.

Verified whole-chart completion including LN tails can show FULL COMBO in the MAX COMBO header, using existing lime #CBEF6B with at most .25 bloom. Online summaries, unverified or incomplete records do not imply it. The existing ALL COMBO image policy remains. Result score white, accuracy cyan, NEW gold and judgement/FAST/SLOW roles retain original fonts/colors. Only clear text and the FULL COMBO suffix use their semantic paint.

Five unchanged supplied MP3s are bundled under `Mainmusic/SFX`: button click.mp3 for ordinary buttons; options edit.mp3 for options; Silder Moving.mp3 for sliders; Start button click.mp3 for manual chart start/retry; done.mp3 for successful load/import/index/chart readiness. Dedicated cues suppress a duplicate generic click. Automatic course/restart steps do not play manual Start. Failed/canceled operations do not emit Done.

Audio settings retain file/default/none selectors and existing saved choices. New defaults use bundled files resolved beside the executable, not the working directory. Drop notification stays separate. Done permits four seconds to retain its full supplied three-second clip; other effects stay capped at two seconds. Effects use Windows system output and master/focus mute; BGM keeps the selected WASAPI/ASIO endpoint. Originals are preserved.

[Verification](color-sound-20261103-gate.md). Physical listening/input, arbitrary skins, long sessions and other PCs require separate testing.
