$ErrorActionPreference = 'Stop'
$certDir = Join-Path $PSScriptRoot 'certs'
New-Item -ItemType Directory -Force -Path $certDir | Out-Null
$pfx = Join-Path $certDir 'KeePassXC-OSPasskeys.pfx'
$cer = Join-Path $certDir 'KeePassXC-OSPasskeys.cer'

# Reuse existing PFX so LocalMachine trust stays valid across rebuilds
if ((Test-Path $pfx) -and (Test-Path $cer)) {
  Write-Host "Reusing existing $pfx"
  exit 0
}

$cert = New-SelfSignedCertificate `
  -Type Custom `
  -Subject 'CN=KeePassXC-OSPasskeys' `
  -KeyUsage DigitalSignature `
  -FriendlyName 'KeePassXC OS Passkeys Sideload' `
  -CertStoreLocation 'Cert:\CurrentUser\My' `
  -TextExtension @('2.5.29.37={text}1.3.6.1.5.5.7.3.3', '2.5.29.19={text}')

$password = ConvertTo-SecureString -String 'keepassxc' -Force -AsPlainText
Export-PfxCertificate -Cert $cert -FilePath $pfx -Password $password | Out-Null
Export-Certificate -Cert $cert -FilePath $cer | Out-Null

# Trust for sideloading (Current User) via X509Store to avoid UI prompts
$x509 = New-Object System.Security.Cryptography.X509Certificates.X509Certificate2($cer)
foreach ($name in @('TrustedPeople', 'Root')) {
  $store = New-Object System.Security.Cryptography.X509Certificates.X509Store($name, 'CurrentUser')
  $store.Open('ReadWrite')
  $store.Add($x509)
  $store.Close()
}

Write-Host "Created $pfx"
Write-Host "NOTE: First install may need admin to trust the .cer in LocalMachine\TrustedPeople + Root."
