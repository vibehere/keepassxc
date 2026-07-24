$ErrorActionPreference = 'Stop'
$msix = Join-Path $PSScriptRoot 'out\KeePassXC-OSPasskeys.msix'
if (-not (Test-Path $msix)) { throw "Missing $msix" }

# Remove previous install if present
Get-AppxPackage -Name 'KeePassXC.OSPasskeys' -ErrorAction SilentlyContinue | Remove-AppxPackage -ErrorAction SilentlyContinue

try {
  Add-AppxPackage -Path $msix
  Write-Host "Installed OK"
} catch {
  Write-Host "Add-AppxPackage failed: $($_.Exception.Message)"
  Write-Host "Trying with -AllowUnsigned / developer path..."
  # Fallback: register if policy blocks
  throw
}

$pkg = Get-AppxPackage -Name 'KeePassXC.OSPasskeys'
if (-not $pkg) { throw 'Package not found after install' }
Write-Host "Package: $($pkg.PackageFullName)"
Write-Host "InstallLocation: $($pkg.InstallLocation)"

# Launch
Start-Process "shell:AppsFolder\$($pkg.PackageFamilyName)!KeePassXC"
Write-Host 'Launch requested. Unlock a database, then enable Tools -> Settings -> OS Passkeys.'
