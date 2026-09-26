# Architecture - Copper IPTV Player

Copper is a Qt 6 / C++17 desktop application for Windows. It is layered so
that the playlist/parsing code is pure and testable, network and I/O happen
without blocking the GUI, and the widget layer only talks to stable
interfaces.

```
+-----------------------------------------------------------------+
| ui/widgets  MainWindow, PlaylistSidebar, GroupListWidget,       |
|             ChannelListWidget, VideoPlayerWidget, ControlBar    |
|      |  signals/slots                                            |
+-----------------------------------------------------------------+
| ui/models   ChannelListModel, GroupListModel, PlaylistListModel,|
|             ChannelFilterProxyModel, ChannelItemDelegate        |
+-----------------------------------------------------------------+
| app  Application (owns services + MainWindow)                   |
|      |                                                          |
|      v                                                          |
| playlist  PlaylistManager | player  PlaybackController -> IPTVPlayer
|      |                    |
|      v                    v
| network  NetworkManager, PlaylistDownloader, ChannelLogoLoader
|      |
|      v
| database  DatabaseManager (SQLite)
|      |
|      v
| core  AppSettings (QSettings), Logger, AppInfo (generated)
+-----------------------------------------------------------------+
```

## Process / thread model

- All Qt widgets and models live on the GUI thread.
- Remote playlist downloads run through `QNetworkAccessManager` (asynchronous,
  no threads).
- Parsing of large payloads happens on the QtConcurrent thread pool
  (`PlaylistManager::startParse`); the `QFutureWatcher` marshals the result
  back to the GUI thread.
- Channel logos are fetched asynchronously via `NetworkManager`, cached in
  `QPixmapCache`, and the list view is repainted on each `logoReady` signal.

## Modules

### core
- `AppSettings` - typed access to `QSettings`: window geometry/state, theme,
  volume/mute, last playlist id + channel URL, sidebar/channel-list
  visibility, compact rows, network timeouts, User-Agent, autoplay, resume
  flag, auto-refresh interval, link overrides.
- `AppInfo` - generated at configure time from `CMakeLists.txt`
  (`src/core/AppInfo.h.in` -> `build/*/generated/core/AppInfo.h`). Holds
  org/app/version/URL constants; the URLs default to empty so no repository
  link is invented.
- `Logger` - centralized message handler that mirrors `qDebug/qInfo/qWarning`
  into a rolling log file under the AppData location and keeps a ring buffer
  for the crash window.
- `Constants.h` - shared numeric constants (timeouts, limits, refresh
  intervals).

### models (value types)
- `Channel` - `Q_GADGET` with name, `groupTitle`, URL, `tvgId`, `tvgName`,
  `logoUrl`, language, country, free-form `attributes` hash, and `isFavorite`
  flag. Registered with QMetaType so it travels in `QVariant` model data.
- `ChannelGroup` - group name plus channel count, built by `ChannelUtils` in
  `Channel.cpp`.
- `Playlist` - id, name, `SourceType` (Local/Remote), source string, enabled,
  channel count, last refresh.

### playlist
- `M3UParser` - pure static parse of `QString` -> `Result{channels, warnings,
  skippedLines}`. Never throws; malformed lines are skipped and counted.
  Details:
  - `#EXTM3U` header, `#EXTINF` with quoted/unquoted attributes and a name
    after the last unquoted comma, `#EXTGRP` override of `group-title`.
  - Attribute keys: tvg-id, tvg-name, tvg-logo, group-title, tvg-language,
    tvg-country; everything else stays in `Channel::attributes`.
  - Three-line entries (`#EXTINF` with no inline name followed by a bare
    name line) are supported.
  - Only `http://`/`https://` URLs become channels.
- `PlaylistManager` - owns playlist CRUD, favorites, refresh/auto-refresh,
  and the QA-system. Implements decode with UTF-8 + BOM handling and a
  Latin-1 fallback (`decodePayload`), then runs `M3UParser::parse` on a
  worker. On a failed refresh the previous channel snapshot is kept; on
  success channels are replaced in one transaction.

### network
- `NetworkManager` - thin `QNetworkAccessManager` wrapper: request factory
  that injects the configured User-Agent, AgressiveRedirectPolicy (target
  based), per-request timeout support, and SSL-error rejection with an
  "ignore TLS errors" escape hatch.
- `PlaylistDownloader` - GET + timeout + status-code check + TLS policy.
  Emits `finished(token, Result)` where `Result.data` is the raw payload.
- `ChannelLogoLoader` - async logo downloader keyed by URL with
  `QPixmapCache` storage; placeholder pixmap fallback.

### player
- `IPTVPlayer` - owns `QMediaPlayer` + `QAudioOutput` + `QVideoWidget`.
  Maps backend `QMediaPlayer::Error` into friendly strings + an
  `ErrorCategory` (InvalidUrl / Network / UnsupportedMedia / Unavailable /
  Backend). Emits playback-state and friendly-error signals.
