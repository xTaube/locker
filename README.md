# Locker

**Locker** is a small, local, console-based password and secret manager written in C.

The main motivation behind this project was the need for a **simple, fast, and offline tool** to store API keys, secrets, and account credentials for various personal projects — without relying on cloud services, browsers, or heavyweight password managers.

Locker is designed to keep secrets **encrypted at rest**, **decrypted only in memory**, and easily accessible through a minimal ncurses-based UI.

---

## Features

- Local, offline secret storage
- Single encrypted locker database
- Console-based UI using **ncurses**
- Fast search across stored secrets
- Add, edit, and remove secrets
- Designed for developers and small projects
- No plaintext secrets written to disk

---

## Security Design

Locker focuses on strong, modern cryptography while keeping the implementation simple and auditable.

### Key Derivation
- **Algorithm:** Argon2id (RFC 9106, version 1.3)
- Used to derive the master key from the user-provided password
- Designed to be resistant to GPU and side-channel attacks

### Encryption
- **Algorithm:** XChaCha20-Poly1305 (AEAD)
- Authenticated encryption ensures both confidentiality and integrity

### Storage Model
- Uses **SQLite** as the internal data model
- The SQLite database file is **encrypted as a whole**
- Decryption happens only after successful authentication
- All read/write operations operate on the in-memory decrypted database
- Under normal operation, decrypted form is **never written to disk**
- Once the application exits, plaintext data is gone

---

## User Interface

- Text-based UI built with **ncurses**
- Minimal and distraction-free
- Keyboard-driven navigation
- Designed for terminal users

---

## ⚠ Limitations

- No cloud sync
- No multi-user support
- No GUI
- No browser integration
- Designed for local, trusted environments

---

## Suported Platforms
- Linux(x86_64, aarch64)
- MacOS(Intel, Apple Silicon)

---

## Installation & Build

Locker is a **source-only release**. Users must compile it locally to ensure security and compatibility.

### Requirements

- C compiler supporting at least **C11** (tested with `gcc >= 10`, `clang >= 12`)
- GNU Make
- Standard development environment on Linux or MacOS

> Note: Locker vendors its dependencies (libsodium, SQLite, ncurses) to ensure reproducible builds. No system libraries are required.

### Build Steps

1. Clone the repository:

```bash
git clone https://github.com/xTaube/locker.git
cd locker
```

2. Build release version:
```bash
make install CC={your compiler} C_VERSION={at lease c11} INSTALL_DIR={location where locker dir will be created}
```

3. Run Locker:
```bash
cd $(INSTALL_DIR)/locker
./locker
```

---

## Project Status

Locker is actively developed and intended for personal and small-scale use.  
While modern cryptography is used, this project has **not been independently audited**.

Use at your own risk.

---

## Disclaimer

This software is provided **as is**, without warranty of any kind.  
Always keep backups of important data.
