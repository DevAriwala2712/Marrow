#include "sysscope/providers/disk_provider.hpp"
#include "sysscope/util.hpp"

#import <CoreFoundation/CoreFoundation.h>
#import <IOKit/IOKitLib.h>

namespace sysscope {

namespace {

std::uint64_t cf_number(CFDictionaryRef dict, CFStringRef key) {
    CFNumberRef num = static_cast<CFNumberRef>(CFDictionaryGetValue(dict, key));
    if (!num) return 0;
    long long v = 0;
    CFNumberGetValue(num, kCFNumberLongLongType, &v);
    return static_cast<std::uint64_t>(v);
}

}  // namespace

void DiskProvider::tick(MetricsSnapshot& snapshot) {
    std::uint64_t read_bytes = 0;
    std::uint64_t write_bytes = 0;

    io_iterator_t iter = IO_OBJECT_NULL;
    kern_return_t kr = IOServiceGetMatchingServices(
        kIOMainPortDefault, IOServiceMatching("IOBlockStorageDriver"), &iter);
    if (kr != KERN_SUCCESS) return;

    for (io_object_t service = IOIteratorNext(iter); service;
         service = IOIteratorNext(iter)) {
        CFDictionaryRef stats = static_cast<CFDictionaryRef>(IORegistryEntryCreateCFProperty(
            service, CFSTR("Statistics"), kCFAllocatorDefault, 0));
        if (stats) {
            read_bytes += cf_number(stats, CFSTR("Bytes (Read)"));
            write_bytes += cf_number(stats, CFSTR("Bytes (Write)"));
            CFRelease(stats);
        }
        IOObjectRelease(service);
    }
    IOObjectRelease(iter);

    const double now = now_seconds();
    if (has_prev_ && now > prev_at_) {
        const double dt = now - prev_at_;
        snapshot.disk.read_bytes_per_sec = static_cast<double>(read_bytes - prev_read_) / dt;
        snapshot.disk.write_bytes_per_sec = static_cast<double>(write_bytes - prev_write_) / dt;
    }
    prev_read_ = read_bytes;
    prev_write_ = write_bytes;
    prev_at_ = now;
    has_prev_ = true;
    snapshot.has_disk = true;
}

}  // namespace sysscope
