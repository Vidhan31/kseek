# kseek

A KDE Plasma 6 KRunner plugin for fast fuzzy file searching. It feeds paths from `fd` into `fzf` over D-Bus, giving you filename fuzzy matching inside KRunner without running an indexing daemon in the background.

## Requirements

You need KDE Plasma 6, Python 3, `fd` (or `fdfind`), and `fzf`.

Install the dependencies for your distribution:

### Arch Linux / Manjaro
```bash
sudo pacman -S fd fzf python-gobject
```

### Fedora / RHEL
```bash
sudo dnf install fd-find fzf python3-gobject
```

### Ubuntu / Debian / KDE Neon
```bash
sudo apt update
sudo apt install fd-find fzf python3-gi
```

### openSUSE
```bash
sudo zypper install fd fzf python3-gobject
```

## Installation

Download or clone the repo and run the install script:

```bash
git clone https://github.com/vidhan31/kseek.git
cd kseek
./install.sh
```

The installer copies:
- The runner script to `~/.local/share/kseek/`
- KRunner desktop metadata to `~/.local/share/krunner/dbusplugins/`
- D-Bus activation files to `~/.local/share/dbus-1/services/`
- A systemd user unit to `~/.config/systemd/user/`

### Enable the plugin in Plasma settings

After running `install.sh`, open System Settings and check that the plugin is active:

1. Open **System Settings** and go to **Search** > **Plasma Search**.
2. Scroll to the **kseek** entry and ensure the checkbox is checked.
3. If KRunner does not immediately pick it up, restart KRunner with `kquitapp6 krunner` or log out and back in.

## Usage

Open KRunner (`Alt+Space`) and type `f` followed by a search term:

```text
f report
f kseek.py
f docs 2024
```

### Actions

Selecting a search match supports several actions:
- **Default (Enter)**: Opens the file or folder in its default application.
- **Show in Folder**: Opens Dolphin and highlights the file.
- **Copy File Path**: Copies the full path to the clipboard.
- **Open Terminal Here**: Opens your default terminal in the containing directory.
- **Drag and Drop**: You can drag search results straight out of KRunner into other applications.

## Warning: Tool configuration

kseek does not hardcode exclusions like `node_modules` or flags like `--hidden`. You need to set your own ignore rules and search options.

- Add folder exclusions such as `node_modules` or `build` to `~/.config/fd/ignore`.
- fd skips hidden files by default. If you want hidden files or symlinks included, set `KSEEK_FD_ARGS="--hidden --follow"` in your environment or service file.
- fzf runs in filter mode (`--filter`). Terminal UI flags like colors or preview windows in your shell files do not apply here.

## Configuration

You can customize runtime behavior by setting environment variables in `~/.config/systemd/user/plasma-runner-kseek.service` or your shell environment:

| Variable | Default | Description |
| :--- | :--- | :--- |
| `KSEEK_ROOT` | `$HOME` | Root directory for file searches. |
| `KSEEK_MAX_RESULTS` | `20` | Maximum number of results to display. |
| `KSEEK_TIMEOUT` | `2.5` | Search timeout in seconds before canceling a slow query. |
| `KSEEK_FD_ARGS` | `""` | Extra arguments passed to `fd` (e.g. `"--hidden --follow"`). |
| `KSEEK_DEBUG` | `0` | Set to `1` to output verbose logs. |

After modifying the service file, reload systemd:

```bash
systemctl --user daemon-reload
systemctl --user restart plasma-runner-kseek.service
```

## Running tests

Run the test suite with Python's built-in unittest runner:

```bash
python3 -m unittest discover tests
```

## Uninstallation

To remove the plugin and all installed files:

```bash
./uninstall.sh
```
