#include "support/file_dialog.h"

#import <AppKit/AppKit.h>
#import <dispatch/dispatch.h>

static std::filesystem::path path_from_url(NSURL* url) {
    return std::filesystem::path([[url path] fileSystemRepresentation]);
}

static NSURL* url_from_path(std::filesystem::path const& path) {
    auto const& native_path = path.native();
    NSString* ns_path = [[NSFileManager defaultManager]
        stringWithFileSystemRepresentation:native_path.c_str()
                                    length:native_path.size()];
    return [NSURL fileURLWithPath:ns_path];
}

void mosaic::show_save_panel(
    std::function<void(std::filesystem::path const&)> on_completion,
    std::function<void()> on_cancel) {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSSavePanel* panel = [NSSavePanel savePanel];
      [panel beginWithCompletionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK) {
            NSURL* url = [panel URL];
            if (url && on_completion) {
                auto path = path_from_url(url);
                on_completion(path);
            }
        } else if (on_cancel) {
            on_cancel();
        }
      }];
    });
}

void mosaic::show_open_panel(
    OpenPanelDesc desc,
    std::function<void(std::span<std::filesystem::path const>)> on_completion,
    std::function<void()> on_cancel) {
    dispatch_async(dispatch_get_main_queue(), ^{
      NSOpenPanel* panel = [NSOpenPanel openPanel];
      panel.canChooseFiles =
          desc.input_type == OpenPanelInputType::files ||
          desc.input_type == OpenPanelInputType::files_and_directories;
      panel.canChooseDirectories =
          desc.input_type == OpenPanelInputType::directories ||
          desc.input_type == OpenPanelInputType::files_and_directories;
      panel.allowsMultipleSelection = desc.allow_multiple_selection;
      [panel beginWithCompletionHandler:^(NSModalResponse response) {
        if (response == NSModalResponseOK) {
            std::vector<std::filesystem::path> paths;
            for (NSURL* url in [panel URLs])
                paths.push_back(path_from_url(url));
            if (on_completion) on_completion(paths);
        } else if (on_cancel) {
            on_cancel();
        }
      }];
    });
}

void mosaic::show_item_in_system_file_manager(std::filesystem::path path) {
    NSURL* url = url_from_path(path);
    if (url)
        [[NSWorkspace sharedWorkspace]
            activateFileViewerSelectingURLs:@[ url ]];
}