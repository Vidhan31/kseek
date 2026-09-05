# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [1.3.0] - 2026-09-05

### Added
- Direct configuration file loading from `~/.config/kseek/kseek.conf` with INI key-value support.
- Default `kseek.conf` template pre-configured with `root = ~` and `fzf_args = --scheme=path`.
- Command-line option `-c, --config <path>` to specify a custom configuration file.
- Dynamic configuration reload via D-Bus `Config()` call.
- Declarative KDE Store packaging (`krunner-plugininstaller`) via `package-kdestore.sh`.

### Changed
- Updated development install and uninstall scripts (`install-dev.sh`, `uninstall-dev.sh`) for the new configuration path.
- Updated documentation with a configuration reference table and D-Bus reload instructions.

### Removed
- Legacy Python installation cleanup logic and systemctl invocations from startup.
- Obsolete `scripts/cleanup-legacy.sh` script.
- Systemd user service unit and `environment.d` dependency in favor of D-Bus activation.

## [1.2.1] - 2026-09-03

### Fixed
- Preserved whitespace in queries passed to `fzf`.
- Formatted home directory result paths with `~/` in KRunner matches.

### Changed
- Updated README with a quick start guide, configuration recipes, and links to `fd` and `fzf` documentation.

## [1.2.0] - 2026-09-03

### Changed
- Refactored process pipeline to rely on standard `fzf` defaults, removing hardcoded `--scheme=path`, `--algo=v2`, and `--tiebreak` flags.
- Preserved user-configured `FZF_DEFAULT_OPTS`, `FZF_DEFAULT_COMMAND`, and `FZF_DEFAULT_OPTS_FILE` environment variables.
- Added `--no-print-query` to prevent search queries from being echoed as candidate file paths when `--print-query` is set in user environments.

### Documentation
- Updated README with visual demonstrations showing kseek in action.

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

[Unreleased]: https://github.com/vidhan31/kseek/compare/v1.3.0...HEAD
[1.3.0]: https://github.com/vidhan31/kseek/compare/v1.2.1...v1.3.0
[1.2.1]: https://github.com/vidhan31/kseek/compare/v1.2.0...v1.2.1
[1.2.0]: https://github.com/vidhan31/kseek/compare/v1.1.0...v1.2.0
[1.1.0]: https://github.com/vidhan31/kseek/compare/v1.0.0...v1.1.0
[1.0.0]: https://github.com/vidhan31/kseek/releases/tag/v1.0.0
