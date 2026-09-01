# kseek

Fast fuzzy file search for KDE Plasma 6.

Find files and folders instantly from KRunner using [fd](https://github.com/sharkdp/fd) and [fzf](https://github.com/junegunn/fzf). No background indexing services, no bloated database.

## Dependencies

`kseek` requires `fd` and `fzf` installed on your system.

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

Download the package for your distribution from [GitHub Releases](https://github.com/vidhan31/kseek/releases):

**Debian and Ubuntu (`.deb`):**
```bash
sudo apt install ./kseek_*_amd64.deb
```

**Fedora (`.rpm`):**
```bash
sudo dnf install ./kseek-*.rpm
```

**Arch Linux (`.pkg.tar.zst`):**
```bash
sudo pacman -U ./kseek-*.pkg.tar.zst
```

### Enable in Plasma settings

1. Open **System Settings** > **Search** > **Plasma Search**.
2. Enable **kseek**.
3. If KRunner does not show it immediately, restart KRunner with `kquitapp6 krunner` or log out and back in.

## Usage

Open KRunner (`Alt+Space`) and prefix your search query with `f `:

```text
f resume pdf
f docker-compose
f taxes 2025
f config.json
```

### Available actions

- **Enter**: Open the file or folder in its default application.
- **Show in folder**: Open Dolphin and select the file.
- **Copy path**: Copy the full path to your clipboard.
- **Open terminal here**: Open your default terminal in the target directory.
- **Drag and drop**: Drag the result directly out of KRunner into other apps.

## Search customization

`kseek` uses `fd` for disk traversal and `fzf` for fuzzy filtering:

- **Ignore files**: `fd` respects `~/.config/fd/ignore` and `.gitignore` rules automatically.
- **Fzf syntax**: Use `fzf` patterns like `'exact` or `.pdf$` to narrow matches.
- **Extra arguments**: Pass custom CLI flags with `KSEEK_FD_ARGS` and `KSEEK_FZF_ARGS`.

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

Reload the user service after making changes:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

## Performance

Rewriting `kseek` in C++ with Qt 6 cut memory usage and eliminated startup lag compared to the original Python version:

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

Package builds write artifacts to `dist/out/`:

```bash
# Debian / Ubuntu (.deb)
docker build -f dist/Dockerfile.deb --target export --output type=local,dest=./dist/out .

# Fedora (.rpm)
docker build -f dist/Dockerfile.rpm --target export --output type=local,dest=./dist/out .

# Arch Linux (.pkg.tar.zst)
docker build -f dist/Dockerfile.pkg --target export --output type=local,dest=./dist/out .
```

### Run tests

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Test over D-Bus

Test the runner service directly using `busctl`:

```bash
busctl --user call org.kde.krunner.kseek /kseek org.kde.krunner1 Match s "f resume"
```

## Uninstall

```bash
# Debian / Ubuntu
sudo apt remove kseek

# Fedora
sudo dnf remove kseek

# Arch Linux
sudo pacman -R kseek
```
