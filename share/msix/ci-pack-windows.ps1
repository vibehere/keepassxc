# Pack a Windows Release build into portable ZIP + sideload MSIX for CI/GitHub Releases.
param(
  [Parameter(Mandatory = $true)][string]$BuildDir,
  [Parameter(Mandatory = $true)][string]$OutDir,
  [Parameter(Mandatory = $true)][string]$Version,
  [Parameter(Mandatory = $true)][string]$QtBin,
  [string]$RepoRoot = ""
)

$ErrorActionPreference = 'Stop'
if (-not $RepoRoot) {
  $RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
}

$exeCandidates = @(
  (Join-Path $BuildDir 'src\Release\KeePassXC.exe'),
  (Join-Path $BuildDir 'src\KeePassXC.exe'),
  (Join-Path $BuildDir 'Release\KeePassXC.exe'),
  (Join-Path $BuildDir 'KeePassXC.exe')
)
$exe = $exeCandidates | Where-Object { Test-Path $_ } | Select-Object -First 1
if (-not $exe) {
  Get-ChildItem $BuildDir -Recurse -Filter 'KeePassXC.exe' -ErrorAction SilentlyContinue |
    Select-Object -First 5 FullName | ForEach-Object { Write-Host $_.FullName }
  throw "KeePassXC.exe not found under $BuildDir"
}
Write-Host "Using exe: $exe"

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$stage = Join-Path $OutDir 'windows-portable'
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Copy-Item $exe (Join-Path $stage 'KeePassXC.exe')

function Find-SiblingExe([string]$Name) {
  $paths = @(
    (Join-Path (Split-Path $exe -Parent) $Name),
    (Join-Path $BuildDir "src\cli\$Name"),
    (Join-Path $BuildDir "src\cli\Release\$Name"),
    (Join-Path $BuildDir "src\proxy\$Name"),
    (Join-Path $BuildDir "src\proxy\Release\$Name"),
    (Join-Path $BuildDir "cli\$Name"),
    (Join-Path $BuildDir "proxy\$Name")
  )
  foreach ($p in $paths) {
    if (Test-Path $p) { return $p }
  }
  $found = Get-ChildItem $BuildDir -Recurse -Filter $Name -ErrorAction SilentlyContinue | Select-Object -First 1
  if ($found) { return $found.FullName }
  return $null
}

$cli = Find-SiblingExe 'keepassxc-cli.exe'
$proxy = Find-SiblingExe 'keepassxc-proxy.exe'
if ($cli) { Copy-Item $cli $stage; Write-Host "cli: $cli" }
if ($proxy) { Copy-Item $proxy $stage; Write-Host "proxy: $proxy" }

