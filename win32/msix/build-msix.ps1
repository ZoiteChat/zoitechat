[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [string] $SourceDir,

    [Parameter(Mandatory = $true)]
    [string] $OutputPath,

    [Parameter(Mandatory = $true)]
    [ValidatePattern('^\d+\.\d+\.\d+\.0$')]
    [string] $PackageVersion,

    [Parameter(Mandatory = $true)]
    [string] $IdentityName,

    [Parameter(Mandatory = $true)]
    [string] $Publisher,

    [Parameter(Mandatory = $true)]
    [string] $PublisherDisplayName,

    [switch] $SelfSign
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -AssemblyName System.Drawing

function Find-WindowsSdkTool {
    param(
        [Parameter(Mandatory = $true)]
        [string] $ToolName
    )

    $command = Get-Command $ToolName -ErrorAction SilentlyContinue
    if ($command) {
        return $command.Source
    }

    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    if (-not (Test-Path $kitsRoot)) {
        throw "Windows SDK bin directory was not found: $kitsRoot"
    }

    $tool = Get-ChildItem -Path $kitsRoot -Filter $ToolName -File -Recurse |
        Where-Object { $_.FullName -match '\\x64\\' } |
        Sort-Object {
            $versionDirectory = Split-Path (Split-Path $_.DirectoryName -Parent) -Leaf
            try {
                [version] $versionDirectory
            }
            catch {
                [version] '0.0'
            }
        } -Descending |
        Select-Object -First 1

    if (-not $tool) {
        throw "$ToolName was not found in the installed Windows SDK."
    }

    return $tool.FullName
}

function Escape-Xml {
    param(
        [Parameter(Mandatory = $true)]
        [string] $Value
    )

    return [System.Security.SecurityElement]::Escape($Value)
}

function New-MsixAsset {
    param(
        [Parameter(Mandatory = $true)]
        [System.Drawing.Image] $SourceImage,

        [Parameter(Mandatory = $true)]
        [int] $Width,

        [Parameter(Mandatory = $true)]
        [int] $Height,

        [Parameter(Mandatory = $true)]
        [string] $Path
    )

    $bitmap = [System.Drawing.Bitmap]::new($Width, $Height)
    try {
        $graphics = [System.Drawing.Graphics]::FromImage($bitmap)
        try {
            $graphics.Clear([System.Drawing.Color]::Transparent)
            $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $graphics.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality

            $scale = [Math]::Min($Width / $SourceImage.Width, $Height / $SourceImage.Height)
            $drawWidth = [Math]::Max(1, [int][Math]::Round($SourceImage.Width * $scale))
            $drawHeight = [Math]::Max(1, [int][Math]::Round($SourceImage.Height * $scale))
            $x = [int](($Width - $drawWidth) / 2)
            $y = [int](($Height - $drawHeight) / 2)

            $graphics.DrawImage($SourceImage, $x, $y, $drawWidth, $drawHeight)
        }
        finally {
            $graphics.Dispose()
        }

        $bitmap.Save($Path, [System.Drawing.Imaging.ImageFormat]::Png)
    }
    finally {
        $bitmap.Dispose()
    }
}

$source = (Resolve-Path $SourceDir).Path
if (-not (Test-Path (Join-Path $source 'zoitechat.exe'))) {
    throw "Staged ZoiteChat executable is missing from $source"
}

$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$templatePath = Join-Path $PSScriptRoot 'AppxManifest.xml.in'
$iconPath = Join-Path $repoRoot 'data\icons\zoitechat.png'

if (-not (Test-Path $templatePath)) {
    throw "MSIX manifest template is missing: $templatePath"
}
if (-not (Test-Path $iconPath)) {
    throw "ZoiteChat PNG icon is missing: $iconPath"
}

if ([System.IO.Path]::IsPathRooted($OutputPath)) {
    $output = [System.IO.Path]::GetFullPath($OutputPath)
}
else {
    $output = [System.IO.Path]::GetFullPath((Join-Path $PWD $OutputPath))
}
$outputDirectory = Split-Path $output -Parent
New-Item -ItemType Directory -Path $outputDirectory -Force | Out-Null

$stagingRoot = Join-Path $env:RUNNER_TEMP 'zoitechat-msix'
$verifyRoot = Join-Path $env:RUNNER_TEMP 'zoitechat-msix-verify'
$pfxPath = Join-Path $env:RUNNER_TEMP 'zoitechat-msix-dev.pfx'

foreach ($path in @($stagingRoot, $verifyRoot)) {
    if (Test-Path $path) {
        Remove-Item -Path $path -Recurse -Force
    }
}
if (Test-Path $pfxPath) {
    Remove-Item -Path $pfxPath -Force
}

New-Item -ItemType Directory -Path $stagingRoot -Force | Out-Null
$appRoot = Join-Path $stagingRoot 'ZoiteChat'
New-Item -ItemType Directory -Path $appRoot -Force | Out-Null
Copy-Item -Path (Join-Path $source '*') -Destination $appRoot -Recurse -Force

# copy.vcxproj stages everything needed by every Inno component. The Store
# package keeps the useful bundled plugins and scripting bridges while letting
# the Microsoft Store own application updates. The WinSparkle updater is the
# only bundled plugin deliberately removed from the Store package.
$excludeFiles = @(
    'portable-mode',
    'zoitechat-text.exe',
    'plugins\hcupd.dll',
    'WinSparkle.dll',
    'share\download.png'
)

foreach ($relativePath in $excludeFiles) {
    $path = Join-Path $appRoot $relativePath
    if (Test-Path $path) {
        Remove-Item -Path $path -Force
    }
}

foreach ($relativePath in @('share\doc\WinSparkle')) {
    $path = Join-Path $appRoot $relativePath
    if (Test-Path $path) {
        Remove-Item -Path $path -Recurse -Force
    }
}

foreach ($requiredPath in @(
    'zoitechat.exe',
    'plugins\hcnotifications-winrt.dll',
    'plugins\hcchecksum.dll',
    'plugins\hcexec.dll',
    'plugins\hcfishlim.dll',
    'plugins\hcsysinfo.dll',
    'share\system.png',
    'plugins\hclua.dll',
    'plugins\hcperl.dll',
    'plugins\hcpython3.dll',
    'plugins\hcpython38.dll'
)) {
    if (-not (Test-Path (Join-Path $appRoot $requiredPath))) {
        throw "MSIX staging is missing required file: $requiredPath"
    }
}

foreach ($requiredPattern in @('_cffi_backend.cp314*.pyd', '_cffi_backend.cp38*.pyd')) {
    $match = Get-ChildItem -Path $appRoot -Filter $requiredPattern -File -ErrorAction SilentlyContinue |
        Select-Object -First 1
    if ($null -eq $match) {
        throw "MSIX staging is missing required Python support file: $requiredPattern"
    }
}

foreach ($requiredPythonFile in @('zoitechat.py', '_zoitechat.py', 'hexchat.py', 'xchat.py')) {
    $path = Join-Path (Join-Path $appRoot 'python') $requiredPythonFile
    if (-not (Test-Path $path)) {
        throw "MSIX staging is missing required Python bridge file: $requiredPythonFile"
    }
}

$assets = Join-Path $stagingRoot 'Assets'
New-Item -ItemType Directory -Path $assets -Force | Out-Null

$image = [System.Drawing.Image]::FromFile($iconPath)
try {
    New-MsixAsset -SourceImage $image -Width 50 -Height 50 `
        -Path (Join-Path $assets 'StoreLogo.png')
    New-MsixAsset -SourceImage $image -Width 44 -Height 44 `
        -Path (Join-Path $assets 'Square44x44Logo.png')
    New-MsixAsset -SourceImage $image -Width 150 -Height 150 `
        -Path (Join-Path $assets 'Square150x150Logo.png')
}
finally {
    $image.Dispose()
}

$manifest = Get-Content $templatePath -Raw -Encoding UTF8
$manifest = $manifest.Replace('@IDENTITY_NAME@', (Escape-Xml $IdentityName))
$manifest = $manifest.Replace('@PUBLISHER@', (Escape-Xml $Publisher))
$manifest = $manifest.Replace('@PUBLISHER_DISPLAY_NAME@', (Escape-Xml $PublisherDisplayName))
$manifest = $manifest.Replace('@PACKAGE_VERSION@', (Escape-Xml $PackageVersion))

if ($manifest -match '@[A-Z0-9_]+@') {
    throw "Unexpanded MSIX manifest placeholder: $($Matches[0])"
}

$manifestPath = Join-Path $stagingRoot 'AppxManifest.xml'
[System.IO.File]::WriteAllText(
    $manifestPath,
    $manifest,
    [System.Text.UTF8Encoding]::new($false)
)

# Catch malformed XML before MakeAppx so CI reports a useful error.
[xml](Get-Content $manifestPath -Raw -Encoding UTF8) | Out-Null

$makeAppx = Find-WindowsSdkTool -ToolName 'MakeAppx.exe'
$signtool = Find-WindowsSdkTool -ToolName 'SignTool.exe'

if (Test-Path $output) {
    Remove-Item -Path $output -Force
}

& $makeAppx pack /d $stagingRoot /p $output /o
if ($LASTEXITCODE -ne 0) {
    throw "MakeAppx failed with exit code $LASTEXITCODE"
}

$certificate = $null
try {
    if ($SelfSign) {
        # Partner Center replaces this development signature with Microsoft's
        # trusted Store signature. The certificate subject must exactly match
        # the manifest Publisher for SignTool to accept the package.
        $certificate = New-SelfSignedCertificate `
            -Type Custom `
            -KeyUsage DigitalSignature `
            -Subject $Publisher `
            -CertStoreLocation 'Cert:\CurrentUser\My' `
            -HashAlgorithm SHA256 `
            -TextExtension @(
                '2.5.29.37={text}1.3.6.1.5.5.7.3.3',
                '2.5.29.19={text}'
            ) `
            -FriendlyName 'ZoiteChat MSIX CI signing certificate'

        if ($certificate.Subject -cne $Publisher) {
            throw "Generated certificate subject '$($certificate.Subject)' does not exactly match manifest Publisher '$Publisher'."
        }

        $passwordText = [Guid]::NewGuid().ToString('N')
        $password = ConvertTo-SecureString -String $passwordText -AsPlainText -Force
        Export-PfxCertificate -Cert $certificate -FilePath $pfxPath -Password $password | Out-Null

        & $signtool sign /fd SHA256 /f $pfxPath /p $passwordText $output
        if ($LASTEXITCODE -ne 0) {
            throw "SignTool failed with exit code $LASTEXITCODE"
        }

        $signature = Get-AuthenticodeSignature -FilePath $output
        if (-not $signature.SignerCertificate -or
            $signature.SignerCertificate.Subject -cne $Publisher) {
            throw 'MSIX signature does not match the manifest Publisher.'
        }
    }

    New-Item -ItemType Directory -Path $verifyRoot -Force | Out-Null
    & $makeAppx unpack /p $output /d $verifyRoot /o
    if ($LASTEXITCODE -ne 0) {
        throw "MakeAppx validation unpack failed with exit code $LASTEXITCODE"
    }

    [xml] $verifiedManifest = Get-Content (Join-Path $verifyRoot 'AppxManifest.xml') -Raw -Encoding UTF8
    $ns = New-Object System.Xml.XmlNamespaceManager($verifiedManifest.NameTable)
    $ns.AddNamespace('f', 'http://schemas.microsoft.com/appx/manifest/foundation/windows10')
    $identity = $verifiedManifest.SelectSingleNode('/f:Package/f:Identity', $ns)
    if (-not $identity) {
        throw 'Validated MSIX does not contain a package Identity.'
    }
    if ($identity.Name -cne $IdentityName -or
        $identity.Publisher -cne $Publisher -or
        $identity.Version -cne $PackageVersion) {
        throw 'Validated MSIX identity does not match the requested Store identity.'
    }

    foreach ($requiredPath in @(
        'ZoiteChat\zoitechat.exe',
        'ZoiteChat\plugins\hcnotifications-winrt.dll',
        'ZoiteChat\plugins\hcchecksum.dll',
        'ZoiteChat\plugins\hcexec.dll',
        'ZoiteChat\plugins\hcfishlim.dll',
        'ZoiteChat\plugins\hcsysinfo.dll',
        'ZoiteChat\share\system.png',
        'ZoiteChat\plugins\hclua.dll',
        'ZoiteChat\plugins\hcperl.dll',
        'ZoiteChat\plugins\hcpython3.dll',
        'ZoiteChat\plugins\hcpython38.dll'
    )) {
        if (-not (Test-Path (Join-Path $verifyRoot $requiredPath))) {
            throw "Validated MSIX is missing required file: $requiredPath"
        }
    }

    foreach ($requiredPattern in @('_cffi_backend.cp314*.pyd', '_cffi_backend.cp38*.pyd')) {
        $match = Get-ChildItem -Path (Join-Path $verifyRoot 'ZoiteChat') -Filter $requiredPattern -File -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($null -eq $match) {
            throw "Validated MSIX is missing required Python support file: $requiredPattern"
        }
    }

    foreach ($requiredPythonFile in @('zoitechat.py', '_zoitechat.py', 'hexchat.py', 'xchat.py')) {
        $path = Join-Path (Join-Path $verifyRoot 'ZoiteChat\python') $requiredPythonFile
        if (-not (Test-Path $path)) {
            throw "Validated MSIX is missing required Python bridge file: $requiredPythonFile"
        }
    }

    foreach ($excludedPath in @(
        'ZoiteChat\portable-mode',
        'ZoiteChat\zoitechat-text.exe',
        'ZoiteChat\plugins\hcupd.dll',
        'ZoiteChat\WinSparkle.dll',
        'ZoiteChat\share\download.png'
    )) {
        if (Test-Path (Join-Path $verifyRoot $excludedPath)) {
            throw "MSIX incorrectly contains excluded file/directory: $excludedPath"
        }
    }
}
finally {
    if ($certificate) {
        Remove-Item -Path "Cert:\CurrentUser\My\$($certificate.Thumbprint)" -Force -ErrorAction SilentlyContinue
    }
    if (Test-Path $pfxPath) {
        Remove-Item -Path $pfxPath -Force -ErrorAction SilentlyContinue
    }
    foreach ($path in @($stagingRoot, $verifyRoot)) {
        if (Test-Path $path) {
            Remove-Item -Path $path -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

if (-not (Test-Path $output)) {
    throw "Expected MSIX was not created: $output"
}

Write-Host "Created MSIX: $output"
