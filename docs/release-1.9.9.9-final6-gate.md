# Final6 release verification

- Build and test Windows Release from the reviewed tree and from an independent extraction of the exact source ZIP. Run CTest and the native application smoke suite on both builds.
- Verify separate 7+1 and 8K bindings through save/reload, NKRO and gameplay. Check that a scratch-free 8K conversion uses the 8K bindings.
- Compare rendered 16-lane previews at the center gap. Verify red scratch defaults, profile color precedence and the ghost field's long-note rendering.
- Check the packaged offline editor, both ZIP inventories and CRCs, SHA-256 checksums and the exact Git blobs in the source ZIP.
- Verify the PR and merged-commit CI runs. Read back all three GitHub release assets and compare them to the audited archives and checksum file.

Physical input, extended play and additional PCs require hands-on verification.
