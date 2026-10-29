# KeePassXC OS Passkeys Fork

This tree is a privacy-first fork of [KeePassXC](https://github.com/keepassxreboot/keepassxc) focused on **system-level passkeys** without requiring KeePassXC-Browser, while keeping **full KeePassXC-Browser compatibility** on all platforms.

## Enable OS Passkeys (opt-in)

OS Passkeys are **off by default**. After installing a release build:

1. Unlock your database.
2. Use the **OS Passkeys** toggle in the toolbar (or **Tools → OS Passkeys**).
3. Turn it **off** again when you are done.
4. Keep the database unlocked only while using a site’s passkey / security-key prompt.

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

On native Linux: `sudo dpkg -i share/linux/out/keepassxc-ospasskeys_2.8.0_amd64.deb`, unlock a database, then use the **OS Passkeys** toolbar toggle.

## Security model

- Private keys never leave the KeePassXC process except into the encrypted KDBX.
- Windows credential-metadata APIs receive public identifiers only.
- Every create/assert requires local confirmation (Windows also requests OS user verification when available).
- Locked database → provider refuses ceremonies.
- Create uses the **active unlocked tab only** (never silently picks another vault).
- Assert searches **all unlocked vaults** and lets you pick the credential in the confirm dialog.
- Ceremony debug logging is off unless `KEEPASSXC_OSPASSKEYS_DEBUG=1`; never writes to `/tmp`.
