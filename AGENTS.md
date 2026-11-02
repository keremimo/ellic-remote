# Source attribution

- New code is credited to `Kerem Kilic (Ellic Studio)`; do not copy Leo Kuroshita's copyright header into independently written files.
- Preserve the applicable SPDX licence identifier and all actual third-party notices.
- For mixed files, credit Melodee FM-1's work and identify retained Felucca code with `Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments`.
- Check source history and provenance when moving, extracting, porting or replacing code. A shared licence, include, protocol, or generic boilerplate does not establish authorship.
- Keep original notices on unchanged upstream code. Do not remove attribution merely because a derived file has extensive modifications.

# This repository

- The names: this firmware is Ellic Remote, the iOS app it serves is Ellic Studio (its repository still `melodee-ios`,
  this one's still `melodee-remote`). Melodee now means only the separate Melodee FM-1 firmware and the `MELODEE_`/
  `melodee_` names `fm1/` keeps from it. The protocol's "MR" bytes (`F0 7D 4D 52`) stay: changing them is a new protocol.
- Ellic Remote is MPL-2.0 file by file. Take nothing from Saadet but Kerem Kilic's own lines (check `git blame` there),
  and never its factory sound data, engines, Felucca-derived UI, or GPL-3.0-only tools and web pages. The exceptions
  are its web installer (`web/install/`, MPL-2.0 with fm1-base's portions) and its release-site tools, which Kerem
  licenses MPL-2.0 here (LICENSING.md, "From Saadet").
- The remote is a pure remote: it runs Melodee FM-1's pages and controls (`~/code/personal/Felucca`, branch `nightly`;
  call it Melodee FM-1, never Felucca) and keeps no music. Never bring Melodee FM-1's engines, voices, mixer, effects,
  sequencer, pattern store, takes, song chain, projects, presets or undo arena, nor any factory data: the model is
  Ellic Studio's, mirrored (`app/unit/remote_model.c`, `remote_sounds.c`, `remote_project.c`), edits and requests sent to
  Ellic Studio (`app/unit/link.c`).
- `fm1/` is the remote's own, forked from Melodee FM-1 (`fm1/SOURCE`): edit it directly, never sync it again. Its
  lines are Kerem Kilic's and Leo Kuroshita's (Leo's under hugelton/felucca-bsp#4; keep his notices, credit him);
  bring nobody else's. Cut what a remote has no use for (storage, presets, engines' own pages, settings that mean
  nothing to Ellic Studio) rather than stubbing it.
- The unit (`app/unit/unit.c`) is its own compilation unit (fm1.toml `o2`): its names never meet fm1-base's; it reaches
  the platform only through `app/unit/unit.h`.
- fm1-base is a pinned dependency: change it only through `tools/base_overlay.py`, whose `replace` fails when a line
  it changes is gone.
- fm1-base's platform shares app.c's namespace (one unity build): it already uses `remote` and `mr_`/`MR_` (its screen
  mirror).
- `docs/PROTOCOL.md` and `app/unit/link.c` describe the same protocol: change them together; a change Ellic Studio's side
  must follow bumps the version (HELLO), and Ellic Studio (`MelodeeKit/Sources/MelodeeUI/Remote*.swift`) moves with it.
