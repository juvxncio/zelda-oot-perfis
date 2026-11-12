# Gera os zips de distribuicao a partir do dist/ ja compilado (make dist):
#
#   dist\zelda-oot-perfis-v<versao>.zip         para extrair na pasta de mods do emulador
#                                               (Open Mod Data Location do jogo)
#   dist\zelda-oot-perfis-v<versao>-source.zip  o codigo-fonte (git archive do HEAD; a GPL-3.0
#                                               pede que o codigo acompanhe o binario)
#
# Uso: powershell -ExecutionPolicy Bypass -File tools\empacotar.ps1
#
# (Mensagens sem acento de proposito: o PowerShell 5.1 le scripts sem BOM como ANSI.)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

$repo = Split-Path -Parent $PSScriptRoot
$exefs = Join-Path $repo 'dist\load\0400486024DEC000\zelda-oot-perfis\exefs'
$ipsNome = 'E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348.ips'
foreach ($a in (Join-Path $exefs 'subsdk9'), (Join-Path $exefs $ipsNome)) {
    if (-not (Test-Path -LiteralPath $a)) { Write-Host "ERRO: compile antes (make dist). Falta: $a" -ForegroundColor Red; exit 1 }
}

$m = Select-String -LiteralPath (Join-Path $repo 'mod\modulo.c') -Pattern '#define VERSAO "([^"]+)"'
if (-not $m) { Write-Host 'ERRO: nao achei VERSAO em mod\modulo.c' -ForegroundColor Red; exit 1 }
$versao = $m.Matches[0].Groups[1].Value
if ($versao -match 'teste') { Write-Host "ERRO: a versao $versao e de teste; nao empacoto." -ForegroundColor Red; exit 1 }

$sujo = & git -C $repo status --porcelain
if ($sujo) { Write-Host 'ERRO: ha mudancas nao commitadas; o zip do codigo nao bateria com o binario.' -ForegroundColor Red; exit 1 }

$nome = "zelda-oot-perfis-v$versao"
$destino = Join-Path $repo 'dist'
$zipMod = Join-Path $destino "$nome.zip"
$zipFonte = Join-Path $destino "$nome-source.zip"

# Pasta temporaria com a estrutura que vai no zip: zelda-oot-perfis\exefs\...
$tmp = Join-Path ([IO.Path]::GetTempPath()) ("empacotar-" + [Guid]::NewGuid())
$pasta = Join-Path $tmp 'zelda-oot-perfis'
New-Item -ItemType Directory -Force -Path (Join-Path $pasta 'exefs') | Out-Null
try {
    Copy-Item -LiteralPath (Join-Path $exefs 'subsdk9') -Destination (Join-Path $pasta 'exefs')
    Copy-Item -LiteralPath (Join-Path $exefs $ipsNome) -Destination (Join-Path $pasta 'exefs')
    Copy-Item -LiteralPath (Join-Path $repo 'LICENSE') -Destination (Join-Path $pasta 'LICENSE.txt')
    Copy-Item -LiteralPath (Join-Path $repo 'mod\font-OFL.txt') -Destination (Join-Path $pasta 'font-OFL.txt')
    $leia = @"
zelda-oot-perfis v$versao - Save Profiles for The Legend of Zelda: Ocarina of Time (Switch 2)
Game 0400486024DEC000, update v131072 (1.1.1). Tested on Fritchii 5.8.4.

INSTALL
1. In the emulator, right-click the game > Open Mod Data Location.
2. Extract this zip into that folder, so you get:
   ...\0400486024DEC000\zelda-oot-perfis\exefs\subsdk9 (+ the .ips file)
3. Start the game: Options > last row "Save Profile" (Profile 1 = your current save).
Back up your save before using any mod.

INSTALAR
1. No emulador, clique com o botao direito no jogo > Open Mod Data Location.
2. Extraia este zip nessa pasta, para ficar:
   ...\0400486024DEC000\zelda-oot-perfis\exefs\subsdk9 (+ o arquivo .ips)
3. Abra o jogo: Opcoes > ultima linha "Perfil de save" (Perfil 1 = o seu save atual).
Faca backup do seu save antes de usar qualquer mod.

License: GPL-3.0 (LICENSE.txt). Source code: $nome-source.zip, distributed with this file.
"@
    [IO.File]::WriteAllText((Join-Path $pasta 'README.txt'), $leia.Replace("`r`n", "`n").Replace("`n", "`r`n"), [Text.UTF8Encoding]::new($false))

    foreach ($z in $zipMod, $zipFonte) { if (Test-Path -LiteralPath $z) { Remove-Item -LiteralPath $z } }
    # Entrada por entrada, com '/' nos nomes: o CreateFromDirectory do .NET Framework grava '\',
    # que alguns descompactadores nao tratam como pasta.
    $zip = [IO.Compression.ZipFile]::Open($zipMod, [IO.Compression.ZipArchiveMode]::Create)
    try {
        Get-ChildItem -LiteralPath $tmp -Recurse -File | Sort-Object FullName | ForEach-Object {
            $rel = $_.FullName.Substring($tmp.Length + 1).Replace('\', '/')
            [void][IO.Compression.ZipFileExtensions]::CreateEntryFromFile($zip, $_.FullName, $rel, [IO.Compression.CompressionLevel]::Optimal)
        }
    } finally {
        $zip.Dispose()
    }
} finally {
    Remove-Item -LiteralPath $tmp -Recurse -Force
}

& git -C $repo archive --format=zip "--prefix=$nome/" -o $zipFonte HEAD
if ($LASTEXITCODE -ne 0) { Write-Host 'ERRO: git archive falhou' -ForegroundColor Red; exit 1 }

Write-Host "OK: $zipMod" -ForegroundColor Green
Write-Host "OK: $zipFonte" -ForegroundColor Green
$z = [IO.Compression.ZipFile]::OpenRead($zipMod)
try { $z.Entries | ForEach-Object { '    ' + $_.FullName + '  (' + $_.Length + ' bytes)' } } finally { $z.Dispose() }
