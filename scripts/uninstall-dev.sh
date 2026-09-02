#!/usr/bin/env bash
set -euo pipefail

# Determine repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default options
PREFIX="${PREFIX:-$HOME/.local}"
BUILD_DIR="${REPO_ROOT}/build"
CLEAN_BUILD=0

# ANSI color formatting
BOLD="\033[1m"
GREEN="\033[0;32m"
YELLOW="\033[0;33m"
BLUE="\033[0;34m"
RED="\033[0;31m"
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

log_error() {
    echo -e "${RED}${BOLD}ERROR:${RESET} $*" >&2
}

usage() {
    cat <<EOF
Usage: $(basename "$0") [options]

Uninstall locally installed kseek development files and refresh Plasma/D-Bus state.

Options:
  -p, --prefix <DIR>       Install prefix to remove from (default: $HOME/.local)
  -B, --build-dir <DIR>    Build directory (default: <repo-root>/build)
  -c, --clean-build        Also remove the build directory
  -h, --help               Display this help message

Examples:
  ./scripts/uninstall-dev.sh
  ./scripts/uninstall-dev.sh --prefix "$HOME/.local" --clean-build
EOF
    exit 0
}

# Parse command-line arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -p|--prefix)
            PREFIX="$2"
            shift 2
            ;;
        -B|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -c|--clean-build)
            CLEAN_BUILD=1
            shift
            ;;
        -h|--help)
            usage
            ;;
        *)
            log_error "Unknown option: $1"
            usage
            ;;
    esac
done

log_info "Stopping kseek services and processes..."

# Stop and disable systemd service if running
if command -v systemctl >/dev/null 2>&1; then
    systemctl --user stop plasma-runner-kseek.service 2>/dev/null || true
    systemctl --user disable plasma-runner-kseek.service 2>/dev/null || true
fi

# Terminate any running process
pkill -x kseek 2>/dev/null || true

# Target files to delete
FILES=(
    "${PREFIX}/bin/kseek"
    "${PREFIX}/share/krunner/dbusplugins/plasma-runner-kseek.desktop"
    "${PREFIX}/share/dbus-1/services/org.kde.krunner.kseek.service"
    "${PREFIX}/share/systemd/user/plasma-runner-kseek.service"
    "${HOME}/.config/systemd/user/plasma-runner-kseek.service"
)

log_info "Removing installed kseek files from ${PREFIX}..."
REMOVED_COUNT=0
for file in "${FILES[@]}"; do
    if [[ -f "$file" || -L "$file" ]]; then
        rm -f "$file"
        echo "  - Removed: $file"
        REMOVED_COUNT=$((REMOVED_COUNT + 1))
    fi
done

# Try removing empty parent directory for krunner plugin if empty
rmdir "${PREFIX}/share/krunner/dbusplugins" 2>/dev/null || true
rmdir "${PREFIX}/share/krunner" 2>/dev/null || true

if [[ "$REMOVED_COUNT" -eq 0 ]]; then
    log_warn "No installed files were found under ${PREFIX}."
else
    log_success "Removed ${REMOVED_COUNT} file(s)."
fi

# Clean build directory if requested
if [[ "$CLEAN_BUILD" -eq 1 && -d "$BUILD_DIR" ]]; then
    log_info "Removing build directory: ${BUILD_DIR}"
    rm -rf "$BUILD_DIR"
    log_success "Build directory removed."
fi

# Reload systemd and Plasma caches
log_info "Refreshing systemd user daemon and Plasma runner cache..."

if command -v systemctl >/dev/null 2>&1; then
    systemctl --user daemon-reload 2>/dev/null || true
    systemctl --user reset-failed 2>/dev/null || true
fi

if command -v kbuildsycoca6 >/dev/null 2>&1; then
    kbuildsycoca6 2>/dev/null || true
fi

if command -v kquitapp6 >/dev/null 2>&1; then
    kquitapp6 krunner 2>/dev/null || true
fi

echo ""
log_success "Local uninstallation completed successfully!"
echo ""
