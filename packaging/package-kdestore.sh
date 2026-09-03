#!/usr/bin/env bash
set -euo pipefail

# Determine repository root
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# ANSI formatting
BOLD="\033[1m"
GREEN="\033[0;32m"
BLUE="\033[0;34m"
YELLOW="\033[0;33m"
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

USE_DOCKER="auto"

while [[ $# -gt 0 ]]; do
    case "$1" in
        --docker)
            USE_DOCKER="always"
            shift
            ;;
        --local)
            USE_DOCKER="never"
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Package kseek for KDE Store / KNewStuff (declarative mode)."
            echo ""
            echo "Options:"
            echo "  --docker    Build inside Ubuntu 24.04 container for maximum glibc compatibility (default if docker is available)"
            echo "  --local     Build locally using host compiler and libraries"
            echo "  -h, --help  Show this help message"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            exit 1
            ;;
    esac
done

# 1. Version and architecture
VERSION="$(tr -d '[:space:]' < "${REPO_ROOT}/VERSION")"
ARCH="$(uname -m)"
DIST_DIR="${REPO_ROOT}/dist"
ARCHIVE_NAME="kseek-v${VERSION}-linux-${ARCH}.tar.gz"
ARCHIVE_PATH="${DIST_DIR}/${ARCHIVE_NAME}"

# Decide build strategy
if [[ "$USE_DOCKER" == "auto" ]]; then
    if command -v docker >/dev/null 2>&1 && docker info >/dev/null 2>&1; then
        USE_DOCKER="always"
    else
        USE_DOCKER="never"
    fi
fi

if [[ "$USE_DOCKER" == "always" ]]; then
    log_info "Packaging kseek v${VERSION} for KDE Store using Ubuntu 24.04 Docker container (${ARCH})..."
    log_info "This ensures a glibc 2.39 baseline for 100% compatibility across all Plasma 6 distributions."

    mkdir -p "$DIST_DIR"
    docker build \
        -f "${REPO_ROOT}/packaging/Dockerfile.kdestore" \
        --target export \
        --output "type=local,dest=${DIST_DIR}" \
        "$REPO_ROOT"
else
    log_info "Packaging kseek v${VERSION} for KDE Store locally on host (${ARCH})..."
    if [[ "$USE_DOCKER" == "never" ]]; then
        log_warn "Building on host environment. Note: if published to store.kde.org, host glibc version will be required by users."
    fi

    BUILD_DIR="${REPO_ROOT}/build-release"
    STAGING_DIR="${DIST_DIR}/staging_kdestore"

    log_info "Configuring and compiling release binary..."
    cmake -B "$BUILD_DIR" -S "$REPO_ROOT" \
        -DCMAKE_BUILD_TYPE=Release \
        -DSTATIC_RUNTIME=ON \
        -DBUILD_TESTING=OFF

    cmake --build "$BUILD_DIR" -j"$(nproc 2>/dev/null || echo 2)" --target kseek

    KSEEK_BIN="${BUILD_DIR}/kseek"
    if [[ ! -f "$KSEEK_BIN" ]]; then
        log_error "Compiled binary not found at ${KSEEK_BIN}"
        exit 1
    fi

    log_info "Assembling flat declarative package staging..."
    rm -rf "$STAGING_DIR"
    mkdir -p "$STAGING_DIR" "$DIST_DIR"

    install -m 0755 "$KSEEK_BIN" "${STAGING_DIR}/kseek"
    install -m 0644 "${REPO_ROOT}/packaging/krunner-plugininstallerrc" "${STAGING_DIR}/krunner-plugininstallerrc"
    sed "s/@PROJECT_VERSION@/${VERSION}/g" \
        "${REPO_ROOT}/src/plasma-runner-kseek.desktop.in" > "${STAGING_DIR}/plasma-runner-kseek.desktop"
    chmod 0644 "${STAGING_DIR}/plasma-runner-kseek.desktop"

    install -m 0644 "${REPO_ROOT}/README.md" "${STAGING_DIR}/README.md"
    install -m 0644 "${REPO_ROOT}/LICENSE" "${STAGING_DIR}/LICENSE"

    log_info "Creating flat archive ${ARCHIVE_NAME}..."
    rm -f "$ARCHIVE_PATH"
    tar -czf "$ARCHIVE_PATH" -C "$STAGING_DIR" .
    rm -rf "$STAGING_DIR"
fi

# Verification
echo ""
log_success "KDE Store package created successfully!"
echo ""
echo -e "${BOLD}Package:${RESET}   ${ARCHIVE_PATH}"
echo -e "${BOLD}Size:${RESET}      $(du -h "$ARCHIVE_PATH" | cut -f1)"
echo -e "${BOLD}SHA256:${RESET}    $(sha256sum "$ARCHIVE_PATH" | cut -d' ' -f1)"
echo ""
echo -e "${BOLD}Archive Contents:${RESET}"
tar -ztvf "$ARCHIVE_PATH"
echo ""
log_info "Ready for upload to store.kde.org under 'Plasma 6 System Runners' or 'App Runners'."
