# KeePassXC OS Passkeys Fork

This tree is a privacy-first fork of [KeePassXC](https://github.com/keepassxreboot/keepassxc) focused on **system-level passkeys** without requiring KeePassXC-Browser, while keeping **full KeePassXC-Browser compatibility** on all platforms.

## Enable OS Passkeys (opt-in)

OS Passkeys are **off by default**. After installing a release build:

1. Unlock your database.
2. Use the **OS Passkeys** toggle in the toolbar (or **Tools → OS Passkeys**).
3. Turn it **off** again when you are done.
4. Keep the database unlocked only while using a site’s passkey / security-key prompt.

KeePassXC-Browser passkeys are separate: enable **Passkeys** in the extension settings if you use that path instead.

## Which package for which OS

| System | Use |
| --- | --- |
| **Bazzite, Fedora Atomic, Aurora, Bluefin, Silverblue** | **Flatpak** (`.flatpak`) — preferred; AppImage also works |
| **Fedora Workstation, Arch, openSUSE, most Linux** | Flatpak or AppImage |
| **Debian / Ubuntu** | `.deb` (or Flatpak / AppImage) |
| **Windows** | `.msix` + trust `.cer` |

Download from GitHub Releases. Flatpak app id: `com.vibehere.KeePassXC.OSPasskeys`.

```bash
# Flatpak
flatpak install --user ./KeePassXC-OSPasskeys-*-x86_64.flatpak
sudo cp 99-keepassxc-uhid.rules /etc/udev/rules.d/
sudo udevadm control --reload-rules && sudo udevadm trigger --name-match=uhid
flatpak run com.vibehere.KeePassXC.OSPasskeys

# AppImage
chmod +x KeePassXC-OSPasskeys-*-x86_64.AppImage
./KeePassXC-OSPasskeys-*-x86_64.AppImage
```

## License

Same as upstream: **GNU GPL v3** (see [LICENSE](LICENSE)).

## Upstream

- Remote name: `upstream` → `https://github.com/keepassxreboot/keepassxc.git`
- Sync from: `upstream/develop`
- Feature branch: `feature/os-passkeys`

## GitHub Actions

GitHub disables Actions on new forks by default. Re-enable under **Actions** if you want release builds. This fork keeps only [`release.yml`](.github/workflows/release.yml) (tag `v*` or manual dispatch). Upstream CodeQL / Copilot setup workflows are removed here to avoid wasted minutes.

## What changes vs upstream

| Area | Behavior |
|------|----------|
| Passkey storage | Unchanged `KPEX_PASSKEY_*` entry attributes (DB compatible) |
| Browser extension | **Fully supported** everywhere (fill, TOTP, KPH fields, groups, Auto-Type, passkeys) |
| Windows OS passkeys | MSIX WebAuthn plugin authenticator (`IPluginAuthenticator`) — no addon needed for passkeys |
| Linux OS passkeys | Virtual FIDO2 device via `/dev/uhid` — no addon needed for passkeys (native Linux/VM; WSL often unsupported) |
| Privacy default | OS passkey provider is **opt-in** and requires an unlocked database + confirmation |
| WebAuthn bridge (optional) | Chromium extension / userscript that rewrites `authenticatorAttachment: "platform"` to `"cross-platform"` so USB / OS Passkeys keys work on restrictive sites |

## Optional WebAuthn bridge

Some sites request only a platform authenticator. Browsers then ignore USB / UHID security keys. This fork ships a small page-world bridge under `share/browser/webauthn-cross-platform-bridge/` (and a matching userscript) that relaxes that filter on all origins.

```bash
bash share/browser/enable-webauthn-cross-platform-bridge.sh
# then Load unpacked → ~/.local/share/webauthn-cross-platform-bridge
# or use the .user.js in a userscript manager (Firefox included)
```

## Build notes

- Keep `KPXC_FEATURE_BROWSER=ON` for full addon protocol compatibility.
- Enable `KPXC_FEATURE_PASSKEYS` (pulled in by browser and/or OS provider).
- Enable `KPXC_FEATURE_OS_PASSKEYS` on Windows or Linux for the OS provider.
- Windows OS provider only works from a **signed MSIX** install.
- Linux OS provider needs `uhid` + write access to `/dev/uhid` (not available in typical WSL).

### Linux packaging

```bash
# build
bash share/linux/build-linux.sh
# package
bash share/linux/pack-deb.sh
# verify (WSL: uhid expected missing)
bash share/linux/verify-linux.sh
```

## Security model

- Private keys never leave the KeePassXC process except into the encrypted KDBX.
- Windows credential-metadata APIs receive public identifiers only.
- Every create/assert requires local confirmation (Windows also requests OS user verification when available).
- Locked database → provider refuses ceremonies.
- Create uses the **active unlocked tab only** (never silently picks another vault).
- Assert searches **all unlocked vaults** and lets you pick the credential in the confirm dialog.
- Ceremony debug logging is off unless `KEEPASSXC_OSPASSKEYS_DEBUG=1`; never writes to `/tmp`.
