# EVKey - Vietnamese Keyboard Input Method Editor

Open-source reimplementation of [EVKey](https://www.nchc.org.vn/evkey/), a Vietnamese keyboard input method for Windows.

> ⚠️ **Note**: This is an open-source reimplementation based on reverse engineering of the original EVKey binary. It is provided for educational and research purposes.

## About

EVKey is a Vietnamese keyboard IME (Input Method Editor) developed by the National Center for High-performance Computing (NCHC), Vietnam. It provides:

- Vietnamese Telex and VNI input methods
- Multiple charset encodings (Unicode, TCVN3/VPS, VNI, VIQR, etc.)
- Macro support for custom shortcuts
- System tray integration
- Per-application Vietnamese mode switching

## Project Structure

```
ev/
├── .github/workflows/build.yml   # GitHub Actions CI/CD
├── src/
│   ├── evk.h                 # Main header (types, constants, declarations)
│   ├── evk_table.h           # Vietnamese character tables
│   ├── evk_main.c            # Entry point, window procedure, initialization
│   ├── evk_hook.c            # WH_KEYBOARD_LL hook callback
│   ├── evk_input.c           # Vietnamese input engine (Telex, VNI)
│   ├── evk_charset.c         # Charset conversion (TCVN3, VISCII, etc.)
│   ├── evk_settings.c        # Settings load/save (setting.ini)
│   ├── evk_tray.c            # System tray icon and menu
│   ├── evk_dialog.c          # Settings and converter dialogs
│   ├── evk_macro.c           # Macro system (evkmacro.txt)
│   ├── evk_util.c            # Utility functions
│   └── evk.rc                # Windows resource file
├── CMakeLists.txt            # CMake build configuration
├── LICENSE
└── README.md
```

## Building

### Prerequisites

- **Windows**: Visual Studio 2019/2022 with C/C++ workload
- **CMake**: 3.15+

### Using Visual Studio (MSVC)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

### Output

The built executable will be at `build/Release/EVKey64.exe`.

## Architecture

The application works by:

1. **System Tray**: Places an icon in the notification area for quick access
2. **Keyboard Hook**: Installs `WH_KEYBOARD_LL` to intercept keystrokes globally
3. **Compose Engine**: Tracks in-progress Vietnamese character composition
4. **Output**: Sends composed characters via `SendInput` or clipboard paste

### Input Methods

| Method | Description |
|--------|-------------|
| **Telex** | Uses letter keys (f/s/r/x/j) for tones, double vowels for special chars |
| **VNI** | Uses digit keys (1-5, 0) for tones, digits 6-9 for special vowels |
| **VIQR** | Uses punctuation marks for tones |

### Supported Charsets

Unicode, UTF-8, TCVN3 (ABC), TCVN5, VISCII, VPS, WinCP1258, VIQR, NCR decimal/hex, C string escapes, and more.

## Configuration

Settings are stored in `setting.ini` in the same directory as the executable.

```ini
[setting]
input_method=telex
charset=0
viet_enabled=1
auto_switch=1
always_viet=0
backspace=1
start_min=0
auto_start=0

[blacklist]
app0=crossfire.exe
app1=dota2.exe
```

## References

- [EVKey Official Page](https://www.nchc.org.vn/evkey/)
- [Ghidra](https://ghidra-sre.org/) - The reverse engineering tool used for analysis

## License

See [LICENSE](LICENSE) for details.