- `PlaybackController` - single-stream guarantee (stop -> setSource -> play),
  next/previous over the *filtered* channel order, current-channel tracking,
  and selection synchronization with the channel list view.

### database
- `DatabaseManager` - opens `copper.db` under
  `QStandardPaths::AppDataLocation`, WAL mode, schema versioning with
  migrations. Tables:
  - `playlists(id, name, source_type, source, enabled, last_refresh, created_at)`
  - `channels(id, playlist_id, name, group_title, url, tvg_id, tvg_name,
    logo, language, country, attrs_json)` - cached snapshot, replaced on
    refresh.
  - `favorites(id, playlist_id, channel_url UNIQUE, name, group, logo, tvg_id)`

### ui/widgets
- `MainWindow` - central `QSplitter`: left = `PlaylistSidebar` (playlist list
  + group list), center = `VideoPlayerWidget` + `ControlBar`, right =
  `ChannelListWidget` (search + channel list). Owns menus, shortcuts,
  drag&drop import, fullscreen handling, and state serialization. The video
  area gets the largest share of space.
- `PlaylistSidebar` - vertical stack of the playlist list and the group list,
  forwarding selection changes.
- `GroupListWidget` - group model list (with "All channels" and "Favorites"
  rows), populates the channel filter.
- `ChannelListWidget` - `QListView` + `ChannelItemDelegate` (name, group,
  live badge, logo), instant search box, empty-state page, and a context
  menu. Return/Enter and double-click play.
- `VideoPlayerWidget` - `QStackedWidget` of Welcome (`app.png`), Loading,
  Video, and Error pages; double-click requests fullscreen.
- `ControlBar` - transport buttons (prev/play/stop/next), volume slider, mute
  and fullscreen toggles; glyphs are text-based so themes stay consistent.

### ui/models
- `ChannelListModel` - flat `QAbstractTableModel` (rows) over
  `QVector<Channel>`; exposes `Channel` via `ChannelRole`.
- `ChannelFilterProxyModel` - `QSortFilterProxyModel` filtering by group key
  (`""` = all, `"favorites"` = favorites, otherwise a group name compared
  case-insensitively) and by search text across name, tvg-id, group, country,
  and language. Exposes `refilter()` for the source-model reset path.
- `GroupListModel` - derived groups with counts; turns the favorites set into
  a "Favorites" pseudo-group.
- `PlaylistListModel` - playlist list presentation (name, enabled state,
  channel count).
- `ChannelItemDelegate` - paints logo (cached), name, and sub-text with
  compact/normal row modes.

### ui/dialogs
- `AddPlaylistDialog` - local file or remote URL; validates via
  `PlaylistManager::addLocalPlaylist` / `addRemotePlaylist`.
- `PlaylistManagerDialog` - table of playlists with add/edit/refresh/remove
  and enable checkbox.
- `SettingsDialog` - General / Playback / Interface / Network tabs.
- `AboutDialog` - version, build date, Qt version, description, and optional
  GitHub/docs links from the generated `AppInfo`.
- `ChannelInfoDialog` - pretty channel details with credentials masked and the
  fetched logo.

### app
- `Application` - creates `AppSettings`, `DatabaseManager`, `NetworkManager`,
  `ChannelLogoLoader`, `PlaylistManager`, then `MainWindow`. Wires the
  settings-derived startup behavior (auto refresh interval, last-channel
  resume via `PlaybackController`).

## Key design decisions

- **Value types over QObjects** - `Channel`/`ChannelGroup`/`Playlist` are
  `Q_GADGET` value types registered with QMetaType; they move freely between
  models and workers without ownership headaches.
- **Filtering is model-level** - search and group filtering happen inside the
  proxy model, so switching groups or typing never reloads a playlist.
- **Async everywhere** - network requests are signal/slot, parsing uses
  QtConcurrent, logos use `QPixmapCache`; the GUI thread never blocks on I/O.
- **Last-good-wins** - a playlist refresh that fails leaves the persisted
  snapshot and channel count untouched.
- **Single stream rule** - `PlaybackController` guarantees only one source is
  ever set up at a time.
- **Programmatic UI** - no `.ui` files; theme switching and any future RTL
  changes stay consistent because layout code is the single source of truth.
- **No invented URLs** - project links are build-time cache variables that
  default to empty.

## Future-proofing notes

- `QVideoWidget` is used as the QWidget video surface (stable in the Qt 6
  LTS line). If a future Qt major deprecates it, only `IPTVPlayer`
  (replacing `setVideoOutput`) and `VideoPlayerWidget::videoSurface()` need
  to change - the rest of the app talks to `IPTVPlayer`.
- `M3UParser` is deliberately free of Qt widgets and I/O so the same code can
  be reused by a non-GUI service or a web backend.