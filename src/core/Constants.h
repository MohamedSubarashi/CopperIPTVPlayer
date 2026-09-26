#pragma once

#include <QString>

namespace AppConstants {

inline const QString kLogFileName = QStringLiteral("copper-iptv-player.log");
inline const QString kDataDirName = QStringLiteral("Copper IPTV Player");
inline const QString kDatabaseFileName = QStringLiteral("copper.db");
inline const QString kCacheSubdir = QStringLiteral("cache");
inline const QString kLogsSubdir = QStringLiteral("logs");

// Playlist intervals (minutes). 0 = disabled.
inline const int kAutoRefreshDisabled = 0;
inline const int kAutoRefresh15m = 15;
inline const int kAutoRefresh30m = 30;
inline const int kAutoRefresh1h = 60;
inline const int kAutoRefresh6h = 360;
inline const int kAutoRefresh12h = 720;
inline const int kAutoRefresh24h = 1440;

// Defaults.
inline const int kDefaultNetworkTimeoutMs = 30000;
inline const int kDefaultDownloadTimeoutMs = 60000;
inline const int kDefaultVolume = 70;
inline const int kMaxVolume = 100;
inline const int kMinVolume = 0;
inline const int kDefaultLogoSize = 28;

// Group sentinel names (canonical data values; localized at display time).
inline const QString kGroupUncategorized = QStringLiteral("Uncategorized");
inline const QString kGroupAll = QStringLiteral("__all__");
inline const QString kGroupFavorites = QStringLiteral("__favorites__");

} // namespace AppConstants