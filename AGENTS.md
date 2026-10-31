# Source attribution

- New Melodee code is credited to `Kerem Kilic (Ellic Studio)`; do not copy Leo Kuroshita's copyright header into independently written files.
- Preserve the applicable SPDX licence identifier and all actual third-party notices.
- For mixed files, credit Melodee's work and identify retained Felucca code with `Portions Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments`.
- Check source history and provenance when moving, extracting, porting or replacing code. A shared licence, include, protocol, or generic boilerplate does not establish authorship.
- Keep original notices on unchanged upstream code. Do not remove attribution merely because a derived file has extensive modifications.

# This repository

- Melodee Remote is MPL-2.0 file by file. Take nothing from Saadet but Kerem Kilic's own lines (check `git blame` there),
  and never its factory sound data, engines, Felucca-derived UI, or GPL-3.0-only tools and web pages.
- The screen is Melodee FM-1's drawing (`~/code/personal/Felucca`, branch `nightly`; call it Melodee FM-1, never
  Felucca), ported in `app/ui/` and `tools/` under MPL-2.0: Kerem Kilic's own code, and Leo Kuroshita's Felucca code
  under his permission (hugelton/felucca-bsp#4: "Anyone may use the Felucca code for which I personally hold the
  copyright under MPL-2.0"; credit him). No other contributor's lines: check `git blame` there before porting more
  (the USB HOST lines in its `ui_draw.c` and `ui_menu.c` are another's).
- Port Melodee FM-1's screens as its renderer draws them now (`tests/ui_render.c`, `build/ui_new/NIGHT`), not its
  older `docs/design/ref` pictures; replace its model's reads with the mirror (`mel_v2_t`, `ui/ui_base.c` V, TSEL).
  `tests/scenes.py` paints each page as Melodee would; `tools/compare_shots.py` puts the shots beside Melodee FM-1's.
- fm1-base is a pinned dependency: change it only through `tools/base_overlay.py`, whose `replace` fails when a line
  it changes is gone.
- fm1-base's platform shares the app's namespace (one unity build): it already uses `remote` and `mr_`/`MR_` (its
  screen mirror). The app's own names start with `mel_`/`MEL_`. fm1-base's gfx.c and Melodee FM-1's share names: the
  app takes Melodee FM-1's under `ux_` names (`app/ui/gfx_names.h`) and gives them back at its end.
- `docs/PROTOCOL.md` and `app/mel_proto.h` describe the same protocol: change them together, and only by adding.
