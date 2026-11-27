# Serial Terminal (Native Qt6)

A high-performance C++ implementation of the Serial Terminal with Python plugin support.

## Features

- **High-Performance**: Non-blocking multi-threaded serial capture.
- **Precision Alignment**: Fixed 13px line-height for consistent terminal rendering.
- **Python Plugins**: Extend the terminal with custom Python decoders for real-time data analysis.
- **Dynamic Rendering**: Instant re-rendering of entire history when toggling Hex/Text or changing decoders.
- **Cross-Platform**: Support for Linux, macOS, and Windows.
- **Smart Auto-scroll**: Intelligently pauses scrolling when inspecting historical data.

## Prerequisites

- **Qt 6**: (Widgets, SerialPort, Core)
- **Python 3**: Development headers required for embedding.
- **CMake**: 3.16+
- **C++17 Compiler**

### Install Dependencies (Linux)
- **Ubuntu/Debian**: `sudo apt install qt6-base-dev qt6-serialport-dev python3-dev`
- **Fedora**: `sudo dnf install qt6-qtbase-devel qt6-qtserialport-devel python3-devel`
- **Arch**: `sudo pacman -S qt6-base qt6-serialport python`

## Build Instructions

### One-Click Build
- **Linux/macOS**: `./build.sh`
- **Windows**: Double-click `build.bat`

### Manual Build
```bash
mkdir build && cd build
cmake ..
cmake --build . --config Release
```

## Python Plugin System

Place `.py` scripts in the `plugins/` directory (located in the same folder as the executable).

### Plugin Template
Each plugin must implement a `decode_serial` function:

```python
# plugins/my_decoder.py
def decode_serial(raw_bytes: bytes) -> str:
    # Your custom decoding logic here
    # Example: convert bytes to a formatted string
    return f"[DECODED] {raw_bytes.hex().upper()}"
```

Select your plugin from the **"Decoder"** dropdown in the UI to apply it to live and historical data.

Plugins may also implement `decode_stream(raw_bytes)` and `reset_stream()` to carry incomplete data across continuous receive chunks. The application resets stream state before replaying history. The `dipc_frame` plugin uses this to retain partial `0x12`-delimited frames until complete and output each frame, including its delimiters, as an uppercase hex line.

## Exporting Logs
The application supports viewport-accurate exporting. Use the **"Export to File"** button to save exactly what is visible in the terminal (respecting your current mode and display settings).
