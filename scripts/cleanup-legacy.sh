#!/usr/bin/env bash
set -euo pipefail

# ANSI color formatting
BOLD="\033[1m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"
RESET="\033[0m"

log_info() {
    echo -e "${BLUE}${BOLD}==>${RESET} ${BOLD}$*${RESET}"
}

log_success() {
    echo -e "${GREEN}${BOLD}✓${RESET} $*"
}

log_warn() {
    echo -e "${YELLOW}${BOLD}WARNING:${RESET} $*"
}

log_info "Stopping any running legacy kseek Python services and processes..."

# Stop and disable legacy systemd user services
if command -v systemctl >/dev/null 2>&1; then
    systemctl --user stop plasma-runner-kseek.service 2>/dev/null || true
    systemctl --user disable plasma-runner-kseek.service 2>/dev/null || true
    systemctl --user stop plasma-runner-fzf-fd.service 2>/dev/null || true
    systemctl --user disable plasma-runner-fzf-fd.service 2>/dev/null || true
fi

# Kill any running python kseek processes
pkill -f "kseek.py" 2>/dev/null || true

# Standard XDG paths
DATA_HOME="${XDG_DATA_HOME:-$HOME/.local/share}"
CONFIG_HOME="${XDG_CONFIG_HOME:-$HOME/.config}"

LEGACY_FILES=(
    "${CONFIG_HOME}/systemd/user/plasma-runner-kseek.service"
    "${DATA_HOME}/systemd/user/plasma-runner-kseek.service"
    "${CONFIG_HOME}/systemd/user/plasma-runner-fzf-fd.service"
    "${DATA_HOME}/systemd/user/plasma-runner-fzf-fd.service"
    "${DATA_HOME}/krunner/dbusplugins/plasma-runner-fzf-fd.desktop"
    "${DATA_HOME}/dbus-1/services/org.kde.krunner.fzf_fd.service"
    "${DATA_HOME}/krunner/dbusplugins/plasma-runner-kseek.desktop"
    "${DATA_HOME}/dbus-1/services/org.kde.krunner.kseek.service"
)

LEGACY_DIRS=(
    "${DATA_HOME}/kseek"
    "${DATA_HOME}/krunner-fzf-fd"
)

log_info "Removing legacy files and directories..."

REMOVED_COUNT=0

for file in "${LEGACY_FILES[@]}"; do
    if [[ -f "$file" || -L "$file" ]]; then
        rm -f "$file"
        echo "  - Removed file: $file"
        REMOVED_COUNT=$((REMOVED_COUNT + 1))
    fi
done

for dir in "${LEGACY_DIRS[@]}"; do
    if [[ -d "$dir" ]]; then
        rm -rf "$dir"
        echo "  - Removed directory: $dir"
        REMOVED_COUNT=$((REMOVED_COUNT + 1))
    fi
done

# Reload systemd user daemon so it clears stale unit definitions
if command -v systemctl >/dev/null 2>&1; then
    systemctl --user daemon-reload 2>/dev/null || true
    systemctl --user reset-failed 2>/dev/null || true
fi

# Refresh Sycoca cache and KRunner if available
if command -v kbuildsycoca6 >/dev/null 2>&1; then
    kbuildsycoca6 2>/dev/null || true
fi

if command -v kquitapp6 >/dev/null 2>&1; then
    kquitapp6 krunner 2>/dev/null || true
fi

echo ""
if [[ "$REMOVED_COUNT" -gt 0 ]]; then
    log_success "Cleaned up ${REMOVED_COUNT} legacy artifact(s). System is ready for a fresh start!"
else
    log_success "No legacy Python or systemd artifacts found. System is already clean."
fi
echo ""
