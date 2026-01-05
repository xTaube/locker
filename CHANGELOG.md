# Changelog

All notable changes to this project will be documented in this file.

This project follows **Semantic Versioning**  
(https://semver.org/) and the format is inspired by  
**Keep a Changelog** (https://keepachangelog.com/).

---

## [0.1.2] - 2026-01-05

### Fixed
- Added pre-compiled files to project so install is possible

## [0.1.0] - 2025-12-28

### Added
- Initial release of Locker
- Encrypted SQLite database stored as a single file
- Console-based UI built with ncurses
- Ability to add, edit, remove secrets
- Phrase search across stored items
- Argon2id (v1.3) for master key derivation
- XChaCha20-Poly1305 (AEAD) encryption
- Debug and release build configurations via Makefile
