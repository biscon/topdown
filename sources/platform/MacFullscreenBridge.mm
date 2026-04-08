#if defined(__APPLE__)

#include "platform/MacFullscreenBridge.h"

#include <raylib.h>
#import <Cocoa/Cocoa.h>
#import <dispatch/dispatch.h>
#import <objc/runtime.h>

static NSWindow* GetMacWindow()
{
    void* handle = GetWindowHandle();
    if (handle == nullptr) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: GetWindowHandle returned null");
        return nil;
    }

    id cocoaObject = (__bridge id)handle;
    if (cocoaObject == nil) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: bridged Cocoa object was nil");
        return nil;
    }

    if ([cocoaObject isKindOfClass:[NSWindow class]]) {
        return (NSWindow*)cocoaObject;
    }

    TraceLog(LOG_WARNING,
             "MacFullscreenBridge: window handle is not NSWindow, class=%s",
             class_getName([cocoaObject class]));

    return nil;
}

bool IsMacNativeFullscreenSupported()
{
    return GetMacWindow() != nil;
}

bool IsMacNativeFullscreenActive()
{
    NSWindow* window = GetMacWindow();
    if (window == nil) {
        return false;
    }

    return (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
}

void ToggleMacNativeFullscreen()
{
    NSWindow* window = GetMacWindow();
    if (window == nil) {
        TraceLog(LOG_WARNING, "MacFullscreenBridge: no NSWindow available for fullscreen toggle");
        return;
    }

    const bool active = (window.styleMask & NSWindowStyleMaskFullScreen) != 0;
    TraceLog(LOG_INFO,
             "MacFullscreenBridge: toggling native fullscreen (active=%s)",
             active ? "true" : "false");

    dispatch_async(dispatch_get_main_queue(), ^{
        NSWindowStyleMask style = [window styleMask];
        style |= NSWindowStyleMaskTitled;
        style |= NSWindowStyleMaskClosable;
        style |= NSWindowStyleMaskMiniaturizable;
        style |= NSWindowStyleMaskResizable;
        [window setStyleMask:style];

        NSWindowCollectionBehavior behavior = [window collectionBehavior];
        behavior |= NSWindowCollectionBehaviorFullScreenPrimary;
        [window setCollectionBehavior:behavior];

        [NSApp activateIgnoringOtherApps:YES];
        [window makeKeyAndOrderFront:nil];

        TraceLog(LOG_INFO,
                 "MacFullscreenBridge: styleMask=0x%llx collectionBehavior=0x%llx key=%s main=%s visible=%s",
                 (unsigned long long)[window styleMask],
                 (unsigned long long)[window collectionBehavior],
                 [window isKeyWindow] ? "true" : "false",
                 [window isMainWindow] ? "true" : "false",
                 [window isVisible] ? "true" : "false");

        [window toggleFullScreen:nil];
    });
}

#endif