#!/usr/bin/env bash
set -euo pipefail

# Determine repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Default options
PREFIX="${PREFIX:-$HOME/.local}"
BUILD_TYPE="Debug"
BUILD_DIR="${REPO_ROOT}/build"
CLEAN_BUILD=0
RUN_TESTS=0
RESTART_SERVICES=1

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

Build and install kseek locally into a user prefix (default: ~/.local) for development and testing.

Options:
  -p, --prefix <DIR>       Install prefix (default: $HOME/.local)
  -b, --build-type <TYPE>  CMake build type: Debug, Release, RelWithDebInfo (default: Debug)
  -B, --build-dir <DIR>    Build directory (default: <repo-root>/build)
  -c, --clean              Clean build directory before configuring
  -t, --test               Run test suite (ctest) before installing
  --no-restart             Do not restart KRunner or reload systemd user services
  -h, --help               Display this help message

Examples:
  ./scripts/install-dev.sh
  ./scripts/install-dev.sh --test --build-type Release
  ./scripts/install-dev.sh --clean --prefix "$HOME/.local"
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
        -b|--build-type)
            BUILD_TYPE="$2"
            shift 2
            ;;
        -B|--build-dir)
            BUILD_DIR="$2"
            shift 2
            ;;
        -c|--clean)
            CLEAN_BUILD=1
            shift
            ;;
        -t|--test)
            RUN_TESTS=1
            shift
            ;;
        --no-restart)
            RESTART_SERVICES=0
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

# 1. Dependency checks
log_info "Checking build and runtime dependencies..."

check_cmd() {
    if ! command -v "$1" >/dev/null 2>&1; then
        log_error "Missing required command: $1 ($2)"
        return 1
    fi
}

check_cmd cmake "Please install CMake (>= 3.25)" || exit 1

# Check for C++ compiler
if ! command -v g++ >/dev/null 2>&1 && ! command -v clang++ >/dev/null 2>&1; then
    log_error "No C++ compiler found. Please install g++ (>= 13) or clang++ (>= 16)."
    exit 1
fi

# Runtime dependencies checks
HAS_FD=0
if command -v fd >/dev/null 2>&1 || command -v fdfind >/dev/null 2>&1; then
    HAS_FD=1
else
    log_warn "Neither 'fd' nor 'fdfind' was found in PATH. kseek requires fd at runtime."
fi

HAS_FZF=0
if command -v fzf >/dev/null 2>&1; then
    HAS_FZF=1
else
    log_warn "'fzf' was not found in PATH. kseek requires fzf at runtime."
fi

# 2. Build configuration
if [[ "$CLEAN_BUILD" -eq 1 && -d "$BUILD_DIR" ]]; then
    log_info "Cleaning build directory: $BUILD_DIR"
    rm -rf "$BUILD_DIR"
fi

log_info "Configuring kseek (${BUILD_TYPE}) with prefix: ${PREFIX}"
cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DBUILD_TESTING=ON

# 3. Compile
log_info "Building kseek..."
cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 2)"

# 4. Run tests if requested
if [[ "$RUN_TESTS" -eq 1 ]]; then
    log_info "Running test suite..."
    ctest --test-dir "$BUILD_DIR" --output-on-failure
    log_success "All tests passed!"
fi

# 5. Install to prefix
log_info "Installing kseek to ${PREFIX}..."
cmake --install "$BUILD_DIR" --prefix "$PREFIX"

# 6. Service reload and session refresh
if [[ "$RESTART_SERVICES" -eq 1 ]]; then
    log_info "Refreshing systemd user daemon and Plasma runner cache..."

    # Terminate any existing running instance
    pkill -x kseek 2>/dev/null || true

    # Reload systemd user daemon if available
    if command -v systemctl >/dev/null 2>&1; then
        systemctl --user daemon-reload 2>/dev/null || true
    fi

    # Update KDE Sycoca service cache so KRunner picks up new desktop entry
    if command -v kbuildsycoca6 >/dev/null 2>&1; then
        kbuildsycoca6 2>/dev/null || true
    fi

    # Restart KRunner if running
    if command -v kquitapp6 >/dev/null 2>&1; then
        kquitapp6 krunner 2>/dev/null || true
    fi
fi

# 7. Verification & Next steps
echo ""
log_success "Local installation completed successfully!"
echo ""
echo -e "${BOLD}Installed components:${RESET}"
echo "  - Binary:          ${PREFIX}/bin/kseek"
echo "  - Runner Desktop:  ${PREFIX}/share/krunner/dbusplugins/plasma-runner-kseek.desktop"
echo "  - D-Bus Service:   ${PREFIX}/share/dbus-1/services/org.kde.krunner.kseek.service"
echo "  - Systemd Service: ${PREFIX}/share/systemd/user/plasma-runner-kseek.service"
echo ""

# Check PATH
BIN_DIR="${PREFIX}/bin"
if [[ ":$PATH:" != *":${BIN_DIR}:"* ]]; then
    log_warn "${BIN_DIR} is not in your PATH. Add it to your shell configuration (e.g. ~/.bashrc or ~/.zshrc):"
    echo "    export PATH=\"${BIN_DIR}:\$PATH\""
    echo ""
fi

echo -e "${BOLD}How to test:${RESET}"
echo "  1. Test D-Bus query via busctl:"
echo "     busctl --user call org.kde.krunner.kseek /kseek org.kde.krunner1 Match s \"f resume\""
echo ""
echo "  2. Test in KRunner:"
echo "     Press Alt+Space (or Meta) and type 'f <query>'"
echo ""
echo "  3. View live runner logs:"
echo "     journalctl --user -u plasma-runner-kseek.service -f"
echo ""
echo "  4. To uninstall:"
echo "     ./scripts/uninstall-dev.sh --prefix \"${PREFIX}\""
echo ""
