# Audio device guidance fix

If gameplay initialization fails because audio output is unavailable, the song-selection footer shows an amber notice. Select a connected device under **Options > Audio > Sound Output Device**, then retry. For ASIO, select an installed ASIO driver.

Previously, the failure text was only drawn inside the difficulty-table URL editor. The notice now uses both visible footer lines and clears after successful session initialization.

No silent gameplay, automatic output switching, audio backend, judgement, scoring or replay changes. The saved device selection remains intact.

Included in 1.9.9.8 hot fix 1 (tag: 1.9.9.8-hotfix1). Fixed baseline: 1.9.6.
