#include "sysscope/anomaly.hpp"
#include "sysscope/ring_buffer.hpp"
#include "sysscope/types.hpp"
#include "sysscope/util.hpp"

#include <cstdio>
#include <filesystem>

static int failures = 0;

#define EXPECT(cond, msg)                          \
    do {                                           \
        if (!(cond)) {                             \
            std::fprintf(stderr, "FAIL: %s\n", msg); \
            ++failures;                            \
        }                                          \
    } while (0)

static void test_anomaly() {
    sysscope::AnomalyDetector det(0.05, 2.5);
    EXPECT(det.observe(10) == std::nullopt, "first no alert");
    for (int i = 0; i < 50; ++i) det.observe(10);
    EXPECT(det.observe(10000).has_value(), "spike alerts");
}

static void test_ring_buffer() {
    const auto path = std::filesystem::temp_directory_path() / "sysscope-cpp-test.sqlite";
    std::filesystem::remove(path);
    auto store = sysscope::make_sqlite_ring_buffer();
    EXPECT(store->open(path.string()), "open db");
    sysscope::MetricsSnapshot snap;
    snap.timestamp = sysscope::now_seconds();
    snap.has_cpu = true;
    snap.cpu.user_percent = 10;
    EXPECT(store->append(snap), "append");
    EXPECT(store->disk_usage_bytes() > 0, "has data");
    store->close();
    std::filesystem::remove(path);
}

int main() {
    test_anomaly();
    test_ring_buffer();
    if (failures == 0) std::printf("All tests passed.\n");
    else std::printf("%d test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
