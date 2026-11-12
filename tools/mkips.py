#!/usr/bin/env python3
"""Gera o patch IPS32 do main com os ganchos de mod/ganchos.def (só a biblioteca padrão).

    python -I tools/mkips.py <saida.ips> [--conferir <main.bin>]

O layout (caverna, desvios, slots) é o de mod/ganchos.h, lido daqui mesmo do cabeçalho para
os dois lados nunca divergirem. Com --conferir, o main achatado (tools/analisar_main.py
achatar) é usado para confirmar a instrução original em cada ponto e que a caverna e os slots
estão zerados; sem ele, vale o que ganchos.def declara.

Os offsets do IPS são offsets do main + 0x100 (o cabeçalho do NSO), como o Lichii/Eden/Fritchii
e o Atmosphère esperam. O arquivo deve se chamar <build id>.ips e ficar no exefs do mod.
"""
import os
import re
import struct
import sys

RAIZ = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CABECALHO_NSO = 0x100
NOP = 0xD503201F
BR_X16 = 0xD61F0200
X16 = 16


def ler_layout():
    h = open(os.path.join(RAIZ, "mod", "ganchos.h"), encoding="utf-8").read()

    def valor(nome):
        m = re.search(r"#define\s+%s\s+(.+?)(?:\s*/\*.*)?$" % nome, h, re.M)
        if not m:
            sys.exit("ganchos.h sem %s" % nome)
        expr = m.group(1).strip()
        expr = re.sub(r"\b(0x[0-9A-Fa-f]+|\d+)u\b", r"\1", expr)
        expr = re.sub(r"\b(GANCHOS_\w+)\b", lambda k: "(%d)" % valor(k.group(1)), expr)
        if expr.startswith('"'):
            return expr.strip('"')
        return int(eval(expr, {"__builtins__": {}}))

    lay = {n: valor(n) for n in ("GANCHOS_CAVERNA", "GANCHOS_DESVIOS", "GANCHOS_DESVIO_TAM",
                                 "GANCHOS_FIM", "GANCHOS_SLOTS", "GANCHOS_VERSAO")}
    lay["GANCHOS_MARCADOR"] = valor("GANCHOS_MARCADOR")
    return lay


def ler_ganchos():
    d = open(os.path.join(RAIZ, "mod", "ganchos.def"), encoding="utf-8").read()
    d = re.sub(r"/\*.*?\*/", "", d, flags=re.S)
    g = [(m.group(1), int(m.group(2), 16), int(m.group(3), 16))
         for m in re.finditer(r"^\s*GANCHO\(\s*(\w+)\s*,\s*(0x[0-9a-fA-F]+)\s*,\s*(0x[0-9a-fA-F]+)\s*\)", d, re.M)]
    nomes = [x[0] for x in g]
    if len(set(nomes)) != len(nomes) or len({x[1] for x in g}) != len(g):
        sys.exit("ganchos.def: nome ou ponto repetido")
    return g


def depende_do_pc(w):
    return ((w & 0x1F000000) == 0x10000000 or      # adr / adrp
            (w & 0x7C000000) == 0x14000000 or      # b / bl
            (w & 0xFF000010) == 0x54000000 or      # b.cond
            (w & 0x7E000000) == 0x34000000 or      # cbz / cbnz
            (w & 0x7E000000) == 0x36000000 or      # tbz / tbnz
            (w & 0x3B000000) == 0x18000000)        # ldr literal


def b(de, para):
    return 0x14000000 | (((para - de) >> 2) & 0x3FFFFFF)


def adrp(rd, de, para):
    paginas = ((para & ~0xFFF) - (de & ~0xFFF)) >> 12
    return 0x90000000 | (paginas & 3) << 29 | ((paginas >> 2) & 0x7FFFF) << 5 | rd


def ldr_x(rt, rn, off):
    assert off % 8 == 0 and off < 0x8000
    return 0xF9400000 | (off // 8) << 10 | rn << 5 | rt


def cbz_x(rt, de, para):
    return 0xB4000000 | ((((para - de) >> 2) & 0x7FFFF) << 5) | rt


def montar(lay, ganchos, img=None):
    """[(offset no main, bytes)]"""
    registros = []
    cav, desv, tam = lay["GANCHOS_CAVERNA"], lay["GANCHOS_DESVIOS"], lay["GANCHOS_DESVIO_TAM"]
    if desv + len(ganchos) * tam > lay["GANCHOS_FIM"]:
        sys.exit("a caverna não comporta %d ganchos" % len(ganchos))
    marcador = lay["GANCHOS_MARCADOR"].encode()
    assert len(marcador) == 8
    registros.append((cav, marcador + struct.pack("<II", lay["GANCHOS_VERSAO"], len(ganchos))))
    for i, (nome, ponto, original) in enumerate(ganchos):
        if depende_do_pc(original):
            sys.exit("%s: a instrução original %#010x depende do PC" % (nome, original))
        if img is not None:
            real = struct.unpack_from("<I", img, ponto)[0]
            if real != original:
                sys.exit("%s: em main+%#x há %#010x, não %#010x" % (nome, ponto, real, original))
        d = desv + i * tam
        slot = lay["GANCHOS_SLOTS"] + i * 8
        codigo = [adrp(X16, d, slot), ldr_x(X16, X16, slot & 0xFFF), cbz_x(X16, d + 8, d + 16),
                  BR_X16, original, b(d + 20, ponto + 4)]
        codigo += [NOP] * (tam // 4 - len(codigo))
        registros.append((d, struct.pack("<%dI" % len(codigo), *codigo)))
        registros.append((ponto, struct.pack("<I", b(ponto, d))))
    if img is not None:
        fim = lay["GANCHOS_FIM"]
        if any(img[cav:fim]):
            sys.exit("a caverna main+%#x..%#x não está zerada neste main" % (cav, fim))
        s = lay["GANCHOS_SLOTS"]
        if s + len(ganchos) * 8 > len(img) or any(img[s:s + len(ganchos) * 8]):
            sys.exit("os slots não estão no .bss zerado deste main")
    return registros


def gravar_ips(caminho, registros):
    with open(caminho, "wb") as f:
        f.write(b"IPS32")
        for off, dados in sorted(registros):
            f.write(struct.pack(">IH", off + CABECALHO_NSO, len(dados)))
            f.write(dados)
        f.write(b"EEOF")


def main():
    a = sys.argv[1:]
    if not a:
        sys.exit(__doc__)
    img = None
    if "--conferir" in a:
        img = open(a[a.index("--conferir") + 1], "rb").read()
    lay = ler_layout()
    ganchos = ler_ganchos()
    regs = montar(lay, ganchos, img)
    gravar_ips(a[0], regs)
    print("%s: %d ganchos, %d registros%s" % (a[0], len(ganchos), len(regs),
                                               " (conferido com o main)" if img is not None else ""))


if __name__ == "__main__":
    main()
