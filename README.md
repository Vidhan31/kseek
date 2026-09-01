# kseek

A KDE Plasma 6 KRunner plugin for fuzzy file search written in C++ and Qt 6. It feeds paths from [fd](https://github.com/sharkdp/fd) into [fzf](https://github.com/junegunn/fzf) over D-Bus, giving you fast filename matching in KRunner without a background indexer.

## Dependencies

Runtime requirements:
- `fzf`
- `fd` (named `fd-find` on Ubuntu and Debian)

On Debian and Ubuntu:
```bash
sudo apt install fd-find fzf
```
`kseek` checks for both `fd` and `fdfind` binaries automatically.

On Arch Linux:
```bash
sudo pacman -S fd fzf
```

On Fedora:
```bash
sudo dnf install fd-find fzf
```

## Installation

Download the package for your distribution from GitHub releases:

**Debian / Ubuntu:**
```bash
sudo apt install ./kseek_*_amd64.deb
```

**Fedora:**
```bash
sudo dnf install ./kseek-*.rpm
```

**Arch Linux:**
```bash
sudo pacman -U ./kseek-*.pkg.tar.zst
```

### Enable in Plasma settings

1. Open **System Settings** and go to **Search** > **Plasma Search**.
2. Make sure **kseek** is checked.
3. If KRunner does not show it immediately, restart KRunner with `kquitapp6 krunner` or log out and back in.

## Usage

Open KRunner (`Alt+Space`) and type `f` followed by your query:

```text
f report
f kseek
f docs 2024
```

### Result actions

- **Default (Enter).** Opens the file or folder in its default application.
- **Show in folder.** Opens Dolphin and selects the file.
- **Copy path.** Copies the full path to the clipboard.
- **Open terminal here.** Opens your default terminal in the target directory.
- **Drag and drop.** Drag results directly out of KRunner into other windows.

## Search customization

`kseek` hands off filesystem traversal to `fd` and filtering to `fzf`:

- Ignore rules: see the [fd documentation](https://github.com/sharkdp/fd) for ignore files (`~/.config/fd/ignore`, `.gitignore`) and flags.
- Search syntax: see the [fzf syntax guide](https://github.com/junegunn/fzf#search-syntax) for exact matching, prefix/suffix matching, and negation.
- Extra arguments: pass CLI flags through `KSEEK_FD_ARGS` and `KSEEK_FZF_ARGS`.

## Configuration

Set environment variables in `~/.config/systemd/user/plasma-runner-kseek.service`, `~/.config/environment.d/`, or your shell:

| Variable | Default | Description |
| :--- | :--- | :--- |
| `KSEEK_ROOT` | `$HOME` | Root directory to search. |
| `KSEEK_MAX_RESULTS` | `20` | Maximum results returned. |
| `KSEEK_TIMEOUT` | `2.5` | Query timeout in seconds. |
| `KSEEK_DEBOUNCE` | `75` | Search debounce in milliseconds (set to `0` to disable). |
| `KSEEK_FD_ARGS` | `""` | Extra arguments for `fd` (e.g. `"--hidden --follow"`). |
| `KSEEK_FZF_ARGS` | `""` | Extra arguments for `fzf` (e.g. `"--exact"`). |
| `KSEEK_FD_BIN` | auto-detected | Path to the `fd` binary. |
| `KSEEK_FZF_BIN` | auto-detected | Path to the `fzf` binary. |
| `TERMINAL` | auto-detected | Terminal emulator for the directory action. |
| `KSEEK_DEBUG` | `0` | Set to `1` for verbose log output. |

Reload systemd after editing the unit:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

## Performance

kseek was originally written in Python with PyGObject. Rewriting it in C++ using Qt 6 Core and D-Bus cut latency and memory usage:

Measured on an AMD Ryzen 5 3600 running Fedora 44, Linux 7.1, and KDE Plasma 6.7 (Wayland):

| Metric | C++ (Qt 6) | Python baseline | Difference |
| :--- | :--- | :--- | :--- |
| Startup latency (D-Bus ready) | 8.6 ms | 91.7 ms | ~10.7x faster |
| Private RAM (`RssAnon`) | 1.4 MB | 16.0 MB | ~11.8x lower |
| Proportional memory (`PSS`) | 1.9 MB | 17.2 MB | ~9.3x lower |
| Total RSS (`VmRSS`) | 15.7 MB | 33.3 MB | ~2.1x lower |

Plasma already shares 14.2 MB of Qt 6 and system libraries in memory.

## Development and testing

### Build from source

Build dependencies:
- C++20 compiler (`g++` >= 13 or `clang++` >= 16)
- `cmake` (>= 3.25)
- `qt6-base-dev` (Qt 6.4+)

Clone, build, and install:

```bash
git clone https://github.com/vidhan31/kseek.git
cd kseek
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build
```

### Build packages with Docker

Build standalone packages for each distribution. Outputs are written to `dist/out/`.

#### .deb (Ubuntu 24.04 / Debian)

```bash
docker build -f dist/Dockerfile.deb --target export --output type=local,dest=./dist/out .
```

#### .rpm (Fedora 44)

```bash
docker build -f dist/Dockerfile.rpm --target export --output type=local,dest=./dist/out .
```

#### .pkg.tar.zst (Arch Linux)

```bash
docker build -f dist/Dockerfile.pkg --target export --output type=local,dest=./dist/out .
```

### Run unit tests

Build and run tests with CTest:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### D-Bus query check

Test the runner service over D-Bus with `busctl`:

```bash
busctl --user call org.kde.krunner.kseek /kseek org.kde.krunner1 Match s "f kseek"
```

## Uninstall

Package manager removal:

```bash
# Debian / Ubuntu
sudo apt remove kseek

# Fedora
sudo dnf remove kseek

# Arch Linux
sudo pacman -R kseek
```

