# KeyWeaver E import

Imported from KeyWeaver 1.3.0 commit `4bbf7cbece9657c41bd1fd2cc479155b575056d6` (MIT; see LICENSE).

This directory has a separate `keyweaver_e` namespace and local include paths. The original `nk2/` library remains frozen for Native/Transform/Remaster replays. No models or new runtime dependency are needed; only this library requires C++20.

TenRiff changes the phrase pair validation to allow its existing target menu: 4, 5, 6, 7, 8, 9, 10, 12, 14, and 16 keys. The adapter uses Faithful + phrase search and, only when expanding, phrase support + dense LN + source LN (E). F/boost is never selected. Placement, budgets and the independent safety audit retain the upstream implementation. The runtime adapter separately checks exact sample intervals and suppresses keysounds on generated notes.

The stable preset token is `source_ln_e_v1`. Keep this implementation stable for its replays; future behavior changes require a new identity. Source provenance hashes are recorded in the local candidate's `evidence/upstream-manifest.json`.
