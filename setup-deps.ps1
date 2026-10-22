$ErrorActionPreference = 'Stop'

# LanHouse: cópia validada no NOSSO fork (Release deps-v19 = moonlight-stream/moonlight-qt-deps v19),
# não o repositório upstream - regra do projeto: binário de terceiro sob nosso controle, com SHA256.
# Só x64: os builds LanHouse para Windows são x64.
$Organization = "brunoerico"
$PrebuiltRepo = "moonlight-qt"
$TargetDir = Join-Path $PSScriptRoot "libs\windows"
$Assets = @{ "windows-x64.zip" = "65ff1bd439222e1e2750054f960dcd6fac46190027da16db7403e345c295f10f" }
$Tag = "deps-v19"

if (Test-Path $TargetDir) {
    Write-Host "Cleaning target directory..." -ForegroundColor Cyan
    Remove-Item -Path "$TargetDir\*" -Recurse -Force
} else {
    New-Item -ItemType Directory -Path $TargetDir | Out-Null
}

foreach ($AssetName in $Assets.Keys) {
    $Url = "https://github.com/$Organization/$PrebuiltRepo/releases/download/$Tag/$AssetName"
    $ArchivePath = Join-Path $env:TEMP $AssetName

    Write-Host "Downloading $AssetName..." -ForegroundColor Cyan
    curl.exe -s -L -f -o "$ArchivePath" "$Url"
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }

    $Hash = (Get-FileHash -Algorithm SHA256 $ArchivePath).Hash.ToLower()
    if ($Hash -ne $Assets[$AssetName]) {
        Write-Error "SHA256 de $AssetName não confere: $Hash"
    }

    Write-Host "Extracting $AssetName..." -ForegroundColor Cyan
    Expand-Archive -Path $ArchivePath -DestinationPath $TargetDir -Force
    Remove-Item $ArchivePath
}

Write-Host "Dependencies successfully deployed" -ForegroundColor Green