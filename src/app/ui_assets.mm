#include "ui_assets.hpp"

#include <AppKit/AppKit.h>
#include <CoreGraphics/CoreGraphics.h>

namespace marrow {

bool load_png_rgba(const char* path, std::vector<unsigned char>& out_rgba, int& out_width, int& out_height) {
    out_rgba.clear();
    out_width = 0;
    out_height = 0;
    if (path == nullptr) return false;

    @autoreleasepool {
        NSString* ns_path = [NSString stringWithUTF8String:path];
        if (ns_path == nil) return false;

        NSImage* image = [[NSImage alloc] initWithContentsOfFile:ns_path];
        if (image == nil) return false;

        NSRect proposed = NSMakeRect(0, 0, image.size.width, image.size.height);
        CGImageRef cg_image = [image CGImageForProposedRect:&proposed context:nil hints:nil];
        if (cg_image == nil) {
            [image release];
            return false;
        }

        const size_t width = CGImageGetWidth(cg_image);
        const size_t height = CGImageGetHeight(cg_image);
        if (width == 0 || height == 0) {
            [image release];
            return false;
        }

        out_rgba.resize(width * height * 4);
        CGColorSpaceRef color_space = CGColorSpaceCreateDeviceRGB();
        CGContextRef context =
            CGBitmapContextCreate(out_rgba.data(), width, height, 8, width * 4, color_space,
                                  kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        CGColorSpaceRelease(color_space);
        if (context == nullptr) {
            out_rgba.clear();
            [image release];
            return false;
        }

        CGContextDrawImage(context, CGRectMake(0, 0, static_cast<CGFloat>(width), static_cast<CGFloat>(height)),
                           cg_image);
        CGContextRelease(context);
        [image release];

        out_width = static_cast<int>(width);
        out_height = static_cast<int>(height);
        return true;
    }
}

}  // namespace marrow
