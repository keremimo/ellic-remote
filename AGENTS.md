# Source attribution

- New Melodee code is credited to `Kerem Kilic (Ellic Studio)`; do not copy Leo Kuroshita's copyright header into independently written files.
- Preserve the applicable SPDX licence identifier and all actual third-party notices.
- For mixed files, credit Melodee's work and identify retained Felucca code with `Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments`.
- Check source history and provenance when moving, extracting, porting or replacing code. A shared licence, include, protocol, or generic boilerplate does not establish authorship.
- Keep original notices on unchanged upstream code. Do not remove attribution merely because a derived file has extensive modifications.

# This repository

- Melodee Remote is MPL-2.0 file by file. Take nothing from Saadet but Kerem Kilic's own lines (check `git blame` there),
  and never its factory sound data, engines, Felucca-derived UI, or GPL-3.0-only tools and web pages.
- fm1-base is a pinned dependency: change it only through `tools/base_overlay.py`, whose `replace` fails when a line
  it changes is gone.
- fm1-base's platform shares the app's namespace (one unity build): it already uses `remote` and `mr_`/`MR_` (its
  screen mirror). The app's own names start with `mel_`/`MEL_`.
- `docs/PROTOCOL.md` and `app/mel_proto.h` describe the same protocol: change them together, and only by adding.
