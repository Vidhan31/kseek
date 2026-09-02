# kseek: fuzzy file search for KRunner

`kseek` is a C++ search plugin for KDE Plasma 6 KRunner. It lets you find files and folders on your computer by typing partial names, extensions, or fuzzy terms directly into KRunner (`Alt+Space`).

Instead of running a background indexing daemon, `kseek` searches on demand using [fd](https://github.com/sharkdp/fd) and [fzf](https://github.com/junegunn/fzf). Results appear as you type, ready to open, locate in Dolphin, copy, or drag into other applications.

## Dependencies

`kseek` requires `fd` and `fzf`. Native packages declare these as dependencies and install them automatically.

If you build from source:

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

### Custom binary installations

Native packages declare `fd` and `fzf` as hard dependencies. Package managers check their own databases, not your `$PATH`. If you installed `fd` or `fzf` outside the package manager (via Cargo, Homebrew, or manual downloads), the package manager will still install the distribution versions alongside your existing binaries.

`kseek` searches `$PATH` and user paths (`~/.local/bin`, `~/.cargo/bin`, `~/.fzf/bin`) at runtime. To point `kseek` to a specific binary instead of the system package, set the path in `~/.config/environment.d/kseek.conf`:

```bash
KSEEK_FD_BIN="$HOME/.cargo/bin/fd"
KSEEK_FZF_BIN="$HOME/.local/bin/fzf"
```

To install the native package without pulling distribution dependencies:

- Debian and Ubuntu: `sudo dpkg -i --ignore-depends=fd-find,fzf ./kseek_*_amd64.deb`
- Fedora: `sudo rpm -ivh --nodeps ./kseek-*.rpm`
- Arch Linux: `sudo pacman -Ud --nodeps ./kseek-*.pkg.tar.zst`

### Enable in Plasma settings

1. Open **System Settings** > **Search** > **Plasma Search**.
2. Enable **kseek**.
3. (Optional) Increase the plugin priority if you want `kseek` results to appear above other search providers.
4. Restart KRunner with `kquitapp6 krunner` or log out and back in.

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

- **Enter.** Open the file or directory in its default application.
- **Show in folder.** Open Dolphin and select the file.
- **Copy path.** Copy the absolute path to your clipboard.
- **Open terminal here.** Open your terminal in the selected directory.
- **Drag and drop.** Drag results from KRunner into other applications.

## Search customization

`kseek` delegates file traversal to `fd` and match filtering to `fzf`:

- **Ignore rules.** `fd` respects `~/.config/fd/ignore` and `.gitignore` files automatically.
- **Fzf syntax.** Use standard `fzf` patterns like `'exact` or `.pdf$` to filter results.
- **Multi-root search.** Search across multiple directories simultaneously by passing multiple `--root` arguments or colon-separated paths in `KSEEK_ROOT`.
- **Extra arguments.** Pass additional flags through `KSEEK_FD_ARGS` and `KSEEK_FZF_ARGS`. Quoting with single quotes, double quotes, and backslash escapes is supported.

## Configuration

Configure `kseek` using environment variables or command-line arguments.

Package installations place an empty `kseek.conf` file in `/usr/lib/environment.d/kseek.conf`. Local development installations create an empty user config at `~/.config/environment.d/kseek.conf`.

To customize settings for normal desktop use with KRunner or systemd, add your environment variables to `~/.config/environment.d/kseek.conf`:

```bash
KSEEK_PREFIX="f"
KSEEK_ROOT="/home/user/Projects:/home/user/Documents"
KSEEK_MAX_RESULTS="30"
```

Reload the user environment and restart the service after modifying `kseek.conf`:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

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

In typical usage, KRunner launches `kseek` automatically via D-Bus activation or the systemd user service.

You can also run `kseek` directly from the terminal or in custom scripts. Command-line options override any environment variables.

When testing or running `kseek` from the terminal while a background instance is already active, pass `--replace` so the process takes over the D-Bus service registration:

```bash
kseek --replace --debug --prefix "find" --root "$HOME/Projects"
```

Available options:

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
  --fd-args <args>           Extra arguments passed to fd (e.g. "--hidden --follow").
  --fzf-args <args>          Extra arguments passed to fzf (e.g. "--exact").
  --fd-bin <path>            Path to fd / fdfind executable.
  --fzf-bin <path>           Path to fzf executable.
  --replace                  Replace an already running kseek instance on D-Bus.
  --debug                    Enable verbose debug logging.
```

## Performance

`kseek` was originally written in Python. Rewriting the runner daemon in C++ with Qt 6 eliminated interpreter startup overhead and significantly cut memory footprint.

The benchmark compares the current C++ (Qt 6) binary directly against the previous Python implementation under identical search queries. Measured on an AMD Ryzen 5 3600 running Fedora 44 and KDE Plasma 6.7:

| Metric | C++ (Qt 6) | Python (previous) | Difference |
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

The test suite runs unit and integration tests against edge-case fixtures covering Unicode, dotfiles, symlinks, compound extensions, and case sensitivity:

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
