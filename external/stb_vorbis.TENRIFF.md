# TenRiff stb_vorbis security backport (2026-11-04)

Base: TenRiff `beaa35718ebd1284a208564fec9f5da4440ca58e` (1.9.9.9).
The vendored license at the end of `stb_vorbis.c` is preserved.
Only the following upstream changes are backported; this is not a replacement
with the upstream development branch:

| Upstream commit | Applied boundary |
| --- | --- |
| [f5d198cedbc74bdf246f010cdfc32dc0bd5a88a3](https://github.com/nothings/stb/commit/f5d198cedbc74bdf246f010cdfc32dc0bd5a88a3) | Sizes before multiplication/alignment, zeroed comment pointers, null-safe cleanup, codebook product limit |
| [a5ccde4eac78bdb9158837a71b300c766d7bd977](https://github.com/nothings/stb/commit/a5ccde4eac78bdb9158837a71b300c766d7bd977) | Move codebook bound before length/sorted-entry allocation, including lookup type zero |
| [46614470ccb980c0abb97909ad1041b33c86ea99](https://github.com/nothings/stb/commit/46614470ccb980c0abb97909ad1041b33c86ea99) | Unsigned byte shifts in packet length assembly |
| [4111a9ece0f6785c9f4b1fb3cd1655c562014aee](https://github.com/nothings/stb/commit/4111a9ece0f6785c9f4b1fb3cd1655c562014aee) | Check valid_bits for setup EOF; get_bits returns zero, not EOP |
| [7776735e26e6a75b71732b2d9ac313bcbbdb8e65](https://github.com/nothings/stb/commit/7776735e26e6a75b71732b2d9ac313bcbbdb8e65) | Track temporary allocations through failures, release once, retain custom-buffer stack behavior |

Dependencies/adaptations: 4111a9e's temporary res/break cleanup is superseded by
7776735's central cleanup. Existing local casts and the lookup loop variable
`len` are retained; upstream's intervening `cblen` rename and optional
STB_VORBIS_NO_COMMENTS feature are not needed and were not imported.

Additional local checks reject negative comment counts, EOF within vendor or
comment strings, and a missing/invalid comment framing byte. The wrapper uses
scoped stb ownership during vector allocation. `stb_vorbis_open_file(..., TRUE)`
already closes FILE on setup failure; adding fclose there would double-close.

Probe and decode both parse through start_decoder. max_frames limits decoded
PCM only and is not a header security bound. GameSession's Media Foundation
fallback is retained for non-Vorbis .ogg inputs and installed codec handlers.
The codebook product limit (2^18) deliberately rejects unusually huge custom
codebooks, matching upstream policy; normal libvorbis output remains supported.
This does not claim a general CPU/memory budget or a full parser security audit.

Regression checks: `stb_vorbis_security_tests` instruments only its isolated
parser copy, including every allocation-failure position, truncated setup
packets, codebook bounds and custom buffers. Windows `bms_parser_tests
--filter=ogg vorbis` exercises the production probe/decode and preview paths.
The existing ASan preset builds and runs both targets.
