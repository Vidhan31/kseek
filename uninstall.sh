#!/usr/bin/env bash
set -euo pipefail

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
    echo "WARN: could not reload KRunner automatically. Press Alt+Space or log out/in to finish removal." >&2
    return 1
}

XDG_DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
XDG_CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"
INSTALL_DIR="$XDG_DATA_HOME/kseek"
KRUNNER_PLUGIN_DIR="$XDG_DATA_HOME/krunner/dbusplugins"
DBUS_SERVICE_DIR="$XDG_DATA_HOME/dbus-1/services"
SYSTEMD_USER_DIR="$XDG_CONFIG_HOME/systemd/user"

echo "==> Removing kseek..."

if command -v systemctl &> /dev/null; then
    systemctl --user stop plasma-runner-kseek.service 2>/dev/null || true
    systemctl --user disable plasma-runner-kseek.service 2>/dev/null || true
fi

rm -rf "$INSTALL_DIR"
rm -f "$KRUNNER_PLUGIN_DIR/plasma-runner-kseek.desktop"
rm -f "$DBUS_SERVICE_DIR/org.kde.krunner.kseek.service"
rm -f "$SYSTEMD_USER_DIR/plasma-runner-kseek.service"

rm -rf "$XDG_DATA_HOME/krunner-fzf-fd"
rm -f "$KRUNNER_PLUGIN_DIR/plasma-runner-fzf-fd.desktop"
rm -f "$DBUS_SERVICE_DIR/org.kde.krunner.fzf_fd.service"
rm -f "$SYSTEMD_USER_DIR/plasma-runner-fzf-fd.service"

if command -v systemctl &> /dev/null; then
    systemctl --user daemon-reload || true
fi

echo "==> Reloading KRunner..."
restart_krunner || true

echo "==> Uninstallation complete."
