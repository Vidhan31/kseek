# kseek: fuzzy file search for KDE Plasma 6 KRunner

https://github.com/user-attachments/assets/c71b2907-5e37-449d-8100-d00c71dba510

`kseek` is a fast, lightweight file search runner for KDE Plasma 6 KRunner. It brings index-free fuzzy file finding to your desktop by connecting [fd](https://github.com/sharkdp/fd) directly to [fzf](https://github.com/junegunn/fzf).

## Quick start

1. Install the package for your distribution (`.deb`, `.rpm`, or `.pkg.tar.zst`) from [GitHub Releases](https://github.com/vidhan31/kseek/releases). If you are unsure or need dependency instructions, see the [Installation](#installation) section.
2. Open **System Settings** > **Search** > **Plasma Search** and enable **kseek**.
3. Open KRunner (`Alt+Space`) and type `f` followed by your search term:

```text
f resume pdf
```

## Features

- **Instant on-demand search.** Searches execute only when you query KRunner. There are no background file watchers, no periodic crawlers, and no indexing processes writing to disk.
- **Full fzf fuzzy matching.** Supports substring searches, typo tolerance, exact phrases (`'`), prefix anchors (`^`), suffix anchors (`$`), and negative filters (`!`).
- **Low overhead and safe execution.** Written in C++20 with Qt 6, starting in under 9 ms over D-Bus with ~1.4 MB RAM. Keystrokes cancel obsolete searches immediately, and paths pass through NUL byte delimiters (`\0`) to handle spaces and special characters safely.
- **Multiple search roots.** Search across multiple directories, project folders, secondary drives, or network mounts simultaneously.
- **Desktop actions.** Open files directly, reveal them in Dolphin, copy absolute paths, spawn your terminal emulator in the directory, or drag items into other applications.

## Usage

Open KRunner (`Alt+Space`) and prefix your search with the plugin trigger (default: `f`).

You can separate the trigger with a space, a colon, or a tab:

```text
f resume pdf
f:docker-compose
f taxes 2025
f config.json
```

> [!NOTE]
> **Path visibility:** KRunner displays the containing folder path when you hover over or highlight a match. For long filenames, KRunner may collapse the path to keep the name readable. The full absolute path is always accessible through the **Copy path** action.

### Search syntax

`kseek` does not implement a custom search language. Filesystem traversal and filtering are handled by `fd`, while query matching and ranking are handled by `fzf`. All standard search syntax from both tools applies directly.

You can customize behavior by passing arguments through `KSEEK_FD_ARGS` and `KSEEK_FZF_ARGS`. Refer to the official [fd documentation](https://github.com/sharkdp/fd#readme) and [fzf documentation](https://github.com/junegunn/fzf#search-syntax) for detailed search syntax, matching patterns, and configuration options.

### Available actions

Highlight a match and press `Tab` or click the action button to access:

- **Open (Enter).** Opens the file or directory in its default application.
- **Show in folder.** Opens Dolphin and selects the target file.
- **Copy path.** Copies the absolute file path to the clipboard.
- **Open terminal here.** Spawns your terminal emulator in the containing directory. Terminal selection checks `$TERMINAL`, followed by common installed emulators.
- **Drag and drop.** Drag results from KRunner directly into Dolphin, web browsers, or text editors.

## Configuration

Configure `kseek` with environment variables or command-line flags.

System package installations create an empty configuration file at:

```text
/usr/lib/environment.d/kseek.conf
```

For user configuration, create or edit:

```text
~/.config/environment.d/kseek.conf
```

Example configuration:

```bash
KSEEK_PREFIX="f"
KSEEK_ROOT="$HOME/Projects:$HOME/Documents"
KSEEK_MAX_RESULTS="30"
KSEEK_FD_ARGS='--hidden --exclude "node_modules"'
KSEEK_FZF_ARGS='--exact'
```

After editing `kseek.conf`, reload your user environment and restart the runner service:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

### Configuration recipes

Common setups for `~/.config/environment.d/kseek.conf`:

#### Prefixless search

Search files directly in KRunner without typing `f`:

```bash
KSEEK_PREFIX=""
```

You can also set `KSEEK_PREFIX="none"` or use another prefix like `find` or `?`.

#### Search multiple drives and folders

Search across multiple folders or external drives using colon-separated paths:

```bash
KSEEK_ROOT="$HOME/Projects:$HOME/Documents:/mnt/data"
```

Setting custom roots replaces the default `$HOME` search root. Include `$HOME` explicitly if you want it included alongside other paths.

#### Search hidden files and follow symlinks

Pass flags to `fd` through `KSEEK_FD_ARGS`:

```bash
KSEEK_FD_ARGS="--hidden --follow"
```

#### Exclude specific directories

Keep search fast by skipping large build or cache folders:

```bash
KSEEK_FD_ARGS='--exclude "node_modules" --exclude "target" --exclude ".git" --exclude ".cache"'
```

> [!TIP]
> Instead of setting `KSEEK_FD_ARGS`, you can use `fd` ignore files such as `.ignore` or `.fdignore` in your project or home directory. `fd` respects these automatically.

#### Exact matching by default

Turn off fuzzy matching and require exact substring matches:

```bash
KSEEK_FZF_ARGS="--exact"
```

#### Select preferred terminal emulator

Override the default terminal used by the **Open terminal here** action:

```bash
TERMINAL="ghostty"
```

Supported options include `alacritty`, `kitty`, `foot`, `konsole`, `wezterm`, and any standard terminal that accepts `--working-directory` or `-e`.

### Environment variables

| Variable | Default | Description |
| :--- | :--- | :--- |
| `KSEEK_PREFIX` | `f` | Trigger prefix for queries in KRunner. Set to `""` or `none` for prefixless queries. |
| `KSEEK_ROOT` | `$HOME` | Search root directories, separated by colons. Replaces `$HOME`. |
| `KSEEK_MAX_RESULTS` | `20` | Maximum results returned to KRunner. Applied after pipeline matching. |
| `KSEEK_TIMEOUT` | `2.5` | Maximum search execution time in seconds. |
| `KSEEK_DEBOUNCE` | `75` | Delay before launching search after query changes, in milliseconds. Set to `0` to disable. |
| `KSEEK_FD_ARGS` | `""` | Extra arguments passed to `fd`. |
| `KSEEK_FZF_ARGS` | `""` | Extra arguments passed to `fzf`. |
| `KSEEK_FD_BIN` | auto-detected | Path to the `fd` or `fdfind` executable. |
| `KSEEK_FZF_BIN` | auto-detected | Path to the `fzf` executable. |
| `TERMINAL` | auto-detected | Terminal emulator binary for the **Open terminal here** action. |
| `KSEEK_DEBUG` | `0` | Set to `1` to enable verbose debug logging to stderr. |

## Performance

Rewriting the runner daemon from Python to C++ with Qt 6 reduced D-Bus startup latency from 91.7 ms to 8.6 ms (~10.7x faster) and lowered private RAM usage from 16.0 MB to 1.4 MB (~11.8x lower).

## Installation

Prebuilt packages are available on [GitHub Releases](https://github.com/vidhan31/kseek/releases).

### Debian and Ubuntu (`.deb`)

```bash
sudo apt install ./kseek_*_amd64.deb
```

### Fedora (`.rpm`)

```bash
sudo dnf install ./kseek-*.rpm
```

### Arch Linux (`.pkg.tar.zst`)

```bash
sudo pacman -U ./kseek-*.pkg.tar.zst
```

### Dependencies

`kseek` requires `fd` and `fzf`. The native packages declare these as dependencies and install them automatically.

To install dependencies manually:

- Debian/Ubuntu: `sudo apt install fd-find fzf`
- Arch Linux: `sudo pacman -S fd fzf`
- Fedora: `sudo dnf install fd-find fzf`

### Custom binary installations

If you installed `fd` or `fzf` outside your system package manager (for example, via Cargo), set their paths in `~/.config/environment.d/kseek.conf`:

```bash
KSEEK_FD_BIN="$HOME/.cargo/bin/fd"
KSEEK_FZF_BIN="$HOME/.local/bin/fzf"
```

To install packages without pulling distribution package dependencies:

- Debian and Ubuntu: `sudo dpkg -i --ignore-depends=fd-find,fzf ./kseek_*_amd64.deb`
- Fedora: `sudo rpm -ivh --nodeps ./kseek-*.rpm`
- Arch Linux: `sudo pacman -Ud --nodeps ./kseek-*.pkg.tar.zst`

### Enable in Plasma settings

1. Open **System Settings** > **Search** > **Plasma Search**.
2. Enable **kseek**.
3. Increase plugin priority if you want `kseek` results to appear above other search providers.
4. Restart KRunner with `kquitapp6 krunner` or log out and back in.

## Development and testing

### Command-line options

KRunner starts `kseek` automatically via D-Bus activation or systemd. You can also run `kseek` manually in a terminal or custom script. Command-line flags take precedence over environment variables.

When testing in a terminal while a service is running, use `--replace` to take over the D-Bus registration:

```bash
kseek --replace --debug --prefix "find" --root "$HOME/Projects"
```

Options:

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

### Local installation script

Build and install `kseek` into `~/.local` without `sudo`:

```bash
# Build and install locally, running test suite first
./scripts/install-dev.sh --test
```

To remove development files:

```bash
./scripts/uninstall-dev.sh
```

### Run tests

The test suite runs unit and integration tests against all edge-case fixtures:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

### Test over D-Bus

Call the runner service directly using `busctl`:

```bash
busctl --user call org.kde.krunner.kseek /kseek org.kde.krunner1 Match s "f resume"
```

### Build packages with Docker

Package builds output to `dist/`:

```bash
# Debian / Ubuntu (.deb)
docker build -f packaging/Dockerfile.deb --target export --output type=local,dest=./dist .

# Fedora (.rpm)
docker build -f packaging/Dockerfile.rpm --target export --output type=local,dest=./dist .

# Arch Linux (.pkg.tar.zst)
docker build -f packaging/Dockerfile.pkg --target export --output type=local,dest=./dist .
```

## Uninstall

### Debian and Ubuntu

```bash
sudo apt remove kseek
```

### Fedora

```bash
sudo dnf remove kseek
```

### Arch Linux

```bash
sudo pacman -R kseek
```