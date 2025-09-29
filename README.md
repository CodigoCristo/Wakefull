# Wakefull

Keep your computer awake by preventing desktop idleness.

Wakefull is a simple C implementation inspired by the Caffeine project. It prevents your computer from going to sleep, activating the screensaver, or locking the screen by inhibiting desktop idleness.

## Features

- **Simple Interface**: Just `--start` and `--stop` commands
- **Lightweight**: Written in C with minimal dependencies
- **Safe**: Properly handles cleanup and signal management
- **Cross-platform**: Works on any X11-based Linux desktop environment

## Requirements

- Linux with X11 desktop environment
- X11 development libraries (`libx11-dev`)
- `xdg-screensaver` utility (usually part of `xdg-utils`)
- Meson build system
- Ninja build tool
- C compiler (GCC or Clang)

## Installation

### Quick Install

```bash
git clone <repository-url>
cd Wakefull
./install.sh
```

### Custom Installation

Install to a custom prefix:

```bash
./install.sh --prefix /usr
```

### Manual Installation

If you prefer to build manually:

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install meson ninja-build libx11-dev xdg-utils

# Or on Arch Linux
sudo pacman -S meson ninja libx11 xdg-utils

# Build and install
meson setup build
ninja -C build
sudo ninja -C build install
```

## Usage

### Start preventing desktop idleness

```bash
wakefull --start
```

This will:
- Fork a daemon process in the background
- Create a dummy X11 window
- Use `xdg-screensaver suspend` to prevent screensaver activation
- Keep running in the background until stopped

### Stop preventing desktop idleness

```bash
wakefull --stop
```

This will:
- Resume the screensaver using `xdg-screensaver resume`
- Clean up the lock file and X11 resources

### Check status

```bash
wakefull --status
```

This will show whether wakefull is currently running and display the daemon PID.

### Get help

```bash
wakefull --help
```

## How It Works

Wakefull works by:

1. Creating a minimal X11 window when started
2. Using the window ID with `xdg-screensaver suspend` to prevent idle detection
3. Storing the window ID in a lock file (`/tmp/wakefull.lock`)
4. Keeping the program running to maintain the X11 connection
5. Properly cleaning up when stopped or interrupted

## Examples

```bash
# Start wakefull before watching a movie
wakefull --start

# Check if it's running
wakefull --status

# Stop when done
wakefull --stop

# Try to start again (will show "Already running" message)
wakefull --start
```

## Signals

Wakefull properly handles these signals for clean shutdown:
- `SIGINT` (Ctrl+C)
- `SIGTERM` 
- `SIGHUP`

When receiving any of these signals, it will automatically resume the screensaver and clean up resources.

## Files

- `wakefull.c` - Main source code
- `meson.build` - Meson build configuration
- `install.sh` - Installation script
- `/tmp/wakefull.lock` - Runtime lock file (contains daemon PID and window ID)

## Differences from Caffeine

While inspired by Caffeine, Wakefull is simpler:

- **No GUI**: Command-line only interface
- **No auto-detection**: Manual start/stop only (no automatic fullscreen detection)
- **No system tray**: No indicator or tray icon
- **Daemon mode**: Runs in background, no need to keep terminal open
- **Minimal dependencies**: Only requires X11 and xdg-utils

## Troubleshooting

### "Cannot open X11 display" error
Make sure you're running this on a system with X11 and the `DISPLAY` environment variable is set.

### "xdg-screensaver not found"
Install xdg-utils package:
```bash
# Ubuntu/Debian
sudo apt install xdg-utils

# Arch Linux  
sudo pacman -S xdg-utils
```

### Program doesn't prevent sleep
Some desktop environments or power management systems may override xdg-screensaver. You may need to configure your specific desktop environment's power settings.

## License

This project is released under the GNU General Public License v3.0 or later.

## Contributing

Contributions are welcome! Please feel free to submit issues or pull requests.

## Acknowledgments

- Inspired by the [Caffeine project](https://launchpad.net/caffeine)
- Uses the same underlying mechanism (`xdg-screensaver`)