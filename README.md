# Copper IPTV Player

A modern IPTV player for Windows built on Qt 6, C++17, and CMake. It plays
M3U/M3U8 playlists over HTTP/HTTPS or from local files using the Qt Multimedia
backend (FFmpeg / Windows Media Foundation depending on your Qt build).

Copper keeps playlist metadata, channel snapshots, and favorites in a local
SQLite database, and everything else (window geometry, theme, volume, network
options, resume behavior) in QSettings.

## Features

- **Playlists** - add local `.m3u`/`.m3u8` files or remote URLs, enable/disable,
  rename, remove, refresh manually, and auto-refresh remote playlists
  (off / 15 min / 30 min / 1 h / 6 h / 12 h / 24 h).
- **Tolerant M3U parser** - `#EXTINF` (quoted and unquoted attributes, names
  after the last unquoted comma), `#EXTGRP` fallback groups, CRLF/LF, UTF-8
  with BOM, Latin-1 fallback; malformed lines are skipped, never fatal.
- **Groups** - channels are grouped by `group-title` (missing groups fall back
  to "Uncategorized"), with a dedicated Favorites group.
- **Instant search** - case-insensitive filtering over name, tvg-id, group,
  language, and country without reloading the playlist.
- **Playback** - play/pause/stop, next/previous channel, volume, mute, and
  fullscreen (F, Enter on a selected row, double-click, Escape).
- **Favorites** - add/remove from the playlist list, a group, or the channel
  context menu; persisted per stream URL.
- **Channel context menu** - Play, toggle favorite, copy stream URL, copy
  channel name, view channel information (credentials are masked).
- **Persistence** - remembers the window size/position, theme, volume/mute,
  sidebar and channel-list visibility, the last active playlist, and can
  resume the last channel on startup.
- **Themes** - dark (default) and light themes applied app-wide via stylesheets.
- **Drag & drop** - drop a local `.m3u`/`.m3u8` file onto the window to import.
- **Local-first** - cached channel snapshots load instantly from SQLite,
  remote lists refresh in the background (QtConcurrent) and keep the last
  good snapshot if a refresh fails.

## Requirements

| Tool | Version | Typical install |
|---|---|---|
| Qt | 6.2+ (6.11.2 used) | `C:/Qt/6.11.2/mingw_64` |
| Qt components | Core, Gui, Widgets, Network, Multimedia, MultimediaWidgets, Sql (sqlite), Test | |
| Compiler | MinGW-w64 GCC (13.1.0 used) | `C:/Qt/Tools/mingw1310_64` |
| CMake | 3.21+ (4.4.3 used) | |
| Ninja | any recent | `C:/Qt/Tools/Ninja` |

## Building

Development uses a single Debug configuration (one version, 0.1.0). Presets
are declared in `CMakePresets.json` and hardcode the toolchain locations used
on this machine. Adjust the path values if your Qt/Tools directory differs.

```powershell
cmake --preset windows-debug
cmake --build --preset windows-debug
```

Outputs:

- `build/debug/CopperIPTVPlayer.exe`
- `build/debug/bin/copper_m3uparser_tests.exe`
- `build/debug/bin/copper_channelutils_tests.exe`

`CMAKE_PREFIX_PATH` (Qt), the compiler, and Ninja are set inside the preset;
running plain `g++` from a bare shell fails silently unless
`C:/Qt/Tools/Ninja;C:/Qt/Tools/mingw1310_64/bin` is on `PATH` - the Ninja
build sets this automatically via the preset environment.

## Running

Every build automatically runs `windeployqt` as a post-build step, copying the
Qt DLLs, plugins, and MinGW compiler runtime next to the executable - so you
can just double-click it:

```powershell
build\debug\CopperIPTVPlayer.exe
```

`scripts/run.ps1` still works as an alternative launcher (it adds the Qt bin
dir to PATH first).

Only one instance may run at a time - launching a second exits it immediately
(the stale lock of a crashed instance is reclaimed automatically).

First-run tips:

- **File → Add Playlist URL** (or **File → Open Playlist File**) to add a
  source; `sample.m3u` in the repo root is a small test playlist.
- Click a group to filter channels; type in the search box to filter
  instantly.
- Double-click a channel (or press Enter) to start playback.
- Use **F** or double-click the video area for fullscreen.

## Tests

```powershell
ctest --preset windows-debug
```

Two Qt Test suites run without a GUI or a database:

- `copper_m3uparser_tests` - M3U/M3U8 parsing: extended and plain playlists,
  attributes, `#EXTGRP` override, name fallbacks, CRLF/whitespace, malformed
  content, quoted commas in names, UTF-8 BOM.
- `copper_channelutils_tests` - group fallback/ordering/counts, multi-field
  search matching, URL masking, stream-URL detection.

`BUILD_TESTING` defaults to ON; pass `-DBUILD_TESTING=OFF` to skip them.

## Deployment (standalone directory)

During development there is no `dist` tree; run directly from
`build/debug` with `scripts/run.ps1`. When you want a distributable copy,
recreate it with `windeployqt` at that time. When MinGW is used it copies the
compiler runtime (`libgcc_s_seh-1.dll`, `libstdc++-6.dll`,
`libwinpthread-1.dll`) automatically - do **not** pass `--no-compiler-runtime`,
or the executable will not start on machines without MinGW installed. Two
things `windeployqt` does not handle: prune the non-SQLite SQL drivers
(`qsqlibase`, `qsqloci`, `qsqlodbc`, `qsqlpsql`, `qsqlmimer`) which import
third-party client DLLs, and copy `libatomic-1.dll` from the compiler bin.

## Project layout

```
Assets/            app.ico, app.png (used as-is)
resources/         resources.qrc, dark.qss, light.qss
src/
  main.cpp         entry point, QApplication setup
  app/             startup wiring (database, network, playlist, window)
  core/            AppInfo (generated), AppSettings, Logger, constants
  models/          Channel, ChannelGroup, Playlist value types
  playlist/        M3UParser, PlaylistManager
  network/         NetworkManager, PlaylistDownloader, ChannelLogoLoader
  player/          IPTVPlayer, PlaybackController
  database/        DatabaseManager (SQLite schema + migrations)
  ui/              MainWindow, ControlBar, Themes
  ui/models/       list/proxy models, channel item delegate
  ui/widgets/      PlaylistSidebar, GroupListWidget, ChannelListWidget,
                   VideoPlayerWidget
  ui/dialogs/      AddPlaylist, PlaylistManager, Settings, About, ChannelInfo
tests/             Qt Test suites + CMakeLists
docs/              ARCHITECTURE.md
```

See [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the module-by-module
documentation.

## Limitations

- Codec support equals whatever the Qt Multimedia backend decodes. Unplayable
  streams surface a friendly error, never a crash.
- No bundled codecs, credentials storage, telemetry, or bundled playlists.
- The About/Help page links come from the build-time `APP_GITHUB_URL` /
  `APP_DOCS_URL` cache variables; they are empty by default so no repository
  URL is invented. Set them at configure time to populate the links.