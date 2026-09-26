# Copper IPTV Player - Implementation Plan

## 1. Environment (verified)

| Item | Value |
|---|---|
| Qt | 6.11.2 `mingw_64` at `C:/Qt/6.11.2/mingw_64` (Core, Gui, Widgets, Network, Multimedia, MultimediaWidgets, Sql+sqlite driver, Test) |
| Compiler | MinGW-w64 GCC 13.1.0 (`C:/Qt/Tools/mingw1310_64`) |
| Tooling | CMake 4.4.3, Ninja (`C:/Qt/Tools/Ninja`), `windeployqt` |
| Assets | `Assets/app.ico`, `Assets/app.png` (folder is `Assets`, capital A - used as-is, never modified) |
| Repo | project dir is NOT a git repo |

## 2. Toolchain & build strategy

- Modern CMake, `cmake_minimum_required(VERSION 3.21)`, `project(CopperIPTVPlayer VERSION 0.1.0)`.
- `qt_standard_project_setup()`, AUTOMOC/AUTORCC/AUTOUIC ON, C++17.
- `find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Network Multimedia MultimediaWidgets Sql)` + `Test` for tests.
- No hard-coded Qt paths in CMakeLists (located via CMAKE_PREFIX_PATH).
- `CMakePresets.json` (schema v6): `windows-debug`/`windows-release` using Ninja + MinGW. Presets prepend PATH with MinGW + Ninja bins, set `CMAKE_PREFIX_PATH` from `$env{CMAKE_PREFIX_PATH:default}` falling back to `C:/Qt/6.11.2/mingw_64`, set the C++ compiler explicitly (avoid VS pickup).
- Tests via `include(CTest)` / `BUILD_TESTING` (default ON), runnable with `ctest`.

## 3. Identity / icon / resources

- Project version is the single source of truth (`project(VERSION 0.1.0)`); a generated `AppInfo.h` provides APP_NAME / ORG_NAME / VERSION / links.
- `resources/resources.qrc` embeds the two assets (aliased to `/assets/...`) plus `dark.qss` / `light.qss`.
- `resources/win32.rc` references `../Assets/app.ico` (relative path) so the executable carries the Copper icon (windres with MinGW).
- Window/taskbar icon set from the embedded PNG via `QApplication::setWindowIcon`.

## 4. Layout

```
root/
├─ CMakeLists.txt  CMakePresets.json  README.md  LICENSE  plan.md
├─ Assets/                     app.ico, app.png (untouched)
├─ resources/                  resources.qrc, dark.qss, light.qss, win32.rc
├─ src/
│  ├─ main.cpp
│  ├─ app/         Application
│  ├─ core/        AppInfo.h.in, Constants.h, Logger, AppSettings
│  ├─ models/      Channel, ChannelGroup, Playlist
│  ├─ playlist/    M3UParser, PlaylistManager
│  ├─ network/     NetworkManager, PlaylistDownloader, ChannelLogoLoader
│  ├─ player/      IPTVPlayer, PlaybackController
│  ├─ database/    DatabaseManager
│  └─ ui/
│     ├─ MainWindow.cpp/h
│     ├─ ControlBar.h/.cpp
│     ├─ models/   ChannelListModel, GroupListModel, PlaylistListModel, ChannelFilterProxyModel, ChannelItemDelegate
│     ├─ widgets/  PlaylistSidebar, GroupListWidget, ChannelListWidget, VideoPlayerWidget
│     └─ dialogs/  AddPlaylistDialog, PlaylistManagerDialog, SettingsDialog, AboutDialog, ChannelInfoDialog
├─ tests/          test_m3uparser.cpp, test_playlist.cpp, CMakeLists.txt
└─ docs/           ARCHITECTURE.md
```

UI is built programmatically (no .ui files) so theme switching and RTL stay consistent.

## 5. Data layer

- QSettings (`AppSettings`): window geometry/state, theme, volume/mute, last playlist id + channel url, UI visibility, network timeouts, UA, autoplay/resume flags, refresh interval, link URLs.
- SQLite (`database/copper.db` under `QStandardPaths::AppDataLocation`):
  - `playlists(id, name, source_type, source, enabled, last_refresh, created_at)`
  - `channels(id, playlist_id, name, group_title, url, tvg_id, tvg_name, logo, language, country, attrs_json)` = cached snapshot
  - `favorites(id, playlist_id, channel_url UNIQUE, name, group, logo, tvg_id)`
  - schema_version + WAL + error-tolerant migrations.

## 6. M3U parser

Pure string parser, tolerant:
- `#EXTM3U`, `#EXTINF` (quoted + unquoted attrs, name-after-last-unquoted-comma), `#EXTGRP` fallback group.
- Attributes: tvg-id, tvg-name, tvg-logo, group-title, tvg-language, tvg-country, plus generic leftovers.
- CRLF/LF, UTF-8 (+BOM), Latin-1 fallback decode.
- Malformed lines skipped + logged, never crashes. Missing group -> "Uncategorized".
- Only http/https URLs accepted (validated); all others skipped with a log line.
- Parsing of large payloads runs via QtConcurrent, result marshaled back on the GUI thread.

