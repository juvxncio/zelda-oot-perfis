# Backup dos saves do Ocarina of Time (Switch 2) no Fritchii.
# Copia a pasta nand\user\save inteira para uma pasta nova com data e hora, confere cada
# arquivo por SHA-256 e nunca apaga nem sobrescreve nada.
#
# Uso:  powershell -ExecutionPolicy Bypass -File tools\backup-saves.ps1
#
# (Mensagens sem acento de proposito: o PowerShell 5.1 le scripts sem BOM como ANSI.)

param(
    [string]$Origem = (Join-Path $env:APPDATA 'fritchii\nand\user\save'),
    [string]$Destino = (Join-Path $env:USERPROFILE 'Documents\BACKUP_ZELDA_NAO_APAGAR')
)

$ErrorActionPreference = 'Stop'

$emulador = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(lichii|eden)$' }
if ($emulador) {
    Write-Host 'ERRO: feche o emulador antes do backup (ele pode estar gravando o save).' -ForegroundColor Red
    exit 1
}

if (-not (Test-Path $Origem)) {
    Write-Host "ERRO: pasta de saves nao encontrada: $Origem" -ForegroundColor Red
    exit 1
}

$alvo = Join-Path $Destino ('saves_' + (Get-Date -Format 'yyyy-MM-dd_HH-mm-ss'))
if (Test-Path $alvo) {
    Write-Host "ERRO: $alvo ja existe; nada foi alterado." -ForegroundColor Red
    exit 1
}

New-Item -ItemType Directory -Path $alvo -Force | Out-Null
Copy-Item -Path (Join-Path $Origem '*') -Destination $alvo -Recurse

$arquivos = Get-ChildItem $Origem -Recurse -File -Force
$diferentes = 0
foreach ($f in $arquivos) {
    $copia = Join-Path $alvo $f.FullName.Substring($Origem.Length).TrimStart('\')
    if (-not (Test-Path $copia) -or (Get-FileHash $f.FullName).Hash -ne (Get-FileHash $copia).Hash) {
        Write-Host "DIFERENTE: $($f.FullName)" -ForegroundColor Red
        $diferentes++
    }
}

if ($diferentes -gt 0) {
    Write-Host "FALHOU: $diferentes arquivo(s) nao conferem. NAO teste o mod." -ForegroundColor Red
    exit 1
}

Write-Host "OK: $($arquivos.Count) arquivos copiados e conferidos em:" -ForegroundColor Green
Write-Host "    $alvo"
