#!/usr/bin/env python3
"""Análise do `main` do jogo (NSO já descomprimido), só com a biblioteca padrão.

    python -I tools/analisar_main.py achatar  <main descomprimido> <main.bin>
    python -I tools/analisar_main.py imports  <main descomprimido> [filtro]
    python -I tools/analisar_main.py textos   <main.bin> <regex> [--contexto N]

achatar  monta a imagem como fica na memória (byte N = offset N do módulo) e confere o build ID.
imports  lista os símbolos que o main importa (ex.: as funções nn::fs), com o nome desmanglado
         de forma simples. O filtro é uma regex sobre o nome mangled.
textos   procura strings ASCII terminadas em zero que casem com a regex e mostra o offset.

Nada é escrito fora do arquivo de saída do `achatar`. O NSO de entrada só é lido.
"""
import re
import struct
import sys

BUILD_ID = "E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348"
BASE = 0x80020000   # endereço onde a comunidade carrega o main (docs/getting-started.md)


def ler_nso(caminho):
    raw = open(caminho, "rb").read()
    if raw[:4] != b"NSO0":
        sys.exit("não é um NSO: %s" % caminho)
    flags = struct.unpack_from("<I", raw, 0xC)[0]
    if flags & 0x80:
        sys.exit("o NSO ainda está comprimido (ZBIC): rode a extração de novo")
    if flags & 0x7:
        sys.exit("o NSO está comprimido com LZ4: descomprima antes")
    return raw


def segmentos(raw):
    # (offset no arquivo, offset na memória, tamanho) de .text, .rodata e .data
    return [struct.unpack_from("<3I", raw, 0x10 + 0x10 * i) for i in range(3)]


def achatar(raw):
    segs = segmentos(raw)
    bss = struct.unpack_from("<I", raw, 0x3C)[0]
    topo = max(m + s for _, m, s in segs)
    img = bytearray(topo + bss)
    for f, m, s in segs:
        img[m:m + s] = raw[f:f + s]
    return img, segs, topo, bss


def cmd_achatar(entrada, saida):
    raw = ler_nso(entrada)
    build = raw[0x40:0x54].hex().upper()
    print("build id", build)
    if build != BUILD_ID:
        sys.exit("build ID diferente do esperado (%s): os endereços da comunidade não valem" % BUILD_ID)
    img, segs, topo, bss = achatar(raw)
    for nome, (_, m, s) in zip(("text", "ro", "data"), segs):
        print("%-5s %#010x..%#010x" % (nome, m, m + s))
    print("bss   %#010x..%#010x" % (topo, topo + bss))
    with open(saida, "wb") as g:
        g.write(img)
    print("gravado:", saida, len(img), "bytes")


def desmanglar(nome):
    # Só o suficiente para ler nomes como _ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci -> nn::fs::OpenFile
    if not nome.startswith("_ZN"):
        return nome
    partes, i = [], 3
    while i < len(nome) and nome[i].isdigit():
        j = i
        while nome[j].isdigit():
            j += 1
        n = int(nome[i:j])
        partes.append(nome[j:j + n])
        i = j + n
    return "::".join(partes) if partes else nome


def cmd_imports(entrada, filtro=None):
    raw = ler_nso(entrada)
    img, segs, _, _ = achatar(raw)
    ro = segs[1][1]
    # Cabeçalho NSO: 0x88 embedded, 0x90 dynstr, 0x98 dynsym (offset, tamanho), relativos ao .rodata
    dynstr_off, dynstr_tam = struct.unpack_from("<2I", raw, 0x90)
    dynsym_off, dynsym_tam = struct.unpack_from("<2I", raw, 0x98)
    dynstr = img[ro + dynstr_off:ro + dynstr_off + dynstr_tam]
    rx = re.compile(filtro) if filtro else None
    vistos = 0
    for k in range(dynsym_tam // 24):
        nome_off, info, _, shndx, valor, _ = struct.unpack_from("<IBBHQQ", img, ro + dynsym_off + 24 * k)
        if shndx != 0 or nome_off == 0:
            continue   # (só os importados: definidos em outro módulo)
        fim = dynstr.index(b"\0", nome_off)
        nome = dynstr[nome_off:fim].decode("ascii", "replace")
        if rx and not rx.search(nome):
            continue
        vistos += 1
        print("%-45s %s" % (desmanglar(nome), nome))
    print("(%d imports)" % vistos)


def cmd_textos(entrada, padrao, contexto=0):
    img = open(entrada, "rb").read()
    rx = re.compile(padrao.encode())
    achados = 0
    for m in re.finditer(rb"[\x20-\x7e]{4,}\x00", img):
        s = m.group()[:-1]
        if not rx.search(s):
            continue
        achados += 1
        print("%#010x  (main+%#x)  %s" % (BASE + m.start(), m.start(), s.decode()))
        if contexto:
            ini = max(0, m.start() - contexto)
            vizinhos = re.findall(rb"[\x20-\x7e]{4,}", img[ini:m.end() + contexto])
            print("            vizinhos:", [v.decode() for v in vizinhos])
    print("(%d textos)" % achados)


def main():
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == "achatar":
        cmd_achatar(a[1], a[2])
    elif len(a) in (2, 3) and a[0] == "imports":
        cmd_imports(a[1], a[2] if len(a) == 3 else None)
    elif len(a) >= 3 and a[0] == "textos":
        ctx = int(a[a.index("--contexto") + 1]) if "--contexto" in a else 0
        cmd_textos(a[1], a[2], ctx)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    for s in (sys.stdout, sys.stderr):
        s.reconfigure(encoding="utf-8", errors="replace")
    main()
