# GitHub Copilot Instructions for Radxa Penta Fan Controller

## Project Overview

This workspace contains two implementations of a fan controller for the Radxa/RockPi Penta SATA HAT top board:

1. **C Implementation** (`radxa-penta-sata-hat-top-board-ctrl-c/`): Production-ready daemon with advanced thermal management, 13x lower memory usage than Python, and proper SystemD integration
2. **Python Implementation** (`piradxatopboard/`): Original prototype with basic fan control and OLED display functionality

The C implementation is the primary focus and production version.

## Project Architecture

### C Implementation Structure

- **Core Components**:
  - `src/main.c` - Main daemon with service lifecycle management
  - `src/thermal.c` - Smart thermal control with PID-like algorithm, hysteresis, dead-band zones, and trend analysis
  - `src/fan.c` - PWM fan control (hardware/software PWM via libgpiod)
  - `src/oled.c` - SSD1306 OLED display management (4 pages: system, resources, disks, RAID)
  - `src/button.c` - GPIO button handling for page navigation
  - `src/config.c` - INI-style configuration parser

- **Headers**: All in `include/` directory with corresponding `.h` files

- **Configuration**:
  - `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.conf` - Temperature thresholds and thermal tunables
  - `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.env` - Hardware pin configuration

- **Build System**: CMake-based with submodule dependency (ssd1306)

- **Packaging**: Debian package with FHS-compliant structure, SystemD service, proper postinst/prerm scripts

## Coding Standards & Conventions

### C Code Style

1. **Indentation**: 4 spaces, no tabs
2. **Naming Conventions**:
   - Snake_case for variables and functions: `thermal_read_cpu_temp()`, `fan_config_t`
   - Suffix types with `_t`: `config_t`, `fan_t`, `thermal_state_t`
   - Prefix with module name: `oled_init()`, `thermal_calculate_duty_cycle()`
3. **Headers**:
   - Include guards: `#ifndef CONFIG_H` / `#define CONFIG_H`
   - SPDX license identifier: `SPDX-License-Identifier: MIT`
   - Copyright notice: `Copyright (c) 2025 Francisco Javier Acosta Padilla`
4. **Error Handling**: Check return values, use `fprintf(stderr, ...)` for errors
5. **Memory Safety**: No dynamic allocation in hot paths, fixed-size buffers with bounds checking

### Python Code Style (Legacy)

1. Follow PEP 8
2. Use environment variables for hardware configuration via `.env` files
3. Thread-safe operations with locks for shared state

## Key Technical Details

### Thermal Control Algorithm

The C implementation uses sophisticated thermal management:

- **Moving Average Filter**: 10-sample history for noise reduction
- **Hysteresis**: 3°C cooling offset to prevent oscillation
- **Dead-Band Zone**: ±1.5°C for stability at target temperature
- **Asymmetric Ramping**:
  - Upward: Base 7%/cycle + trend-based gain (max 30%/cycle)
  - Downward: Conservative 5%/cycle
- **Cooldown Hold**: 20s hold after increases before allowing decreases
- **Trend Analysis**: Detects heating/cooling based on temperature slope

Configuration tunables in `thermal_tunables_t`:
- `hysteresis_c`, `deadband_c`, `trend_heat_c`, `trend_fast_heat_c`
- `up_rate_base_per_cycle`, `up_rate_trend_gain`, `up_rate_max_per_cycle`
- `down_rate_per_cycle`, `cooldown_hold_sec`

### Hardware Integration

- **GPIO Library**: libgpiod (modern kernel interface, no deprecated sysfs GPIO)
- **PWM**: Supports both hardware PWM (`/sys/class/pwm/`) and software PWM via GPIO
- **OLED**: I2C-based SSD1306 128x32 display via `ssd1306` library (pinned submodule)
- **Temperature Sources**:
  - CPU: `/sys/class/thermal/thermal_zone0/temp`
  - SSD: `smartctl -A /dev/sdX` with 5-second caching
- **Button**: GPIO interrupt-based (libgpiod edge detection)

### SystemD Integration

- Service type: `simple` with restart policy
- Logging: stdout → systemd-journal (unbuffered with `setbuf(stdout, NULL)`)
- Dependencies: After `multi-user.target`
- Post-install: Enables service, reloads daemon
- Pre-remove: Stops service gracefully

## Development Guidelines

### When Adding Features

1. **Configuration Changes**: Update both:
   - `include/config.h` - Add struct members
   - `src/config.c` - Add parsing logic with defaults
   - `radxa-penta-fan-ctrl.conf` - Document new options

2. **Hardware Access**: Always use libgpiod, never raw sysfs GPIO

3. **Temperature Monitoring**: Use cached reads for expensive operations (smartctl)

4. **OLED Updates**: Run display updates in separate pthread to avoid blocking main control loop

5. **Error Handling**: Log warnings but continue operation when possible (graceful degradation)

### Building & Testing

```bash
# Development build
git submodule update --init --recursive
mkdir -p build && cd build
cmake .. && make

# Test binary (requires root for GPIO/I2C access)
sudo ./radxa-penta-fan-ctrl

# Build Debian package
dpkg-buildpackage -us -uc -b
sudo dpkg -i ../radxa-penta-fan-ctrl_*.deb
```

### Release Process

Use the automated release script:
```bash
./scripts/release.sh [VERSION]
```

Updates:
- `CMakeLists.txt` version
- `src/main.c` version string
- `debian/changelog`
- Creates git tag and GitHub release with .deb artifacts

## Dependencies

### Runtime
- `libgpiod2` (≥1.6) - GPIO access
- `smartmontools` - SSD temperature reading

### Build-time
- `cmake`, `libgpiod-dev`, `g++`, `git`, `debhelper-compat (=13)`
- Submodule: `ssd1306` library (pinned to commit `1c2c71043d981b756ec39129abc57440a59e5d3e`)

## Common Tasks

### Adding OLED Page

1. Add page logic in `src/oled.c` → `oled_display_page()`
2. Update `OLED_MAX_PAGES` in `include/oled.h`
3. Handle page in auto-scroll logic

### Adjusting Thermal Behavior

1. Modify defaults in `src/config.c` → `config_load()`
2. Users can override in `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.conf`
3. Test with different workloads (stress-ng, disk I/O)

### Supporting New Hardware

1. Add pin mappings to `radxa-penta-fan-ctrl.env`
2. Update board detection in `debian/postinst`
3. Test PWM availability (fallback to software PWM if needed)

## Compatibility

- **Primary Target**: Raspberry Pi 5 (optimized thresholds: 55/62/70/78°C)
- **Also Supports**: Raspberry Pi 4, ROCK Pi 4/5, ROCK 5 series
- **Kernel**: Requires modern kernel with libgpiod support (5.10+)
- **Architecture**: Any (multi-arch Debian package)

## Important Notes

1. **Root Privileges Required**: Daemon needs root for GPIO/I2C/PWM access
2. **I2C Permissions**: Ensure user is in `i2c` group or run as root
3. **SSD Detection**: Hardcoded to `/dev/sda` through `/dev/sdd` (configurable via code)
4. **Thread Safety**: OLED and button handlers run in separate threads with proper synchronization
5. **Python Version**: Legacy/reference implementation, not for production use

## Documentation

- **README.md**: Features, installation, usage
- **RELEASING.md**: Release automation and versioning
- **RELEASE_AUTOMATION.md**: Detailed release script documentation
- **debian/changelog**: Debian package changelog

## License

MIT License - See LICENSE file