$windeployqt = Join-Path $QtBin 'windeployqt.exe'
if (-not (Test-Path $windeployqt)) { throw "windeployqt not found: $windeployqt" }
& $windeployqt --release --no-translations --no-system-d3d-compiler --no-opengl-sw `
  (Join-Path $stage 'KeePassXC.exe')
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

$releaseDir = Split-Path $exe -Parent
Get-ChildItem $releaseDir -Filter '*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
  Copy-Item $_.FullName $stage -Force
}
# Also pick up vcpkg applocal deps from build root
Get-ChildItem $BuildDir -Filter '*.dll' -ErrorAction SilentlyContinue | ForEach-Object {
  Copy-Item $_.FullName $stage -Force
}

$zipName = "KeePassXC-OSPasskeys-${Version}-win64.zip"
$zipPath = Join-Path $OutDir $zipName
if (Test-Path $zipPath) { Remove-Item -Force $zipPath }
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zipPath -Force

$msixStage = Join-Path $OutDir 'msix-stage'
if (Test-Path $msixStage) { Remove-Item -Recurse -Force $msixStage }
New-Item -ItemType Directory -Force -Path (Join-Path $msixStage 'Assets') | Out-Null
Copy-Item (Join-Path $stage '*') $msixStage -Recurse -Force

function Write-MinimalPng([string]$Path) {
  # 1x1 blue PNG
  $b64 = 'iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR42mP8z8BQDwAEhQGAhKmMIQAAAABJRU5ErkJggg=='
  [IO.File]::WriteAllBytes($Path, [Convert]::FromBase64String($b64))
}

$assetsSrc = Join-Path $RepoRoot 'share\msix\Assets'
$assetNames = @('StoreLogo.png', 'Square44x44Logo.png', 'Square150x150Logo.png')
foreach ($name in $assetNames) {
  $dest = Join-Path $msixStage "Assets\$name"
  $src = Join-Path $assetsSrc $name
  if (Test-Path $src) {
    Copy-Item $src $dest -Force
  } else {
    Write-MinimalPng $dest
  }
}

$manifestSrc = Join-Path $RepoRoot 'share\msix\Package.appxmanifest'
$manifest = Get-Content -Raw -Encoding utf8 $manifestSrc
$verParts = @(($Version -replace '[^0-9.]', '').Split('.') | Where-Object { $_ -ne '' })
while ($verParts.Count -lt 4) { $verParts += '0' }
$msixVer = ($verParts[0..3] -join '.')
# Case-sensitive: do not touch XML declaration version="1.0"
$manifest = [regex]::Replace(
  $manifest,
  '(<Identity\b[^>]*\bVersion=")[^"]+(")',
  "`${1}$msixVer`${2}",
  1
)
if ($manifest -notmatch [regex]::Escape("Version=`"$msixVer`"")) {
  throw "failed to set Identity Version to $msixVer"
}
# makeappx rejects BOM and requires a clean XML declaration
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
$manifestPath = Join-Path $msixStage 'AppxManifest.xml'
[IO.File]::WriteAllText($manifestPath, $manifest.TrimStart([char]0xFEFF), $utf8NoBom)
Write-Host "AppxManifest Identity Version=$msixVer"
Write-Host (("manifest head: " + ((Get-Content -Raw $manifestPath).Substring(0, [Math]::Min(80, (Get-Item $manifestPath).Length)))))

$certDir = Join-Path $OutDir 'certs'
New-Item -ItemType Directory -Force -Path $certDir | Out-Null
$pfx = Join-Path $certDir 'KeePassXC-OSPasskeys.pfx'
$cer = Join-Path $certDir 'KeePassXC-OSPasskeys.cer'
$cert = New-SelfSignedCertificate -Type Custom -Subject 'CN=KeePassXC-OSPasskeys' `
  -KeyUsage DigitalSignature -FriendlyName 'KeePassXC OS Passkeys CI' `
  -CertStoreLocation 'Cert:\CurrentUser\My' `
  -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')
$pwd = ConvertTo-SecureString -String 'keepassxc' -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $pwd | Out-Null
Export-Certificate -Cert $cert -FilePath $cer | Out-Null

$sdkRoot = "${env:ProgramFiles(x86)}\Windows Kits\10\bin"
$makeappx = Get-ChildItem $sdkRoot -Recurse -Filter makeappx.exe -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match '\\x64\\makeappx.exe$' } |
  Sort-Object FullName -Descending | Select-Object -First 1
$signtool = Get-ChildItem $sdkRoot -Recurse -Filter signtool.exe -ErrorAction SilentlyContinue |
  Where-Object { $_.FullName -match '\\x64\\signtool.exe$' } |
  Sort-Object FullName -Descending | Select-Object -First 1
if (-not $makeappx) { throw 'makeappx.exe not found (Windows SDK)' }
if (-not $signtool) { throw 'signtool.exe not found (Windows SDK)' }

$msixPath = Join-Path $OutDir "KeePassXC-OSPasskeys-${Version}-win64.msix"
& $makeappx.FullName pack /d $msixStage /p $msixPath /o
if ($LASTEXITCODE -ne 0) { throw 'makeappx failed' }
& $signtool.FullName sign /fd SHA256 /f $pfx /p keepassxc $msixPath
if ($LASTEXITCODE -ne 0) { throw 'signtool failed' }

Copy-Item $cer (Join-Path $OutDir 'KeePassXC-OSPasskeys.cer') -Force

Get-ChildItem $OutDir -File | Select-Object Name, Length | Format-Table -AutoSize
Write-Host "WINDOWS_PACK_OK"
