#include "sysscope/anomaly.hpp"
#include "sysscope/metrics_codec.hpp"
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

static void test_thermal_codec_roundtrip() {
    sysscope::MetricsSnapshot snap;
    snap.timestamp = sysscope::now_seconds();
    snap.has_thermal = true;
    snap.thermal.clusters = {
        {"P-Core", 46.5, 72.0, 4.3},
        {"E-Core", 22.0, 61.5, 1.2},
    };
    snap.thermal.gpu_utilization_percent = 34.0;
    snap.thermal.ane_utilization_percent = 9.5;
    snap.thermal.cpu_die_temp_celsius = 78.0;
    snap.thermal.gpu_die_temp_celsius = 66.0;
    snap.thermal.fan_rpm = 3250;
    snap.thermal.die_temperature_grid = {{41.0, 42.5, 44.0}, {43.0, 46.5, 49.0}};

    sysscope::XpcResponse resp;
    resp.kind = sysscope::XpcResponseKind::Snapshot;
    resp.snapshot = snap;

    const auto encoded = sysscope::encode_response(resp);
    const auto decoded = sysscope::decode_response(encoded);
    EXPECT(decoded.has_value(), "thermal roundtrip decodes");
    EXPECT(decoded->kind == sysscope::XpcResponseKind::Snapshot, "thermal roundtrip kind");
    EXPECT(decoded->snapshot.has_thermal, "thermal flag preserved");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid.size() == 2, "grid row count preserved");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid[0].size() == 3, "grid col count preserved");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid[1][2] == 49.0, "grid cell preserved");
    EXPECT(decoded->snapshot.thermal.fan_rpm == 3250, "fan preserved");
}

static void test_thermal_codec_partial_grid() {
    sysscope::MetricsSnapshot snap;
    snap.has_thermal = true;
    snap.thermal.die_temperature_grid = {{}, {39.0, 40.0}, {}};

    sysscope::XpcResponse resp;
    resp.kind = sysscope::XpcResponseKind::Snapshot;
    resp.snapshot = snap;

    const auto decoded = sysscope::decode_response(sysscope::encode_response(resp));
    EXPECT(decoded.has_value(), "partial grid decodes");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid.size() == 3, "partial grid row count preserved");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid[0].empty(), "empty first row preserved");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid[1].size() == 2, "second row preserved");
    EXPECT(decoded->snapshot.thermal.die_temperature_grid[2].empty(), "empty third row preserved");
}

int main() {
    test_anomaly();
    test_ring_buffer();
    test_thermal_codec_roundtrip();
    test_thermal_codec_partial_grid();
    if (failures == 0) std::printf("All tests passed.\n");
    else std::printf("%d test(s) failed.\n", failures);
    return failures == 0 ? 0 : 1;
}
