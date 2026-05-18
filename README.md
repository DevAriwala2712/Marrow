<img width="250" height="250" alt="screen" src="https://github.com/user-attachments/assets/82c54dbf-468b-4c93-9abd-c7fc4dbc088a" />

# Marrow 

macOS system monitor scaffold — **C++17 + Dear ImGui + GLFW + OpenGL**. No Swift.

## Benefits over `btop`

While `btop` is a fantastic terminal-based system monitor, Marrow provides several distinct advantages by leveraging native GUI technologies and a daemon-based architecture:

1. **Hardware-Accelerated GUI**: Marrow uses OpenGL and Dear ImGui for buttery-smooth 60fps rendering. `btop` is confined to the terminal (ncurses/TTY), which limits graphical fidelity and UI responsiveness.
2. **Rich Visualizations**: Because it isn't restricted by character cells, Marrow can render complex charts, scatter plots, and fine-grained timelines.
3. **Dedicated Daemon Architecture**: Marrow separates the UI from a privileged helper daemon. This allows it to securely collect deep system metrics (via IOKit, Endpoint Security, SMJobBless) while the GUI runs completely unprivileged. With `btop`, you often have to run the entire application as root to get advanced metrics.
4. **Historical DVR (Ring Buffer)**: Marrow includes an SQLite-backed ring buffer storing up to 30 minutes (200MB) of metrics. You can seamlessly scrub back in time to view historical anomalies.
5. **Anomaly Detection (Z-Score)**: Built-in statistical analysis highlights unexpected spikes automatically.
6. **Extensible CLI**: Includes a dedicated CLI (`marrow-cli`) that outputs JSON or Prometheus formats for seamless integration with external monitoring systems.

## Build

Requires: `g++`, GLFW, SQLite3, OpenGL (Homebrew: `brew install glfw sqlite`).

```bash
cd SysScope
make          # sysscope-app, sysscope-helper, sysscope-cli
make test
```

Run the GUI:

```bash
./sysscope-app
```

CLI:

```bash
./sysscope-cli ping
./sysscope-cli --format prometheus
```

Helper daemon (stub loop):

```bash
./sysscope-helper
```

## Layout

| Path | Purpose |
|------|---------|
| `include/sysscope/` | Types, `IMetricProvider`, stubs, anomaly, IPC, ring buffer |
| `src/app/` | ImGui main window + sidebar sections |
| `src/helper/` | Privileged helper scaffold |
| `src/cli/` | `sysscope` JSON/Prometheus CLI |
| `src/storage/` | SQLite ring buffer (200 MB / 30 min) |
| `tests/` | Native test runner (`make test`) |

Uses vendored **ImGui** from `../imgui` (same as `mac_stats_gui`).

## Architecture

- **GUI** (`sysscope-app`): unprivileged; stub metric providers by default.
- **Helper** (`sysscope-helper`): placeholder for SMJobBless / IOKit / Endpoint Security.
- **CLI** (`sysscope-cli`): shares stub `HelperClient` until real IPC is wired.
- **Ring buffer**: `IRingBufferStore` + SQLite implementation.

Real collectors (powermetrics, FSEvents, Network Extension, etc.) plug in by implementing `IMetricProvider` in the helper.

## Targets (original spec → C++)

| Feature | Status |
|---------|--------|
| Process / Network / Thermal / Disk / Timeline / Settings UI | Stub panels in ImGui |
| Menubar sparklines | Inline CPU sparkline in app title bar |
| MetricProvider + stubs | `include/sysscope/stub_providers.hpp` |
| Typed IPC | `include/sysscope/ipc.hpp` (in-process stub) |
| Ring buffer DVR | SQLite store + Timeline scrub UI |
| Anomaly Z-score | `include/sysscope/anomaly.hpp` + tests |
| Metal 60fps charts | Use ImGui plots now; Metal can replace later |
