# kseek: fuzzy file search for KDE Plasma 6 KRunner

https://github.com/user-attachments/assets/c71b2907-5e37-449d-8100-d00c71dba510

`kseek` is a fast, lightweight file search runner for KDE Plasma 6 KRunner. It brings index-free fuzzy file finding to your desktop by connecting [fd](https://github.com/sharkdp/fd) directly to [fzf](https://github.com/junegunn/fzf).

## Quick start

1. **Install kseek:**
   - **KDE Store (Recommended):** Open **System Settings** > **Search** > **Plasma Search** > **Get New Plugins...**, search for **kseek**, and click **Install**.
   - **Distribution package:** Install `.deb`, `.rpm`, or `.pkg.tar.zst` from [GitHub Releases](https://github.com/vidhan31/kseek/releases).
2. Ensure external dependencies `fd` and `fzf` are installed (see [Dependencies](#dependencies)).
3. Make sure **kseek** is enabled under **System Settings** > **Search** > **Plasma Search**.
4. Open KRunner (`Alt+Space` or `Meta`) and search:

```text
f resume pdf
```

> [!TIP]
> Check for updates periodically in **System Settings** > **Search** > **Plasma Search** > **Get New Plugins...** to receive bug fixes and feature updates.

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

Customize behavior by passing arguments in `kseek.conf` (`fd_args`, `fzf_args`), through environment variables (`KSEEK_FD_ARGS`, `KSEEK_FZF_ARGS`), or via CLI options (`--fd-args`, `--fzf-args`). Refer to the official [fd documentation](https://github.com/sharkdp/fd#readme) and [fzf documentation](https://github.com/junegunn/fzf#search-syntax) for detailed search syntax and matching patterns.

> [!TIP]
> **Prevent search clutter:** Searching broad directories like `$HOME` can return excessive results if deep build trees or cache directories are included. Follow the [fd documentation on ignore files](https://github.com/sharkdp/fd#excluding-specific-files-or-directories) to exclude unwanted folders using `.ignore`, `.fdignore`, or global ignore files (`~/.config/fd/ignore`), and configure `fzf` options to refine query matching.

### Available actions

Highlight a match and press `Tab` or click the action button to access:

- **Open (Enter).** Opens the file or directory in its default application.
- **Show in folder.** Opens Dolphin and selects the target file.
- **Copy path.** Copies the absolute file path to the clipboard.
- **Open terminal here.** Spawns your terminal emulator in the containing directory. Terminal selection checks `$TERMINAL`, followed by common installed emulators.
- **Drag and drop.** Drag results from KRunner directly into Dolphin, web browsers, or text editors.

## Configuration

Configure `kseek` through a configuration file, environment variables, or command-line flags. Settings resolve in this order:

1. Built-in defaults
2. User configuration file (`~/.config/kseek/kseek.conf`)
3. Environment variables (`KSEEK_*`)
4. Command-line flags (`--prefix`, `--root`, etc.)

Create or edit your user configuration at:

```text
~/.config/kseek/kseek.conf
```

Default configuration (`~/.config/kseek/kseek.conf`):

```ini
[General]
# Trigger prefix for queries in KRunner (default: f, set to "" or none for prefixless)
# prefix = f

# Search root directories (default: $HOME)
root = ~

# Maximum results returned to KRunner (default: 20)
# max_results = 20

# Query timeout in seconds (default: 2.5)
# timeout = 2.5

# Search debounce delay in milliseconds (default: 75)
# debounce = 75

# Extra arguments passed to fd
# fd_args = --hidden

# Extra arguments passed to fzf
fzf_args = --scheme=path

# Custom binary paths (if installed outside PATH)
# fd_bin = /usr/bin/fd
# fzf_bin = /usr/bin/fzf

# Enable verbose debug logging
# debug = false
```

After editing `kseek.conf`, reload settings immediately via D-Bus:

```bash
qdbus org.kde.krunner.kseek /kseek org.kde.krunner1.Config
```

Alternatively, terminate the running instance so D-Bus restarts it on your next search:

```bash
pkill -x kseek
```

### Configuration reference

| Setting (`kseek.conf`) | CLI Option | Environment Variable | Default | Description |
| :--- | :--- | :--- | :--- | :--- |
| `prefix` | `-p, --prefix` | `KSEEK_PREFIX` | `f` | Trigger prefix for queries in KRunner. Set to `""` or `none` for prefixless queries. |
| `root` | `-r, --root` | `KSEEK_ROOT` | `~` (`$HOME`) | Search root directories, separated by colons (e.g. `~/Projects:~/Documents`). |
| `max_results` | `-m, --max-results` | `KSEEK_MAX_RESULTS` | `20` | Maximum results returned to KRunner. |
| `timeout` | `-t, --timeout` | `KSEEK_TIMEOUT` | `2.5` | Maximum search execution time in seconds. |
| `debounce` | `-d, --debounce` | `KSEEK_DEBOUNCE` | `75` | Delay in milliseconds before launching a search after query changes. |
| `fd_args` | `--fd-args` | `KSEEK_FD_ARGS` | `""` | Extra arguments passed to `fd` (e.g. `--hidden --follow`). |
| `fzf_args` | `--fzf-args` | `KSEEK_FZF_ARGS` | `--scheme=path` in `kseek.conf` (`""` if unset) | Extra arguments passed to `fzf` (e.g. `--scheme=path --exact`). |
| `fd_bin` | `--fd-bin` | `KSEEK_FD_BIN` | auto-detected | Path to the `fd` or `fdfind` executable. |
| `fzf_bin` | `--fzf-bin` | `KSEEK_FZF_BIN` | auto-detected | Path to the `fzf` executable. |
| `debug` | `--debug` | `KSEEK_DEBUG` | `false` | Enable verbose debug logging to stderr. |
| — | `-c, --config` | — | `~/.config/kseek/kseek.conf` | Path to custom configuration file. |
| — | `--replace` | — | — | Replace an already running instance on D-Bus. |
| — | — | `TERMINAL` | auto-detected | Terminal binary used by the **Open terminal here** action. |

### Custom binary paths

If you installed `fd` or `fzf` outside standard system paths, set their paths in `~/.config/kseek/kseek.conf`:

```ini
fd_bin = ~/.cargo/bin/fd
fzf_bin = ~/.local/bin/fzf
```

### Preferred terminal emulator

Override the default terminal used by the **Open terminal here** action by exporting `$TERMINAL` in your environment:

```bash
export TERMINAL="ghostty"
```

Supported options include `ghostty`, `alacritty`, `kitty`, `foot`, `konsole`, `ptyxis`, `wezterm`, and any standard terminal that accepts `--working-directory` or `-e`.

## Performance

Rewriting the runner daemon from Python to C++ with Qt 6 reduced D-Bus startup latency from 91.7 ms to 8.6 ms (~10.7x faster) and lowered private RAM usage from 16.0 MB to 1.4 MB (~11.8x lower).

## Installation

### KDE Store (Recommended)

You can install `kseek` directly from the Plasma desktop interface:

1. Open **System Settings** > **Search** > **Plasma Search**.
2. Click **Get New Plugins...** in the bottom-right corner.
3. Search for **kseek** and click **Install**.

You can also browse or download the package directly from [store.kde.org](https://store.kde.org).

> [!TIP]
> Check for updates periodically in **System Settings** > **Search** > **Plasma Search** > **Get New Plugins...** to receive bug fixes and feature updates.

### Distribution Packages

Prebuilt packages are available on [GitHub Releases](https://github.com/vidhan31/kseek/releases).

#### Debian and Ubuntu (`.deb`)

```bash
sudo apt install ./kseek_*_amd64.deb
```

#### Fedora (`.rpm`)

```bash
sudo dnf install ./kseek-*.rpm
```

#### Arch Linux (`.pkg.tar.zst`)

```bash
sudo pacman -U ./kseek-*.pkg.tar.zst
```

### Dependencies

`kseek` requires `fd` and `fzf`. Native distribution packages (`.deb`, `.rpm`, `.pkg.tar.zst`) declare these as dependencies and install them automatically.

If you install via the **KDE Store**, install the dependencies using your package manager:

- Debian/Ubuntu: `sudo apt install fd-find fzf`
- Arch Linux: `sudo pacman -S fd fzf`
- Fedora: `sudo dnf install fd-find fzf`
- openSUSE: `sudo zypper install fd fzf`

### Custom binary installations

If you installed `fd` or `fzf` outside your system package manager (for example, via Cargo), set their paths in `~/.config/kseek/kseek.conf`:

```ini
fd_bin = ~/.cargo/bin/fd
fzf_bin = ~/.local/bin/fzf
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

KRunner starts `kseek` automatically via D-Bus session activation. You can also run `kseek` manually in a terminal or custom script. Command-line flags take precedence over environment variables.

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
  -c, --config <path>        Path to configuration file (default: ~/.config/kseek/kseek.conf).
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

The script copies [`kseek.conf`](kseek.conf) to `~/.config/kseek/kseek.conf` if no user configuration exists.

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
# KDE Store / KNewStuff package (.tar.gz built on Ubuntu 24.04 glibc baseline)
docker build -f packaging/Dockerfile.kdestore --target export --output type=local,dest=./dist .

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