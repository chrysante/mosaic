#include "application/directories.h"

#include <stdexcept>
#include <string>

#import <Foundation/Foundation.h>

using namespace mosaic;

static std::filesystem::path path_from_url(NSURL* url) {
    if (!url || !url.fileURL)
        throw std::runtime_error("Expected a valid file URL");
    char const* path = url.fileSystemRepresentation;
    if (!path)
        throw std::runtime_error("Failed to convert URL to filesystem path");
    return std::filesystem::path(path);
}

static std::filesystem::path user_directory(NSSearchPathDirectory directory) {
    NSFileManager* manager = NSFileManager.defaultManager;
    NSError* error = nil;
    NSURL* base_url = [manager URLForDirectory:directory
                                      inDomain:NSUserDomainMask
                             appropriateForURL:nil
                                        create:YES
                                         error:&error];
    if (!base_url) {
        NSString* description = error.localizedDescription
                                    ? error.localizedDescription
                                    : @"Unknown Foundation error";
        throw std::runtime_error(description.UTF8String
                                     ? description.UTF8String
                                     : "Failed to locate user directory");
    }
    return path_from_url(base_url);
}

static std::filesystem::path
ensure_directory(std::filesystem::path const& path) {
    NSError* error = nil;
    NSURL* url =
        [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.c_str()]
                   isDirectory:YES];
    if (!url) throw std::runtime_error("Failed to construct directory URL");
    if (![NSFileManager.defaultManager createDirectoryAtURL:url
                                withIntermediateDirectories:YES
                                                 attributes:nil
                                                      error:&error]) {
        NSString* description = error.localizedDescription
                                    ? error.localizedDescription
                                    : @"Unknown Foundation error";
        throw std::runtime_error(
            description.UTF8String ? description.UTF8String
                                   : "Failed to create application directory");
    }
    return path;
}

static std::string application_identifier() {
    NSString* identifier = [] {
        NSString* identifier = NSBundle.mainBundle.bundleIdentifier;
        if (identifier.length) return identifier;
        NSString* name = NSProcessInfo.processInfo.processName;
        if (!name.length)
            throw std::runtime_error("Unable to determine application name");
        return name;
    }();
    char const* utf8 = identifier.UTF8String;
    if (!utf8)
        throw std::runtime_error(
            "Failed to convert application bundle identifier to UTF-8");
    return utf8;
}

static std::filesystem::path
application_directory(NSSearchPathDirectory base_directory,
                      char const* child_directory = nullptr) {
    auto path = user_directory(base_directory) / application_identifier();
    if (child_directory) path /= child_directory;
    return ensure_directory(path);
}

std::filesystem::path mosaic::resource_directory() {
    NSURL* url = NSBundle.mainBundle.resourceURL;
    if (!url)
        throw std::runtime_error(
            "Failed to locate application bundle resources");
    return path_from_url(url);
}

std::filesystem::path mosaic::config_directory() {
    return application_directory(NSApplicationSupportDirectory, "config");
}

std::filesystem::path mosaic::data_directory() {
    return application_directory(NSApplicationSupportDirectory, "data");
}

std::filesystem::path mosaic::cache_directory() {
    return application_directory(NSCachesDirectory);
}
