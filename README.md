# Penta Fan Controller - C Implementation

High-performance fan control daemon for Penta SATA HAT top board, rewritten in C for production use with advanced thermal management.

## ✨ Key Improvements Over Python Version

- **13x Lower Memory Usage**: ~2-3 MB vs ~30-40 MB
- **5x Faster**: <1% CPU usage vs 2-5%
- **Advanced Thermal Control**: PID-like algorithm with hysteresis, dead-band zone, and trend analysis
- **Zero Python Dependencies**: Pure C with direct libgpiod integration
- **Instant Startup**: 0.5s vs 2-3s
- **Production Ready**: SystemD integration, proper logging, FHS-compliant packaging

## 🚀 Quick Start

### Installation from Pre-built DEB Package

```bash
# Download the latest .deb package from releases
sudo dpkg -i radxa-penta-fan-ctrl_*.deb

# Enable and start the service
sudo systemctl enable --now radxa-penta-fan-ctrl
```

### Building from Source

```bash
# Install dependencies
sudo apt install build-essential cmake libgpiod-dev g++ smartmontools debhelper git

# Fetch third-party dependency (ssd1306) as a pinned submodule
git submodule update --init --recursive

# Build
mkdir -p build && cd build
cmake .. && make

# Test the binary
sudo ./radxa-penta-fan-ctrl

# Install locally (optional)
sudo make install
```

### Building a DEB Package

```bash
# From the repository root, ensure submodules are present first
git submodule update --init --recursive
dpkg-buildpackage -us -uc -b

# Install the generated package
sudo dpkg -i ../radxa-penta-fan-ctrl_*.deb
```

## 📋 Features

- ✅ Smart fan control with 3°C hysteresis & dead-band zone
- ✅ OLED display (4 pages: system, resources, disks, RAID)
- ✅ Button navigation (GPIO)
- ✅ SystemD integration with journal logging
- ✅ RPi 5 optimized (55°C/62°C/70°C/78°C thresholds)
- ✅ Smooth duty cycle ramping from 0-100% based on thermal algorithm
- ✅ SSD temperature monitoring

## 📁 Compliance with PR #5 (Debian Best Practices)

