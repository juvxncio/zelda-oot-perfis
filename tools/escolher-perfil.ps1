# Escolhe o perfil de save que o mod usa na proxima vez que o jogo abrir.
#
# Uso:  powershell -ExecutionPolicy Bypass -File tools\escolher-perfil.ps1 -Perfil 2
#       powershell -ExecutionPolicy Bypass -File tools\escolher-perfil.ps1          (so mostra)
#
# Grava o numero em save\...\0400486024DEC000\perfil_ativo.txt (ASCII, sem BOM). Perfil 1 = o
# save original, nos caminhos de sempre. Nao mexe em nenhum save.
#
# (Mensagens sem acento de proposito: o PowerShell 5.1 le scripts sem BOM como ANSI.)

param(
    [ValidateRange(1, 9)] [int]$Perfil = 0,
    [string]$Saves = (Join-Path $env:APPDATA 'fritchii\nand\user\save\0000000000000000')
)

$ErrorActionPreference = 'Stop'

$pastas = @(Get-ChildItem -LiteralPath $Saves -Directory | ForEach-Object { Join-Path $_.FullName '0400486024DEC000' } | Where-Object { Test-Path -LiteralPath $_ })
if ($pastas.Count -ne 1) {
    Write-Host "ERRO: esperava 1 usuario com save do jogo em $Saves, achei $($pastas.Count)." -ForegroundColor Red
    exit 1
}
$jogo = $pastas[0]
$arquivo = Join-Path $jogo 'perfil_ativo.txt'

if ($Perfil -ne 0) {
    $emulador = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(lichii|eden)$' }
    if ($emulador) {
        Write-Host 'ERRO: feche o emulador antes de trocar de perfil.' -ForegroundColor Red
        exit 1
    }
    [IO.File]::WriteAllBytes($arquivo, [Text.Encoding]::ASCII.GetBytes([string]$Perfil))
}

$atual = 1
if (Test-Path -LiteralPath $arquivo) {
    $texto = [IO.File]::ReadAllText($arquivo).Trim()
    if ($texto -match '^[1-9]$') { $atual = [int]$texto }
}

Write-Host "Perfil ativo: $atual" -ForegroundColor Green
Write-Host "  Perfil 1: save original ($jogo)"
Get-ChildItem -LiteralPath $jogo -Directory | Where-Object { $_.Name -match '^perfil[2-9]$' } | ForEach-Object {
    $slots = @(Get-ChildItem -LiteralPath $_.FullName -Directory | Where-Object { $_.Name -match '^slot_\d\d$' }).Count
    Write-Host ("  Perfil {0}: {1} slot(s) em {2}" -f $_.Name.Substring(6), $slots, $_.FullName)
}
