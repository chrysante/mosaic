#pragma once

#include <filesystem>

#include <mosaic/support/export.h>

namespace mosaic {

/// \returns the directory containing the application's bundled resources.
///
/// Use this for read-only assets shipped with the application, such as fonts,
/// images, shaders, and other files included in the app bundle.
///
/// On macOS, this resolves to the main bundle's Resources directory.
/// The returned directory is managed by the application bundle and should
/// not be used for user-generated or writable files.
MOSAIC_EXPORT std::filesystem::path resource_directory();

/// \returns the directory for application configuration files.
///
/// Use this for user preferences and settings that should persist between
/// launches, such as application configuration files and UI layout state.
///
/// On macOS, this resolves to:
///
//     ~/Library/Application Support/<bundle identifier>/config/
///
/// The directory is created if necessary.
MOSAIC_EXPORT std::filesystem::path config_directory();

/// \returns the directory for persistent application data.
///
/// Use this for application-owned files that are not configuration, such as
/// databases, indexes, and other persistent state.
///
/// On macOS, this resolves to:
///
///     ~/Library/Application Support/<bundle identifier>/data/
///
/// The directory is created if necessary.
MOSAIC_EXPORT std::filesystem::path data_directory();

/// \returns the directory for disposable application cache files.
///
/// Use this for data that can be regenerated if it is deleted, such as
/// cached computations, downloaded temporary assets, and intermediate files.
/// Do not use this for irreplaceable user data or settings.
///
/// On macOS, this resolves to:
///
///     ~/Library/Caches/<bundle identifier>/
///
/// The directory is created if necessary.
MOSAIC_EXPORT std::filesystem::path cache_directory();

} // namespace mosaic