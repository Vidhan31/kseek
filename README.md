# kseek

[![KDE Store](https://img.shields.io/badge/KDE%20Store-kseek-blue?logo=kde)](https://store.kde.org/p/2369956)

A KDE Plasma 6 KRunner plugin for fast fuzzy file searching written in C++ (Qt 6). Originally written in Python (PyGObject), it was rewritten in C++ to minimize startup latency and memory footprint. It feeds paths from [fd](https://github.com/sharkdp/fd) into [fzf](https://github.com/junegunn/fzf) over D-Bus, giving you filename fuzzy matching inside KRunner without running a heavy indexing daemon in the background.

If you are not already familiar with [fd](https://github.com/sharkdp/fd) and [fzf](https://github.com/junegunn/fzf), check those projects first. This plugin is designed around their search behavior and syntax.

## Installation

### Option 1: From KDE Store

You can get `kseek` directly from the [KDE Store](https://store.kde.org/p/2369956) or install it via Plasma:
1. Open **System Settings** > **Search** > **Plasma Search**.
2. Click **Get New Plugins...** and search for **kseek**.

### Option 2: From Source

Clone the repository and run the install script:

```bash
git clone https://github.com/vidhan31/kseek.git
cd kseek
./install.sh
```

The installer builds the native C++ binary and copies:
- The runner executable to `~/.local/share/kseek/`
- KRunner desktop metadata to `~/.local/share/krunner/dbusplugins/`
- D-Bus activation files to `~/.local/share/dbus-1/services/`
- A systemd user unit to `~/.config/systemd/user/`

### Enable the plugin in Plasma settings

After running `install.sh`, verify that the plugin is active:

1. Open **System Settings** and go to **Search** > **Plasma Search**.
2. Scroll to the **kseek** entry and ensure the checkbox is checked.
3. If KRunner does not immediately pick it up, restart KRunner with `kquitapp6 krunner` or log out and back in.

## Usage

Open KRunner (`Alt+Space`) and type `f` followed by a search term:

```text
f report
f kseek
f docs 2024
```

### Actions

Selecting a search match supports several actions:
- **Default (Enter)**: Opens the file or folder in its default application.
- **Show in Folder**: Opens Dolphin and highlights the file.
- **Copy File Path**: Copies the full path to the clipboard.
- **Open Terminal Here**: Opens your default terminal in the containing directory.
- **Drag and Drop**: You can drag search results straight out of KRunner into other applications.

## Search Customization

kseek directly leverages `fd` for filesystem traversal and `fzf` for fuzzy filtering without reimplementing or constraining their behavior:

- **Ignore Rules & Traversal**: Refer to the [fd documentation](https://github.com/sharkdp/fd) for details on ignore files (`~/.config/fd/ignore`, `.gitignore`), hidden files, and traversal options.
- **Search Syntax & Filtering**: Refer to the [fzf search syntax guide](https://github.com/junegunn/fzf#search-syntax) for details on exact matching, prefix/suffix matching, negation, and boolean operators.
- **Custom Arguments**: You can extend or customize either tool by passing CLI arguments via `KSEEK_FD_ARGS` and `KSEEK_FZF_ARGS`.

## Configuration

You can customize runtime behavior by setting environment variables in `~/.config/systemd/user/plasma-runner-kseek.service`, `~/.config/environment.d/`, or your shell:

| Variable | Default | Description |
| :--- | :--- | :--- |
| `KSEEK_ROOT` | `$HOME` | Root directory for file searches (supports root `/` or any folder). |
| `KSEEK_MAX_RESULTS` | `20` | Maximum number of results to display. |
| `KSEEK_TIMEOUT` | `2.5` | Search timeout in seconds before canceling a slow query. |
| `KSEEK_DEBOUNCE` | `75` | Debounce delay in milliseconds before triggering search (set to `0` to disable). |
| `KSEEK_FD_ARGS` | `""` | Extra arguments passed to `fd` (e.g. `"--hidden --follow"` or `"-a"`). |
| `KSEEK_FZF_ARGS` | `""` | Extra arguments passed to `fzf` (e.g. `"--exact"`, `"-i"`). |
| `KSEEK_FD_BIN` | auto-detected | Custom path to the `fd` executable. |
| `KSEEK_FZF_BIN` | auto-detected | Custom path to the `fzf` executable. |
| `TERMINAL` | auto-detected | Preferred terminal emulator executable for "Open Terminal Here". |
| `KSEEK_DEBUG` | `0` | Set to `1` to output verbose logs. |

After modifying the service file or environment, reload systemd:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

## Performance

kseek was originally written in Python using PyGObject. It was rewritten in C++ as a headless daemon using Qt 6 Core and Qt 6 DBus, removing Python runtime startup overhead and reducing memory usage.

Measured on an AMD Ryzen 5 3600 running Fedora 44, Linux 7.1, and KDE Plasma 6.7 (Wayland):

| Metric | C++ (Qt 6) | Python baseline | Difference |
| :--- | :--- | :--- | :--- |
| **Startup latency (D-Bus ready)** | 8.6 ms | 91.7 ms | ~10.7x faster |
| **Private RAM (`RssAnon`)** | 1.4 MB | 16.0 MB | ~11.8x lower |
| **Proportional memory (`PSS`)** | 1.9 MB | 17.2 MB | ~9.3x lower |
| **Total RSS (`VmRSS`)** | 15.7 MB | 33.3 MB | ~2.1x lower |

Most of the C++ resident memory (14.2 MB) consists of shared Qt 6 and system library pages already loaded by the Plasma desktop session.

## Testing & Debugging

### Direct D-Bus Query

You can test the running runner daemon directly from the terminal using `busctl`:

```bash
busctl --user call org.kde.krunner.kseek /kseek org.kde.krunner1 Match s "f kseek"
```

### Running Unit Tests

Build and run the automated test suite with CTest:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Uninstallation

To remove the plugin and all installed files:

```bash
./uninstall.sh
```
