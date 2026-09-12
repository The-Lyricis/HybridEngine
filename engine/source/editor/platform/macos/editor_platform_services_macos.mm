#include "editor_platform_services_macos.h"

#import <Cocoa/Cocoa.h>

namespace Hybrid
{
    namespace
    {
        NSString* toNSString(const std::filesystem::path& path)
        {
            return [NSString stringWithUTF8String:path.string().c_str()];
        }

        NSString* toNSString(const std::string& value)
        {
            return [NSString stringWithUTF8String:value.c_str()];
        }

        std::filesystem::path toPath(NSURL* url)
        {
            return url ? std::filesystem::path([[url path] fileSystemRepresentation]) : std::filesystem::path{};
        }

        NSArray<NSString*>* fileTypes(const std::vector<FileDialogFilter>& filters)
        {
            NSMutableArray<NSString*>* result = [NSMutableArray array];
            for (const auto& filter : filters)
            {
                std::string pattern = filter.pattern;
                std::size_t start = 0;
                while (start < pattern.size())
                {
                    const std::size_t end = pattern.find(';', start);
                    std::string item = pattern.substr(start, end == std::string::npos ? end : end - start);
                    const std::size_t dot = item.rfind("*.");
                    if (dot != std::string::npos && dot + 2 < item.size())
                        [result addObject:toNSString(item.substr(dot + 2))];
                    start = end == std::string::npos ? pattern.size() : end + 1;
                }
            }
            return result;
        }

        void configurePanel(NSSavePanel* panel, const std::string& title,
                            const std::filesystem::path& initial_dir,
                            const std::vector<FileDialogFilter>& filters)
        {
            if (!title.empty())
                [panel setTitle:toNSString(title)];
            if (!initial_dir.empty())
                [panel setDirectoryURL:[NSURL fileURLWithPath:toNSString(initial_dir) isDirectory:YES]];
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            NSArray<NSString*>* types = fileTypes(filters);
            if ([types count] > 0)
                [panel setAllowedFileTypes:types];
#pragma clang diagnostic pop
        }
    }

    std::filesystem::path EditorPlatformServicesMacOS::getEditorUserDataDir() const
    {
        @autoreleasepool
        {
            NSArray<NSURL*>* urls = [[NSFileManager defaultManager] URLsForDirectory:NSApplicationSupportDirectory
                                                                            inDomains:NSUserDomainMask];
            if ([urls count] > 0)
                return toPath([urls firstObject]) / "HybridEngine" / "Editor";
            return std::filesystem::temp_directory_path() / "HybridEngine" / "Editor";
        }
    }

    std::filesystem::path EditorPlatformServicesMacOS::getCurrentExecutablePath() const
    {
        @autoreleasepool
        {
            NSString* executable = [[NSBundle mainBundle] executablePath];
            if (!executable)
                executable = [[NSProcessInfo processInfo] arguments][0];
            return std::filesystem::path([executable fileSystemRepresentation]);
        }
    }

    bool EditorPlatformServicesMacOS::launchEditorProcess(const std::filesystem::path& executable,
                                                           const std::vector<std::string>& args) const
    {
        if (executable.empty()) return false;
        @autoreleasepool
        {
            NSTask* task = [[NSTask alloc] init];
            [task setExecutableURL:[NSURL fileURLWithPath:toNSString(executable)]];
            NSMutableArray<NSString*>* arguments = [NSMutableArray arrayWithCapacity:args.size()];
            for (const auto& arg : args) [arguments addObject:toNSString(arg)];
            [task setArguments:arguments];
            NSError* error = nil;
            const bool launched = [task launchAndReturnError:&error];
            [task release];
            return launched;
        }
    }

    void EditorPlatformServicesMacOS::configureApplicationAppearance()
    {
        @autoreleasepool
        {
            NSBundle* bundle = [NSBundle mainBundle];
            NSString* icon_file = [bundle objectForInfoDictionaryKey:@"CFBundleIconFile"];
            if (![icon_file isKindOfClass:[NSString class]] || [icon_file length] == 0)
                return;

            NSString* icon_name = [icon_file stringByDeletingPathExtension];
            NSString* icon_extension = [icon_file pathExtension];
            if ([icon_extension length] == 0)
                icon_extension = @"icns";
            NSURL* icon_url = [bundle URLForResource:icon_name withExtension:icon_extension];
            if (!icon_url)
                return;

            NSImage* icon = [[NSImage alloc] initWithContentsOfURL:icon_url];
            if (!icon)
                return;
            [[NSApplication sharedApplication] setApplicationIconImage:icon];
            [icon release];
        }
    }

    std::optional<std::filesystem::path> EditorPlatformServicesMacOS::showSaveFileDialog(
        NativeWindowHandle, const SaveFileDialogDesc& desc)
    {
        @autoreleasepool
        {
            NSSavePanel* panel = [NSSavePanel savePanel];
            configurePanel(panel, desc.title, desc.initial_dir, desc.filters);
            if (!desc.default_name.empty()) [panel setNameFieldStringValue:toNSString(desc.default_name)];
            if (!desc.default_extension.empty())
            {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
                [panel setAllowedFileTypes:@[toNSString(desc.default_extension)]];
#pragma clang diagnostic pop
            }
            return [panel runModal] == NSModalResponseOK ? std::optional<std::filesystem::path>(toPath([panel URL])) : std::nullopt;
        }
    }

    std::vector<std::filesystem::path> EditorPlatformServicesMacOS::showOpenFileDialog(
        NativeWindowHandle, const OpenFileDialogDesc& desc)
    {
        @autoreleasepool
        {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            configurePanel(panel, desc.title, desc.initial_dir, desc.filters);
            [panel setCanChooseFiles:YES]; [panel setCanChooseDirectories:NO];
            [panel setAllowsMultipleSelection:desc.allow_multi_select];
            std::vector<std::filesystem::path> result;
            if ([panel runModal] == NSModalResponseOK)
                for (NSURL* url in [panel URLs]) result.push_back(toPath(url));
            return result;
        }
    }

    std::optional<std::filesystem::path> EditorPlatformServicesMacOS::showSelectFolderDialog(
        NativeWindowHandle, const SelectFolderDialogDesc& desc)
    {
        @autoreleasepool
        {
            NSOpenPanel* panel = [NSOpenPanel openPanel];
            configurePanel(panel, desc.title, desc.initial_dir, {});
            [panel setCanChooseFiles:NO]; [panel setCanChooseDirectories:YES];
            return [panel runModal] == NSModalResponseOK ? std::optional<std::filesystem::path>(toPath([panel URL])) : std::nullopt;
        }
    }

    bool EditorPlatformServicesMacOS::revealInFileBrowser(const std::filesystem::path& path)
    {
        if (path.empty()) return false;
        @autoreleasepool
        {
            NSURL* url = [NSURL fileURLWithPath:toNSString(path)];
            if (std::filesystem::is_directory(path))
                return [[NSWorkspace sharedWorkspace] openURL:url];
            [[NSWorkspace sharedWorkspace] activateFileViewerSelectingURLs:@[url]];
            return true;
        }
    }

    std::unique_ptr<IEditorPlatformServices> CreateEditorPlatformServices()
    {
        return std::make_unique<EditorPlatformServicesMacOS>();
    }
}
