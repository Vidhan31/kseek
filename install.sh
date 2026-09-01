#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)"
cd "$SCRIPT_DIR"

restart_krunner() {
    if systemctl --user is-active --quiet plasma-krunner.service 2>/dev/null; then
        systemctl --user restart plasma-krunner.service && return 0
    fi
    for q in qdbus6 qdbus-qt6 qdbus; do
        if command -v "$q" &> /dev/null && "$q" org.kde.krunner /krunner org.kde.krunner.Quit &> /dev/null; then
            return 0
        fi
    done
    if command -v kquitapp6 &> /dev/null && kquitapp6 krunner &> /dev/null; then
        return 0
    fi
    echo "WARN: could not reload KRunner automatically. Press Alt+Space or log out/in to pick up the change." >&2
    return 1
}

echo "==> Checking dependencies for kseek..."

FD_BIN="$(command -v fd || command -v fdfind || true)"
if [[ -z "$FD_BIN" ]]; then
    echo "ERROR: 'fd' (or 'fdfind') is not installed." >&2
    exit 1
fi

FZF_BIN="$(command -v fzf || true)"
if [[ -z "$FZF_BIN" ]]; then
    echo "ERROR: 'fzf' is not installed." >&2
    exit 1
fi

if ! "$FD_BIN" --max-results 1 . "$HOME" &> /dev/null; then
    echo "ERROR: your version of fd does not support --max-results (needs fd >= 8.3). Please upgrade fd." >&2
    exit 1
fi

if ! command -v cmake &> /dev/null; then
    echo "ERROR: 'cmake' is required to build kseek." >&2
    exit 1
fi

for f in CMakeLists.txt plasma-runner-kseek.desktop org.kde.krunner.kseek.service.in plasma-runner-kseek.service.in; do
    if [[ ! -f "$f" ]]; then
        echo "ERROR: $f not found next to this script. Run install.sh from inside the extracted project directory." >&2
        exit 1
    fi
done

echo "==> Building kseek (C++20 / Qt 6)..."
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target kseek

XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
INSTALL_DIR="$XDG_DATA_HOME/kseek"
KRUNNER_PLUGIN_DIR="$XDG_DATA_HOME/krunner/dbusplugins"
DBUS_SERVICE_DIR="$XDG_DATA_HOME/dbus-1/services"
SYSTEMD_USER_DIR="$XDG_CONFIG_HOME/systemd/user"

echo "==> Installing kseek for Plasma 6..."

mkdir -p "$INSTALL_DIR" "$KRUNNER_PLUGIN_DIR" "$DBUS_SERVICE_DIR" "$SYSTEMD_USER_DIR"

install -m 0755 build/kseek "$INSTALL_DIR/kseek"
install -m 0644 plasma-runner-kseek.desktop "$KRUNNER_PLUGIN_DIR/plasma-runner-kseek.desktop"
sed "s|@KSEEK_EXECUTABLE@|$INSTALL_DIR/kseek|g" org.kde.krunner.kseek.service.in > "$DBUS_SERVICE_DIR/org.kde.krunner.kseek.service"
sed "s|@KSEEK_EXECUTABLE@|$INSTALL_DIR/kseek|g" plasma-runner-kseek.service.in > "$SYSTEMD_USER_DIR/plasma-runner-kseek.service"

if command -v systemctl &> /dev/null; then
    systemctl --user daemon-reload || true
fi

echo "==> Installation complete."
echo "    1. Ensure kseek is enabled in System Settings > Search > Plasma Search."
echo "    2. Configure file exclusions in ~/.config/fd/ignore if you want to skip directories like node_modules."
echo "    3. Open KRunner (Alt+Space) and search using 'f <filename>'."
