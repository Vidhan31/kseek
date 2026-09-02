# kseek - fuzzy file search for KRunner

A KRunner plugin for KDE Plasma that finds files and folders using [fd](https://github.com/sharkdp/fd) and [fzf](https://github.com/junegunn/fzf).

## Dependencies

`kseek` requires `fd` and `fzf`.

Debian and Ubuntu:
```bash
sudo apt install fd-find fzf
```

Arch Linux:
```bash
sudo pacman -S fd fzf
```

Fedora:
```bash
sudo dnf install fd-find fzf
```

## Installation

Prebuilt packages are available on [GitHub Releases](https://github.com/vidhan31/kseek/releases).

Debian and Ubuntu (`.deb`):
```bash
sudo apt install ./kseek_*_amd64.deb
```

Fedora (`.rpm`):
```bash
sudo dnf install ./kseek-*.rpm
```

Arch Linux (`.pkg.tar.zst`):
```bash
sudo pacman -U ./kseek-*.pkg.tar.zst
```

### Enable in Plasma settings

1. Open **System Settings** > **Search** > **Plasma Search**.
2. Enable **kseek**.
3. Restart KRunner with `kquitapp6 krunner` or log out and back in.

## Usage

Open KRunner (`Alt+Space`) and prefix your search with the plugin trigger (default is `f` followed by a space or colon):

```text
f resume pdf
f:docker-compose
f taxes 2025
f config.json
```

The trigger prefix is configurable or can be disabled for prefixless queries.

### Available actions

- **Enter**: Open the item in its default application.
- **Show in folder**: Open Dolphin and select the file.
- **Copy path**: Copy the absolute path to your clipboard.
- **Open terminal here**: Open your terminal in the selected directory.
- **Drag and drop**: Drag the result out of KRunner into another application.

## Search customization

`kseek` delegates path discovery to `fd` and filtering to `fzf`:

- **Ignore files**: `fd` automatically respects `~/.config/fd/ignore` and `.gitignore` rules.
- **Fzf syntax**: Use standard `fzf` patterns like `'exact` or `.pdf$` to filter results.
- **Extra arguments**: Pass additional CLI flags using `KSEEK_FD_ARGS` and `KSEEK_FZF_ARGS`. Quoting with single quotes, double quotes, and backslash escapes is supported.

## Configuration

Configure `kseek` through environment variables or command-line flags. For systemd user sessions, define variables in `~/.config/environment.d/kseek.conf` or edit the user service unit.

### Environment variables

| Variable | Default | Description |
| :--- | :--- | :--- |
| `KSEEK_PREFIX` | `f` | Trigger prefix for queries, such as `f`, `find`, or `?`. Set to `""` or `none` for prefixless search. |
| `KSEEK_ROOT` | `$HOME` | Root directory for file searches. Supports colon-separated lists for multiple roots (e.g. `~/Projects:~/Documents` or `/dir1:/dir2`). |
| `KSEEK_MAX_RESULTS` | `20` | Maximum number of results returned. |
| `KSEEK_TIMEOUT` | `2.5` | Search timeout in seconds. |
| `KSEEK_DEBOUNCE` | `75` | Search debounce in milliseconds. Set to `0` to disable. |
| `KSEEK_FD_ARGS` | `""` | Extra arguments passed to `fd`, for example `'--exclude "My Documents" --hidden'`. |
| `KSEEK_FZF_ARGS` | `""` | Extra arguments passed to `fzf`, for example `"--exact --tiebreak=begin"`. |
| `KSEEK_FD_BIN` | auto-detected | Explicit path to the `fd` or `fdfind` executable. |
| `KSEEK_FZF_BIN` | auto-detected | Explicit path to the `fzf` executable. |
| `TERMINAL` | auto-detected | Terminal emulator binary for the directory action. |
| `KSEEK_DEBUG` | `0` | Set to `1` to enable debug logs. |

### Command-line options

`kseek` provides CLI options that override environment variables:

```text
Usage: kseek [options]

Options:
  -h, --help                 Displays help on commandline options.
  -v, --version              Displays version information.
  -p, --prefix <prefix>      Trigger prefix for queries (default: 'f', use '' or 'none' for prefixless).
  -r, --root <path>          Root directory to search (can be specified multiple times or colon-separated, default: $HOME).
  -m, --max-results <count>  Maximum results returned (default: 20).
  -t, --timeout <seconds>    Query timeout in seconds (default: 2.5).
  -d, --debounce <ms>        Search debounce in milliseconds (default: 75).
  --fd-args <args>           Extra arguments passed to fd.
  --fzf-args <args>          Extra arguments passed to fzf.
  --fd-bin <path>            Path to fd / fdfind executable.
  --fzf-bin <path>           Path to fzf executable.
  --replace                  Replace an already running kseek instance on D-Bus.
  --debug                    Enable verbose debug logging.
```

Reload the user service after editing configuration files:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

## Performance

Rewriting the runner daemon in C++ with Qt 6 reduced memory usage and startup latency compared to the Python baseline.

Measured on an AMD Ryzen 5 3600 running Fedora 44 and KDE Plasma 6.7:

| Metric | C++ (Qt 6) | Python baseline | Difference |
| :--- | :--- | :--- | :--- |
| Startup latency (D-Bus ready) | 8.6 ms | 91.7 ms | ~10.7x faster |
| Private RAM (`RssAnon`) | 1.4 MB | 16.0 MB | ~11.8x lower |
| Proportional memory (`PSS`) | 1.9 MB | 17.2 MB | ~9.3x lower |
| Total RSS (`VmRSS`) | 15.7 MB | 33.3 MB | ~2.1x lower |

## Development and testing

### Build from source

Requirements:
- C++20 compiler (`g++` >= 13 or `clang++` >= 16)
- `cmake` (>= 3.25)
- `qt6-base-dev` (Qt 6.4+)

```bash
git clone https://github.com/vidhan31/kseek.git
cd kseek
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Build packages with Docker

Package builds write release archives to `dist/`:

```bash
# Debian / Ubuntu (.deb)
docker build -f packaging/Dockerfile.deb --target export --output type=local,dest=./dist .

# Fedora (.rpm)
docker build -f packaging/Dockerfile.rpm --target export --output type=local,dest=./dist .

# Arch Linux (.pkg.tar.zst)
docker build -f packaging/Dockerfile.pkg --target export --output type=local,dest=./dist .
```

### Run tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Local installation for testing

To build and install `kseek` into your user environment (`~/.local`) without requiring `sudo`:

```bash
# Build and install locally, running tests first
./scripts/install-dev.sh --test

# Optional flags:
# ./scripts/install-dev.sh --build-type Release
# ./scripts/install-dev.sh --clean
# ./scripts/install-dev.sh --prefix "$HOME/.local"
```

The script configures CMake, builds the binary, registers the KRunner plugin desktop file and D-Bus service, reloads systemd user units, and refreshes the KDE service cache.

### Local uninstallation

To remove locally installed development files and reset Plasma/D-Bus state:

```bash
./scripts/uninstall-dev.sh

# Optional: also clean the build directory
./scripts/uninstall-dev.sh --clean-build
```

### Test over D-Bus

Call the runner service directly using `busctl`:

```bash
busctl --user call org.kde.krunner.kseek /kseek org.kde.krunner1 Match s "f resume"
```

## Uninstall

Debian and Ubuntu:
```bash
sudo apt remove kseek
```

Fedora:
```bash
sudo dnf remove kseek
```

Arch Linux:
```bash
sudo pacman -R kseek
```
