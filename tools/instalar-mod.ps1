# Instala ou remove o mod zelda-oot-perfis na pasta de mods do Fritchii.
#
# Instalar:  powershell -ExecutionPolicy Bypass -File tools\instalar-mod.ps1
# Remover:   powershell -ExecutionPolicy Bypass -File tools\instalar-mod.ps1 -Remover
#
# Ao instalar, na ordem: confere que o emulador esta fechado, faz o backup dos saves
# (tools\backup-saves.ps1, para se falhar), guarda uma copia do qt-config.ini, liga as
# mensagens de debug do jogo no log (Debug_Emulated:Debug) e copia o subsdk9.
# Ao remover: apaga so a pasta deste mod e volta o filtro de log que estava antes.
#
# (Mensagens sem acento de proposito: o PowerShell 5.1 le scripts sem BOM como ANSI.)

param(
    [switch]$Remover,
    [string]$Dados = (Join-Path $env:APPDATA 'fritchii')
)

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$pastaMod = Join-Path $Dados 'load\0400486024DEC000\zelda-oot-perfis'
$config = Join-Path $Dados 'config\qt-config.ini'
$guardado = Join-Path $Dados 'config\qt-config.ini.antes-do-zelda-oot-perfis'
$filtroDiag = '*:Info Debug_Emulated:Debug'

# Fim de linha: [^\r\n]* em vez de .* para nao engolir o \r de arquivos CRLF.
$reFiltro = '(?m)^log_filter=[^\r\n]*'
$rePadrao = '(?m)^log_filter\\default=[^\r\n]*'

function Ler-Texto($caminho) {
    $bytes = [IO.File]::ReadAllBytes($caminho)
    $bom = $bytes.Length -ge 3 -and $bytes[0] -eq 0xEF -and $bytes[1] -eq 0xBB -and $bytes[2] -eq 0xBF
    $ini = $(if ($bom) { 3 } else { 0 })
    $texto = [Text.UTF8Encoding]::new($false).GetString($bytes, $ini, $bytes.Length - $ini)
    return @{ Texto = $texto; Bom = $bom }
}

function Gravar-Texto($caminho, $texto, $bom) {
    [IO.File]::WriteAllText($caminho, $texto, [Text.UTF8Encoding]::new($bom))
}

# Troca a linha inteira que casa com $re por $linha (literal: o $ nao e especial).
function Trocar-Linha($texto, $re, $linha) {
    return [regex]::Replace($texto, $re, [Text.RegularExpressions.MatchEvaluator] { param($m) $linha })
}

$emulador = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '^(lichii|eden)$' }
if ($emulador) {
    Write-Host 'ERRO: feche o emulador antes de instalar ou remover o mod.' -ForegroundColor Red
    exit 1
}
if (-not (Test-Path -LiteralPath $config)) {
    Write-Host "ERRO: configuracao nao encontrada: $config" -ForegroundColor Red
    exit 1
}

if ($Remover) {
    if (Test-Path -LiteralPath $pastaMod) {
        Remove-Item -LiteralPath $pastaMod -Recurse -Force
        Write-Host "Mod removido: $pastaMod"
    } else {
        Write-Host 'O mod nao estava instalado.'
    }
    if (Test-Path -LiteralPath $guardado) {
        $original = (Ler-Texto $guardado).Texto
        $atual = Ler-Texto $config
        $texto = $atual.Texto
        $m = [regex]::Match($original, $reFiltro)
        if ($m.Success) { $texto = Trocar-Linha $texto $reFiltro $m.Value }
        $m = [regex]::Match($original, $rePadrao)
        if ($m.Success) { $texto = Trocar-Linha $texto $rePadrao $m.Value }
        Gravar-Texto $config $texto $atual.Bom
        Remove-Item -LiteralPath $guardado
        Write-Host 'Filtro de log restaurado para o que estava antes.'
    }
    Write-Host 'OK: pronto.' -ForegroundColor Green
    exit 0
}

$subsdk = Join-Path $repo 'dist\load\0400486024DEC000\zelda-oot-perfis\exefs\subsdk9'
if (-not (Test-Path -LiteralPath $subsdk)) {
    Write-Host "ERRO: compile antes (make dist). Nao achei: $subsdk" -ForegroundColor Red
    exit 1
}

& powershell -NoProfile -ExecutionPolicy Bypass -File (Join-Path $PSScriptRoot 'backup-saves.ps1')
if ($LASTEXITCODE -ne 0) {
    Write-Host 'ERRO: o backup falhou; o mod NAO foi instalado.' -ForegroundColor Red
    exit 1
}

$atual = Ler-Texto $config
if (-not [regex]::IsMatch($atual.Texto, $reFiltro)) {
    Write-Host 'ERRO: nao achei a linha log_filter= no qt-config.ini; nada foi alterado.' -ForegroundColor Red
    exit 1
}
if (-not (Test-Path -LiteralPath $guardado)) {
    Copy-Item -LiteralPath $config -Destination $guardado
}
$texto = Trocar-Linha $atual.Texto $rePadrao 'log_filter\default=false'
$texto = Trocar-Linha $texto $reFiltro ('log_filter="' + $filtroDiag + '"')
Gravar-Texto $config $texto $atual.Bom
Write-Host "Filtro de log: $filtroDiag (copia do original em $guardado)"

New-Item -ItemType Directory -Force -Path (Join-Path $pastaMod 'exefs') | Out-Null
Copy-Item -LiteralPath $subsdk -Destination (Join-Path $pastaMod 'exefs\subsdk9') -Force
Write-Host "OK: mod instalado em $pastaMod" -ForegroundColor Green
