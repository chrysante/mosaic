#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <span>

#include <mosaic/support/export.h>

namespace mosaic {

/// Displays a system "save" panel
MOSAIC_EXPORT void
show_save_panel(std::function<void(std::filesystem::path const&)> on_completion,
                std::function<void()> on_cancel = {});

enum class OpenPanelInputType { files, directories, files_and_directories };

/// Options for `show_open_panel()`
struct OpenPanelDesc {
    OpenPanelInputType input_type = OpenPanelInputType::files;

    /// Set to `true` to allow selecting multiple files
    bool allow_multiple_selection = false;
};

/// Displays a system "open" panel
/// The span passed to \p on_completion is never empty. If
/// `desc.allow_multiple_selection` is false, the span is guaranteed to contain
/// exactly one element
MOSAIC_EXPORT void show_open_panel(
    OpenPanelDesc desc,
    std::function<void(std::span<std::filesystem::path const>)> on_completion,
    std::function<void()> on_cancel = {});

/// Opens the system file explorer with given path selected
MOSAIC_EXPORT void show_item_in_system_file_manager(std::filesystem::path path);

} // namespace mosaic