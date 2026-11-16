# MSIX packaging (Windows OS passkeys)

Windows only accepts third-party passkey providers that are declared inside a **signed MSIX** package.

## Contents

- `Package.appxmanifest` — declares KeePassXC.exe as a COM ExeServer for CLSID `A7C3E8F1-2B4D-4E9A-9C1F-8D6E5A4B3C2D`

## Build / sideload outline

1. Build KeePassXC with `KPXC_FEATURE_OS_PASSKEYS=ON` (default on Windows).
2. Stage `KeePassXC.exe` and dependencies into a package layout with this manifest and logo assets.
3. Sign with a code-signing certificate trusted on the machine.
4. Install via `Add-AppxPackage` (sideload) or Store distribution.
5. Unlock a database → turn on the **OS Passkeys** toolbar toggle.

Classic portable/MSI installs cannot register the plugin authenticator (`WebAuthNPluginAddAuthenticator` fails unpackaged).