## 7. Playlist manager & network

- `PlaylistManager`: add local / add remote (async) / remove / rename / enable-disable / refresh (async, keeps last-good snapshot on failure) / auto-refresh QTimer (Off, 15m, 30m, 1h, 6h, 12h, 24h - remote only).
- `NetworkManager`: QNAM wrapper; redirects via NoLessSafeRedirectPolicy, timeouts, SSL-error policy (default: fail; optional "ignore TLS errors" setting), HTTP status checks, User-Agent from settings.
- `PlaylistDownloader`: async GET -> decode -> parse; status flow Downloading -> Parsing -> Loaded N.
- `ChannelLogoLoader`: async logo fetch, QPixmapCache-backed; never blocks the GUI; generic placeholder fallback.
- Startup: load playlist sources + cached channel snapshots from DB instantly, then auto-refresh remote lists whose interval elapsed.

## 8. Player

- `IPTVPlayer` (mediator-free): QMediaPlayer + QAudioOutput + QVideoWidget; play/pause/stop/volume/mute; maps `errorOccurred` to friendly categories (InvalidUrl / Network / UnsupportedMedia / Unavailable / Backend).
- `PlaybackController`: single-stream guarantee (stop -> setSource -> play), Next/Prev over the filtered channel order, selection sync, auto-resume.
- `VideoPlayerWidget`: stacked Welcome (app.png) / Loading / Video / Error; double-click toggles fullscreen.

## 9. UI

- MainWindow: horizontal QSplitter - left sidebar (playlist list + group list), center video + ControlBar, right channel list (search + list). Videos get majority space.
- Menus: File (Open Playlist File, Add Playlist URL, Manage Playlists, Export Favorites, Exit), Playlist (Add, Refresh, Remove, Rename), View (Sidebar, Channel List, Fullscreen, Dark/Light), Playback (Play/Pause/Stop/Prev/Next/Fullscreen), Settings, Help (About, GitHub, Documentation).
- Links live in one place (AppInfo + AppSettings override); defaults empty so no repository URL is invented - unconfigured links show an explanatory message.
- Dialogs: Add (local/URL), Manage (table + Add/Edit/Refresh/Remove/Close), Settings (General/Playback/Interface/Network tabs), About (app.png, version, license, link), ChannelInfo (credentials masked).
- Context menu: Play / Add-Remove Favorite / Copy Stream URL / Copy Channel Name / Channel Information.
- Model/view everywhere: custom QListView + delegate; instant, case-insensitive search over name/tvg-id/group/country/language without reloading.
- Fullscreen = full-window (controls + status bar stay usable); F and double-click toggle, Esc exits, playback preserved.
- Shortcuts scoped so typing in fields is never hijacked (Space handled at widget scope, not window-global).
- Drag & drop local .m3u/.m3u8 -> import; invalid -> friendly error.
- Dark theme (default) + light theme, applied app-wide via QSS; RTL-ready (layouts, no hard-coded LTR positions).
- Status bar: Ready / Loading… / Downloading… / Parsing… / Loaded N / Playing: X / Playback error / Playlist refreshed.

## 10. Settings

General (start with Windows via registry, remember last channel, remember window size/pos, confirm before removing), Playback (default volume, autoplay, resume last channel on restore - default ON), Interface (theme, sidebar visibility, channel list visibility, compact rows), Network (timeouts, UA, auto-refresh interval, ignore-TLS option).

## 11. Tests (Qt Test)

- `test_m3uparser`: valid / empty / multi-channel / groups / logos / missing metadata / malformed / URLs with query params / UTF-8 (Arabic, English, French, Japanese) / CRLF+BOM / EXTGRP / skipped counts.
- `test_playlist`: add/remove channels, group building, favorites logic, search matching across fields, duplicate URLs.

## 12. Documented limitations

- Codec support = whatever the Qt FFmpeg (default) / Windows Media Foundation backend decodes; unplayable streams produce a friendly error, never a crash.
- QVideoWidget is the stable QWidget video surface in Qt 6.11 (not yet deprecated); future-proofing via ARCHITECTURE.md note (QVideoSink swap).
- No bundled codecs/engines (no FFmpeg/VLC/yt-dlp bundling), no credentials storage, no telemetry, no bundled playlists.
- No hard-coded repo URL (configurable links).

## 13. Verification sequence

1. Scaffold + core -> build.
2. Models + parser + tests -> build + ctest.
3. DB + network + playlist manager.
4. Player + controls.
5. Full UI + dialogs + themes.
6. Full build (Debug + Release), ctest green.
7. Smoke run (sample M3U, search, favorites, fullscreen, restart persistence, icon).
8. windeployqt verification of a standalone Release tree.
9. README + ARCHITECTURE.md finalized to match code.

## 14. Deliverable

Working executable `build/bin/CopperIPTVPlayer.exe` (Release), full C++17/Qt6 source, tests, README, ARCHITECTURE.md.