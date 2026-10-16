#pragma once

#include <filesystem>
#include <functional>
#include <span>

#include <mosaic/support/export.h>

namespace mosaic {

/// Displays a system "save" panel
MOSAIC_EXPORT void
show_save_panel(std::function<void(std::filesystem::path const&)> completion);

/// Options for `showOpenPanel()`
struct OpenPanelDescription {
    ///
    bool resolves_aliases = true;

    ///
    bool can_choose_directories = false;

    /// Set to `true` to only allow selecting multiple files
    bool allows_multiple_selection = false;

    /// Set to `false` to only allow choosing directories
    bool can_choose_files = true;
};

/// Displays a system "open" panel
MOSAIC_EXPORT void show_open_panel(
    OpenPanelDescription const& desc,
    std::function<void(std::span<std::filesystem::path const>)> completion);

/// \overload
MOSAIC_EXPORT void
show_open_panel(OpenPanelDescription const& desc,
                std::function<void(std::filesystem::path const&)> completion);

/// Opens the system file explorer with given path selected
MOSAIC_EXPORT void show_file_in_file_explorer(std::filesystem::path path);

} // namespace mosaic
