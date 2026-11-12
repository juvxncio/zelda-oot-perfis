# Extrai o exefs (o executavel do jogo, ~40 MB) ou, com -Parte romfs, os dados do jogo (~26 GB)
# do dump .nup, usando o nup.py do Hookshot.
# O .nup e aberto so para leitura; o script confere data e tamanho antes e depois.
#
# Uso:
#   powershell -ExecutionPolicy Bypass -File tools\extrair-exefs.ps1 -Hookshot <pasta do Hookshot> -Python <python.exe> -Saida <pasta> [-Parte exefs|romfs]
#
# (Mensagens sem acento de proposito: o PowerShell 5.1 le scripts sem BOM como ANSI.)

param(
    [Parameter(Mandatory)] [string]$Hookshot,
    [Parameter(Mandatory)] [string]$Python,
    [Parameter(Mandatory)] [string]$Saida,
    [ValidateSet('exefs', 'romfs')] [string]$Parte = 'exefs',
    [string]$Nup = (Join-Path $env:USERPROFILE 'Documents\ROMS Switch 2\The Legend of Zelda Ocarina of Time [0400486024dec000][v131072].nup'),
    [string]$Llvm = (Join-Path $env:LOCALAPPDATA 'Programs\llvm-23.1.5\bin')
)

$ErrorActionPreference = 'Stop'

foreach ($p in $Nup, $Python, (Join-Path $Hookshot 'nx2\nup.py'), (Join-Path $Llvm 'clang.exe')) {
    if (-not (Test-Path -LiteralPath $p)) { Write-Host "ERRO: nao encontrado: $p" -ForegroundColor Red; exit 1 }
}

$env:Path = "$Llvm;$env:Path"
$antes = (Get-Item -LiteralPath $Nup).LastWriteTimeUtc.Ticks, (Get-Item -LiteralPath $Nup).Length

& $Python -I (Join-Path $Hookshot 'nx2\nup.py') $Nup --extract $Saida --only $Parte
$codigo = $LASTEXITCODE

$depois = (Get-Item -LiteralPath $Nup).LastWriteTimeUtc.Ticks, (Get-Item -LiteralPath $Nup).Length
if (($antes -join ',') -ne ($depois -join ',')) {
    Write-Host 'ALERTA: o .nup mudou de data ou tamanho!' -ForegroundColor Red
    exit 1
}
Write-Host 'OK: .nup inalterado (data e tamanho conferidos).' -ForegroundColor Green

if ($codigo -ne 0) { Write-Host "ERRO: nup.py terminou com codigo $codigo" -ForegroundColor Red; exit $codigo }

$arquivos = @(Get-ChildItem -LiteralPath $Saida -Recurse -File)
if ($Parte -eq 'exefs') { $arquivos | Select-Object FullName, Length | Format-Table -AutoSize }
else { Write-Host ("{0} arquivos, {1:N1} GB" -f $arquivos.Count, (($arquivos | Measure-Object Length -Sum).Sum / 1GB)) }
