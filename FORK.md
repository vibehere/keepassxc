# KeePassXC OS Passkeys Fork

This tree is a privacy-first fork of [KeePassXC](https://github.com/keepassxreboot/keepassxc) focused on **system-level passkeys** without requiring KeePassXC-Browser, while keeping **full KeePassXC-Browser compatibility** on all platforms.

## Enable OS Passkeys (opt-in)

OS Passkeys are **off by default**. After installing a release build:

1. Unlock your database.
2. Open **Tools → Settings → OS Passkeys**.
3. Enable the provider (Linux virtual FIDO2 / Windows MSIX passkey provider).
4. Keep the database unlocked when using a site’s passkey / security-key prompt.

The app also shows a one-time tip after the first unlock if OS Passkeys are still disabled.

KeePassXC-Browser passkeys are separate: enable **Passkeys** in the extension settings if you use that path instead.

## License

Same as upstream: **GNU GPL v3** (see [LICENSE](LICENSE)).

## Upstream

- Remote name: `upstream` → `https://github.com/keepassxreboot/keepassxc.git`
- Sync from: `upstream/develop`
- Feature branch: `feature/os-passkeys`

## What changes vs upstream

| Area | Behavior |
|------|----------|
| Passkey storage | Unchanged `KPEX_PASSKEY_*` entry attributes (DB compatible) |
| Browser extension | **Fully supported** everywhere (fill, TOTP, KPH fields, groups, Auto-Type, passkeys) |
| Windows OS passkeys | MSIX WebAuthn plugin authenticator (`IPluginAuthenticator`) — no addon needed for passkeys |
| Linux OS passkeys | Virtual FIDO2 device via `/dev/uhid` — no addon needed for passkeys (native Linux/VM; WSL often unsupported) |
| Privacy default | OS passkey provider is **opt-in** and requires an unlocked database + confirmation |

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

Outputs under `share/linux/out/`:
- `keepassxc-ospasskeys_2.8.0_amd64.deb` — installable package (+ udev rule)
- `KeePassXC`, `keepassxc-proxy`, `keepassxc-cli` — loose binaries
- `verify-report.txt` — last smoke check

On native Linux: `sudo dpkg -i share/linux/out/keepassxc-ospasskeys_2.8.0_amd64.deb`, then Tools → Settings → OS Passkeys.

## Security model

- Private keys never leave the KeePassXC process except into the encrypted KDBX.
- Windows credential-metadata APIs receive public identifiers only.
- Every create/assert requires local confirmation.
- Locked database → provider refuses ceremonies.
