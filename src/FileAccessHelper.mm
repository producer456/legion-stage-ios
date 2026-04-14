// FileAccessHelper — handles iOS security-scoped file access
// Cannot include JuceHeader.h alongside Foundation due to namespace conflicts.
// Uses C strings as bridge between JUCE and ObjC.

#include <TargetConditionals.h>
#include <string>

#if TARGET_OS_IOS
#import <Foundation/Foundation.h>

// C-linkage helpers called from the header's inline implementation
extern "C" {

const char* fileAccess_readFile(const char* path)
{
    NSString* nsPath = [NSString stringWithUTF8String: path];
    NSURL* url = [NSURL fileURLWithPath: nsPath];

    // Start security-scoped access
    BOOL accessed = [url startAccessingSecurityScopedResource];

    NSError* error = nil;
    NSString* content = [NSString stringWithContentsOfURL: url
                                                 encoding: NSUTF8StringEncoding
                                                    error: &error];

    if (content == nil)
    {
        // Try NSData approach
        NSData* data = [NSData dataWithContentsOfURL: url options: 0 error: &error];
        if (data != nil)
            content = [[NSString alloc] initWithData: data encoding: NSUTF8StringEncoding];
    }

    if (accessed)
        [url stopAccessingSecurityScopedResource];

    if (content == nil)
        return nullptr;

    // Caller must free this
    const char* utf8 = [content UTF8String];
    char* result = strdup(utf8);
    return result;
}

bool fileAccess_writeFile(const char* path, const char* content)
{
    NSString* nsPath = [NSString stringWithUTF8String: path];
    NSURL* url = [NSURL fileURLWithPath: nsPath];

    BOOL accessed = [url startAccessingSecurityScopedResource];

    NSString* nsContent = [NSString stringWithUTF8String: content];
    NSError* error = nil;
    BOOL ok = [nsContent writeToURL: url atomically: YES encoding: NSUTF8StringEncoding error: &error];

    if (accessed)
        [url stopAccessingSecurityScopedResource];

    return ok == YES;
}

} // extern "C"

#else

extern "C" {
const char* fileAccess_readFile(const char*) { return nullptr; }
bool fileAccess_writeFile(const char*, const char*) { return false; }
}

#endif
