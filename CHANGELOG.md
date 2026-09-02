# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.1.0] - 2026-09-02

### Added
- Multi-root search through multiple `--root` flags or colon-separated paths in `KSEEK_ROOT`.
- Development install and uninstall scripts (`install-dev.sh`, `uninstall-dev.sh`).
- Test suite and edge-case fixtures covering Unicode, spaces, dotfiles, symlinks, compound extensions, and case sensitivity.
- Package dependencies for `fd` and `fzf` across Debian, RPM, and Arch Linux packages.
- Default `kseek.conf` template installed to `/usr/lib/environment.d` and local prefixes.

### Changed
- Swapped external `head` and `awk` processes for in-process parsing and scoring.
- Moved packaging files into `packaging/` and service files into `src/`.

[Unreleased]: https://github.com/vidhan31/kseek/compare/v1.1.0...HEAD
[1.1.0]: https://github.com/vidhan31/kseek/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/vidhan31/kseek/releases/tag/v1.0.0
