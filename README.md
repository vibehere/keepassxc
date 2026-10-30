# KeePassXC OS Passkeys

Privacy-first fork of [KeePassXC](https://github.com/keepassxreboot/keepassxc) that adds **system-level passkeys** (Windows WebAuthn plugin / Linux UHID FIDO2) while keeping full KeePassXC-Browser compatibility.

Upstream KeePassXC docs and community links still apply for core password-manager features. This README covers **what this fork adds**. Packaging, security model, and build notes live in [FORK.md](FORK.md).

## What this fork adds

- **OS Passkeys (opt-in)** — register and assert passkeys from an unlocked database without KeePassXC-Browser.
  - Windows: MSIX WebAuthn plugin authenticator
  - Linux: virtual FIDO2 security key via `/dev/uhid`
- **Neutral authenticator identity** — OS CTAP uses a generic AAGUID / name (not KeePassXC branding).
- **KeePassXC-Browser** — still fully supported for fill, TOTP, and extension passkeys.
- **Optional WebAuthn bridge** — for sites that set `authenticatorAttachment: "platform"` and would otherwise ignore USB / OS Passkeys keys:
  - Chromium extension: [`share/browser/webauthn-cross-platform-bridge/`](share/browser/webauthn-cross-platform-bridge/)
  - Userscript: [`share/browser/webauthn-cross-platform-bridge.user.js`](share/browser/webauthn-cross-platform-bridge.user.js)
  - Helper: `bash share/browser/enable-webauthn-cross-platform-bridge.sh`

## Quick start (OS Passkeys)

1. Install a release package (see [FORK.md](FORK.md) for Flatpak / AppImage / deb / MSIX).
2. Unlock your database.
3. Turn on the **OS Passkeys** toolbar toggle (or **Tools → OS Passkeys**).
4. Use the site’s passkey / security-key prompt, then turn the toggle off when finished.

On Linux, install the bundled udev rule so `/dev/uhid` is writable (details in [FORK.md](FORK.md)).

## Packages

| System | Prefer |
| --- | --- |
| Immutable / Atomic Linux (e.g. Fedora Atomic family) | Flatpak `com.vibehere.KeePassXC.OSPasskeys` |
| Most other Linux | Flatpak or AppImage (`.deb` also available) |
| Windows | Signed MSIX (+ trust the accompanying certificate) |

## Upstream KeePassXC

Core features (KDBX vaults, browser integration, TOTP, SSH agent, Secret Service, etc.) match upstream. See the [upstream README history](https://github.com/keepassxreboot/keepassxc) and [docs](https://keepassxc.org/docs/) for general usage.

- Upstream remote: `https://github.com/keepassxreboot/keepassxc.git`
- Sync from: `upstream/develop`
- Feature branch: `feature/os-passkeys`

## License

Same as upstream: **GPL-2 or GPL-3**. See [COPYING](COPYING) and [LICENSE](LICENSE).
