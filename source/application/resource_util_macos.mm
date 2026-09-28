#include "application/resource_util.h"

#include <cstdlib>
#include <vector>

#import <Appkit/Appkit.h>

using namespace mosaic;

std::filesystem::path mosaic::resource_directory() {
    if (auto* dir = std::getenv("RESOURCE_DIR"))
        return dir;
    return std::filesystem::path(
               [NSBundle mainBundle].resourceURL.path.UTF8String) /
           "resource";
}

std::filesystem::path mosaic::library_directory() {
    NSError* error;
    NSURL* file_url =
        [[NSFileManager defaultManager] URLForDirectory:NSLibraryDirectory
                                               inDomain:NSUserDomainMask
                                      appropriateForURL:nil
                                                 create:true
                                                  error:&error];
    return std::filesystem::path(file_url.path.UTF8String) / "Preferences";
}

void mosaic::show_save_panel(
    std::function<void(std::filesystem::path const&)> completion) {
    NSSavePanel* panel = [NSSavePanel savePanel];
    [panel beginWithCompletionHandler:^(NSInteger result) {
      if (result == NSModalResponseOK) {
          NSURL* url = [panel URL];
          completion(url.path.UTF8String);
      }
    }];
}

void mosaic::show_open_panel(
    OpenPanelDescription const& desc,
    std::function<void(std::span<std::filesystem::path const>)> completion) {
    NSOpenPanel* panel = [NSOpenPanel openPanel];
    [panel setResolvesAliases:desc.resolves_aliases];
    [panel setCanChooseDirectories:desc.can_choose_directories];
    [panel setAllowsMultipleSelection:desc.allows_multiple_selection];
    [panel setCanChooseFiles:desc.can_choose_files];
    [panel beginWithCompletionHandler:^(NSInteger result) {
      if (result == NSModalResponseOK) {
          NSArray* urls = [panel URLs];
          std::vector<std::filesystem::path> paths;
          paths.resize(urls.count);
          for (size_t index = 0; auto& path : paths) {
              path = ((NSURL*)[urls objectAtIndex:index]).path.UTF8String;
              ++index;
          }
          completion(paths);
      }
    }];
}

void mosaic::show_file_in_file_explorer(std::filesystem::path path) {
    auto root = path.parent_path();
    [[NSWorkspace sharedWorkspace]
                      selectFile:[[NSString alloc]
                                     initWithUTF8String:path.c_str()]
        inFileViewerRootedAtPath:[[NSString alloc]
                                     initWithUTF8String:root.c_str()]];
}