This implementation follows the Debian packaging best practices from [radxa/penta-fan-ctrl#5](https://github.com/radxa/penta-fan-ctrl/pull/5):

### FHS-Compliant File Organization
- ✅ **Configuration**: `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.conf` and `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.env`
- ✅ **Binary**: `/usr/bin/radxa-penta-fan-ctrl`
- ✅ **SystemD service**: `/lib/systemd/system/radxa-penta-fan-ctrl.service`

### Debian Package Standards
- ✅ Proper `debian/` structure with changelog, copyright, compat
- ✅ Idempotent postinst/prerm scripts
- ✅ `debhelper-compat 13`
- ✅ Multi-board detection (RPi 4/5, ROCK series)
- ✅ Automatic dependency resolution (`libgpiod2`, `smartmontools`)

### Build System
- ✅ CMake-based build (modern, cross-platform)
- ✅ Proper library dependencies (ssd1306 as submodule)
- ✅ Clean separation of source and build artifacts

Optional (developer convenience): You can fetch `ssd1306` automatically at configure time by enabling a CMake option:

```bash
cmake -S . -B build -DRADXA_PENTA_USE_FETCHCONTENT=ON
cmake --build build
```
This uses CMake's FetchContent with the same pinned commit. Note: distro packaging typically disallows network during builds, so the submodule approach is the default.

## 🎯 Usage

### Service Control
```bash
# Start the service
sudo systemctl start radxa-penta-fan-ctrl

# Stop the service
sudo systemctl stop radxa-penta-fan-ctrl

# Restart the service
sudo systemctl restart radxa-penta-fan-ctrl

# Check status
systemctl status radxa-penta-fan-ctrl

# Enable auto-start on boot
sudo systemctl enable radxa-penta-fan-ctrl
```

### Viewing Logs
```bash
# Follow live logs
journalctl -u radxa-penta-fan-ctrl -f

# View recent logs
journalctl -u radxa-penta-fan-ctrl --since "10 minutes ago"

# View all logs
journalctl -u radxa-penta-fan-ctrl
```

## ⚙️ Configuration

### Fan Temperature Thresholds

Edit `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.conf`:

```ini
[fan]
# CPU temperature thresholds (Celsius)
# Optimized for Raspberry Pi 5 (throttles at 85°C)
lv0 = 55  # First activation
lv1 = 62  # Moderate load
lv2 = 70  # High load
lv3 = 78  # Maximum

[fan_ssd]
# SSD temperature thresholds (Celsius)
# Most SSDs are safe up to 70°C, but cooler is better
lv0 = 45  # First activation
lv1 = 50  # Moderate
lv2 = 55  # High
lv3 = 60  # Maximum
```

**Notes:**
- The daemon uses **3°C hysteresis** to prevent fan oscillation
- A **±1.5°C dead-band zone** ignores minor fluctuations
- Fan speed changes are **rate-limited** to 10%/second for smooth operation
- Duty cycle can ramp smoothly from 0-100% based on thermal control algorithm

### Thermal Tunables (advanced)

You can fine-tune ramp behavior and cooling hold in the `[thermal]` section of the config or via environment variables:

- `hysteresis` (`RADXA_HYSTERESIS_C`): Cooling hysteresis in °C. Default: `3.0`.
- `deadband` (`RADXA_DEADBAND_C`): Ignore small fluctuations ±°C. Default: `1.5`.
- `trend_heat` (`RADXA_TREND_HEAT_C`): Trend considered heating. Default: `0.3`.
- `trend_fast_heat` (`RADXA_TREND_FAST_HEAT_C`): Trend for fast heating. Default: `1.0`.

**Asymmetric and adaptive ramping:**
- `up_rate_base` (`RADXA_UP_RATE_BASE`): Base ramp-up per cycle (fraction). Default: `0.07` (7%).
- `up_rate_trend_gain` (`RADXA_UP_RATE_TREND_GAIN`): Extra ramp per +1°C positive trend. Default: `0.20` (20%, responsive to rapid heating).
- `up_rate_max` (`RADXA_UP_RATE_MAX`): Cap on ramp-up per cycle. Default: `0.30` (30%).
- `down_rate` (`RADXA_DOWN_RATE`): Ramp-down per cycle (gentle). Default: `0.05` (5%).
- `cooldown_hold_sec` (`RADXA_COOLDOWN_HOLD_SEC`): Prevent decreases for this many seconds after any increase. Default: `20`.

This setup makes small temperature bumps ramp gently, while rapid heating ramps the fan quickly to catch up. When temperatures start falling, the controller holds the fan speed for a short time and then decreases gradually—helping heat soak dissipate and avoiding premature spin-down.

### GPIO and PWM Configuration

Edit `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.env` (only if using non-standard GPIOs or hardware PWM):

```bash
FAN_CHIP=0        # GPIO chip number (software PWM / tach)
FAN_LINE=27       # GPIO line for fan PWM
BUTTON_CHIP=0     # GPIO chip number for button
BUTTON_LINE=17    # GPIO line for button

# Hardware PWM (optional, requires hardware mod on RPi5 – see below)
HARDWARE_PWM=0    # Set to 1 to enable hardware PWM
PWMCHIP=0         # PWM chip number (e.g., 0 → /sys/class/pwm/pwmchip0)
PWMCHAN=0         # PWM channel (e.g., 1 → pwm1 on pwmchip0, i.e., GPIO13)
```

**Default GPIOs** (Raspberry Pi 5, software PWM):
- Fan PWM: GPIO 27
- Button: GPIO 17
- I2C OLED: I2C bus 1 (address 0x3C)

**Raspberry Pi 5 with hardware PWM mod** (`HARDWARE_PWM=1, PWMCHIP=0, PWMCHAN=1`):
- Fan PWM: GPIO 13 (PWM0_CHAN1) via hardware PWM at 25 kHz
- Eliminates the software PWM thread — zero CPU overhead

After editing configuration, restart the service:
```bash
sudo systemctl restart radxa-penta-fan-ctrl
```

---

## 🔧 Raspberry Pi 5 Hardware PWM Mod

The Penta SATA HAT top board routes the fan header through a 35Ω resistor on the bottom layer that interferes with hardware PWM on GPIO 13. With a small hardware modification you can enable true 25 kHz hardware PWM, which eliminates the software PWM thread entirely.

### What to do

1. **Resistor relocation**: On the bottom layer of the top board, locate the 35Ω resistor immediately below the top hat connector. Move it to the free pad to its right.
2. **Device tree overlay**: Add the following line to `/boot/firmware/config.txt` and reboot:
   ```
   dtoverlay=pwm-2chan,pin2=13,func2=4
   ```
3. **Use a 4-pin PWM fan**: A 3-pin fan will not work with hardware PWM. Recommended: **Noctua NF-A4x10 5V PWM**.
4. **Do NOT connect the tach line** of the original fan header — leave it disconnected.

### Configuration after the mod

In `/etc/radxa-penta-fan-ctrl/radxa-penta-fan-ctrl.env`:
```bash
HARDWARE_PWM=1
PWMCHIP=0
PWMCHAN=1
```

Verify the PWM sysfs node appeared after reboot:
```bash
ls /sys/class/pwm/pwmchip0/
```

---

## 📦 Project Structure

```
radxa-penta-sata-hat-top-board-ctrl-c/
├── src/              Source code
│   ├── main.c        Entry point, main loop
│   ├── config.c      Configuration file parser
│   ├── fan.c         Fan control with PWM
│   ├── thermal.c     Temperature monitoring & algorithm
│   ├── oled.c        OLED display management
│   └── button.c      Button navigation
├── include/          Header files
├── lib/ssd1306/      OLED library (git submodule)
├── debian/           Debian packaging files (PR#5 compliant)
│   ├── changelog     Version history
│   ├── control       Package metadata & dependencies
│   ├── copyright     License information
│   ├── postinst      Post-installation script
│   ├── prerm         Pre-removal script
│   └── rules         Build rules (debhelper)
├── build/            Build artifacts (generated)
├── CMakeLists.txt    CMake build configuration
├── radxa-penta-fan-ctrl.conf    Default configuration file
├── radxa-penta-fan-ctrl.env     Environment variables
└── radxa-penta-fan-ctrl.service SystemD service file
```

## 🔬 Advanced Thermal Algorithm

- **Moving average**: 10-sample filter for stable readings
- **Hysteresis**: 3°C prevents rapid on/off cycling
- **Dead-band**: ±1.5°C ignores minor temperature fluctuations
- **Rate limiting**: Maximum 10%/second fan speed change
- **Trend analysis**: Predicts heating/cooling patterns

## 💻 Compatibility

- ✅ **Raspberry Pi 5** (tested, fully working)
- ✅ **Raspberry Pi 4** (should work, untested)
- 🔄 **ROCK Pi 4/5** (should work, untested)
- 🔄 **ROCK 3 series** (should work, untested)

## 🛠️ Dependencies

### Runtime

| Package | Version | Purpose |
|---|---|---|
| `libgpiod2` | ≥ 1.6 | GPIO access (fan PWM, button) |
| `smartmontools` | any | SSD temperature reading via `smartctl` |

These are pulled in automatically when installing the `.deb` package.

To install them manually:
```bash
sudo apt install libgpiod2 smartmontools
```

### Build-time

```bash
sudo apt install build-essential cmake libgpiod-dev g++ git debhelper
```

| Package | Purpose |
|---|---|
| `build-essential` / `g++` | C/C++ compiler toolchain |
| `cmake` | Build system |
| `libgpiod-dev` | GPIO headers |
| `git` | Fetching the `ssd1306` submodule |
| `debhelper` | Debian packaging helpers |

## 📝 License

MIT License - see LICENSE file

## 🙏 Credits

- **Original Python version**: [Radxa rockpi-penta](https://github.com/radxa/rockpi-penta/)
- **OLED Library**: [lexus2k/ssd1306](https://github.com/lexus2k/ssd1306)

## 🔗 Links

- [Explanatory issue why I created this repo](https://github.com/radxa/rockpi-penta/issues/18#issuecomment-3445260735)
- [Penta SATA HAT Documentation](https://docs.radxa.com/en/accessories/penta-sata-hat)
- [PR #5 Reference](https://github.com/radxa/rockpi-penta/pull/5)
