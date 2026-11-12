#!/usr/bin/env python3
"""Análise do `main` do jogo (NSO já descomprimido), só com a biblioteca padrão.

    python -I tools/analisar_main.py achatar  <main descomprimido> <main.bin>
    python -I tools/analisar_main.py imports  <NSO descomprimido> [filtro]
    python -I tools/analisar_main.py exports  <NSO descomprimido> [filtro]
    python -I tools/analisar_main.py textos   <main.bin> <regex> [--contexto N]
    python -I tools/analisar_main.py refs     <main.bin> <offset> [<offset> ...]
    python -I tools/analisar_main.py asm      <main.bin> <offset> <instrucoes> [--llvm-mc <exe>]

achatar  monta a imagem como fica na memória (byte N = offset N do módulo) e confere o build ID.
imports  lista os símbolos que o NSO importa (ex.: as funções nn::fs), com o nome desmanglado
         de forma simples. O filtro é uma regex sobre o nome mangled.
exports  lista os símbolos que o NSO define e exporta (ex.: o que o sdk oferece), com o offset.
textos   procura strings ASCII terminadas em zero que casem com a regex e mostra o offset.
refs     acha no código os pares ADRP+ADD/LDR que apontam para os offsets dados (ex.: quem usa
         uma string), procurando o início provável da função (o STP/SUB SP mais próximo antes).
asm      desmonta instruções a partir de um offset com o llvm-mc do LLVM (padrão: no PATH).

Nada é escrito fora do arquivo de saída do `achatar`. O NSO de entrada só é lido.
"""
import hashlib
import re
import struct
import sys

BUILD_ID = "E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348"
BASE = 0x80020000   # endereço onde a comunidade carrega o main (docs/getting-started.md)


def lz4_bloco(src, tamanho):
    """Descomprime um bloco LZ4 (formato de bloco, sem cabeçalho de frame)."""
    out = bytearray()
    i = 0
    while i < len(src):
        token = src[i]
        i += 1
        n = token >> 4
        if n == 15:
            while True:
                b = src[i]
                i += 1
                n += b
                if b != 255:
                    break
        out += src[i:i + n]
        i += n
        if i >= len(src):
            break   # (a última sequência só tem literais)
        dist = src[i] | src[i + 1] << 8
        i += 2
        if dist == 0 or dist > len(out):
            raise ValueError("LZ4: distância inválida")
        n = token & 15
        if n == 15:
            while True:
                b = src[i]
                i += 1
                n += b
                if b != 255:
                    break
        n += 4
        ini = len(out) - dist
        for k in range(n):   # (byte a byte: a cópia pode sobrepor o que acabou de ser escrito)
            out.append(out[ini + k])
    if len(out) != tamanho:
        raise ValueError("LZ4: %d bytes, esperava %d" % (len(out), tamanho))
    return bytes(out)


def ler_nso(caminho):
    """Cabeçalho e segmentos (offset na memória, bytes) de .text, .rodata e .data."""
    raw = open(caminho, "rb").read()
    if raw[:4] != b"NSO0":
        sys.exit("não é um NSO: %s" % caminho)
    flags = struct.unpack_from("<I", raw, 0xC)[0]
    if flags & 0x80:
        sys.exit("o NSO ainda está comprimido (ZBIC): rode a extração de novo")
    comprimidos = struct.unpack_from("<3I", raw, 0x60)
    segs = []
    for i in range(3):
        f, m, s = struct.unpack_from("<3I", raw, 0x10 + 0x10 * i)
        dados = raw[f:f + comprimidos[i]] if flags & (1 << i) else raw[f:f + s]
        if flags & (1 << i):
            dados = lz4_bloco(dados, s)
        if flags & (8 << i) and hashlib.sha256(dados).digest() != raw[0xA0 + 0x20 * i:0xC0 + 0x20 * i]:
            sys.exit("segmento %d de %s: o SHA-256 não confere com o cabeçalho" % (i, caminho))
        segs.append((m, dados))
    return raw, segs


def achatar(nso):
    raw, segs = nso
    bss = struct.unpack_from("<I", raw, 0x3C)[0]
    topo = max(m + len(d) for m, d in segs)
    img = bytearray(topo + bss)
    for m, d in segs:
        img[m:m + len(d)] = d
    return img, segs, topo, bss


def cmd_achatar(entrada, saida):
    nso = ler_nso(entrada)
    build = nso[0][0x40:0x54].hex().upper()
    print("build id", build)
    if build != BUILD_ID:
        sys.exit("build ID diferente do esperado (%s): os endereços da comunidade não valem" % BUILD_ID)
    img, segs, topo, bss = achatar(nso)
    for nome, (m, d) in zip(("text", "ro", "data"), segs):
        print("%-5s %#010x..%#010x" % (nome, m, m + len(d)))
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


def simbolos(entrada):
    """(nome, shndx, valor) de cada símbolo dinâmico do NSO."""
    nso = ler_nso(entrada)
    raw = nso[0]
    img, segs, _, _ = achatar(nso)
    ro = segs[1][0]
    # Cabeçalho NSO: 0x88 embedded, 0x90 dynstr, 0x98 dynsym (offset, tamanho), relativos ao .rodata
    dynstr_off, dynstr_tam = struct.unpack_from("<2I", raw, 0x90)
    dynsym_off, dynsym_tam = struct.unpack_from("<2I", raw, 0x98)
    dynstr = img[ro + dynstr_off:ro + dynstr_off + dynstr_tam]
    for k in range(dynsym_tam // 24):
        nome_off, _, _, shndx, valor, _ = struct.unpack_from("<IBBHQQ", img, ro + dynsym_off + 24 * k)
        if nome_off == 0:
            continue
        fim = dynstr.index(b"\0", nome_off)
        yield dynstr[nome_off:fim].decode("ascii", "replace"), shndx, valor


def cmd_simbolos(entrada, exportados, filtro=None):
    rx = re.compile(filtro) if filtro else None
    vistos = 0
    for nome, shndx, valor in simbolos(entrada):
        if (shndx != 0) != exportados:
            continue   # (shndx 0 = importado, definido em outro módulo)
        if rx and not rx.search(nome):
            continue
        vistos += 1
        onde = "+%#x  " % valor if exportados else ""
        print("%s%-45s %s" % (onde, desmanglar(nome), nome))
    print("(%d %s)" % (vistos, "exports" if exportados else "imports"))


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


TEXTO_FIM = 0x3C461D0   # fim do .text deste build (docs/investigacao.md)


def _int(t):
    t = t.lower().replace("main+", "")
    v = int(t, 0)
    return v - BASE if v >= BASE else v


def inicio_funcao(ins, i, limite=4096):
    """Índice provável do início da função que contém a instrução i: o primeiro
    'STP X29, X30' / 'SUB SP, SP' / 'PACIBSP' visto voltando, depois de um RET/B."""
    for j in range(i, max(0, i - limite), -1):
        w = ins[j]
        if w == 0xD65F03C0 or (w & 0xFC000000) == 0x14000000:   # RET ou B incondicional antes
            if j < i:
                return j + 1
    return None


def cmd_refs(entrada, alvos):
    img = open(entrada, "rb").read()
    n = min(len(img), TEXTO_FIM) // 4
    ins = struct.unpack_from("<%dI" % n, img, 0)
    alvos = set(alvos)
    paginas = {a & ~0xFFF for a in alvos}
    achados = 0
    for i in range(n):
        w = ins[i]
        if (w & 0x9F000000) != 0x90000000:
            continue
        imm = ((w >> 29) & 3) | (((w >> 5) & 0x7FFFF) << 2)
        if imm & (1 << 20):
            imm -= 1 << 21
        pagina = ((i * 4) & ~0xFFF) + (imm << 12)
        if pagina not in paginas:
            continue
        rd = w & 31
        for j in range(i + 1, min(n, i + 24)):
            v = ins[j]
            rn = (v >> 5) & 31
            alvo = None
            if (v & 0xFF800000) == 0x91000000 and rn == rd:          # ADD Xd, Xn, #imm
                alvo = pagina + ((v >> 10) & 0xFFF)
            elif (v & 0xFFC00000) == 0xF9400000 and rn == rd:        # LDR Xt, [Xn, #imm]
                alvo = pagina + ((v >> 10) & 0xFFF) * 8
            if alvo is not None:
                if alvo in alvos:
                    f = inicio_funcao(ins, i)
                    achados += 1
                    print("main+%#x usa main+%#x   (função em main+%s)" % (
                        j * 4, alvo, "%#x" % (f * 4) if f is not None else "?"))
                break
    print("(%d referências)" % achados)


def cmd_asm(entrada, ini, qtd, llvm_mc="llvm-mc"):
    import subprocess
    img = open(entrada, "rb").read()
    dados = img[ini:ini + qtd * 4]
    hexa = " ".join("0x%02x" % b for b in dados)
    r = subprocess.run([llvm_mc, "--disassemble", "-triple=aarch64", "-mattr=+v8.5a,+crc,+crypto"],
                       input=hexa, capture_output=True, text=True)
    linhas = [l.strip() for l in r.stdout.splitlines() if l.strip() and not l.strip().startswith(".")]
    for k, l in enumerate(linhas):
        pc = ini + k * 4
        w = struct.unpack_from("<I", img, pc)[0]
        extra = ""
        if (w & 0x9F000000) == 0x90000000:   # ADRP: mostra a página como offset
            imm = ((w >> 29) & 3) | (((w >> 5) & 0x7FFFF) << 2)
            if imm & (1 << 20):
                imm -= 1 << 21
            extra = "   ; página main+%#x" % ((pc & ~0xFFF) + (imm << 12))
        elif (w & 0xFC000000) in (0x94000000, 0x14000000):   # BL / B
            imm = w & 0x3FFFFFF
            if imm & (1 << 25):
                imm -= 1 << 26
            extra = "   ; -> main+%#x" % (pc + imm * 4)
        print("main+%#09x  %08x  %s%s" % (pc, w, l, extra))
    if r.stderr.strip():
        print(r.stderr.strip()[:2000], file=sys.stderr)


def main():
    a = sys.argv[1:]
    if len(a) == 3 and a[0] == "achatar":
        cmd_achatar(a[1], a[2])
    elif len(a) in (2, 3) and a[0] in ("imports", "exports"):
        cmd_simbolos(a[1], a[0] == "exports", a[2] if len(a) == 3 else None)
    elif len(a) >= 3 and a[0] == "textos":
        ctx = int(a[a.index("--contexto") + 1]) if "--contexto" in a else 0
        cmd_textos(a[1], a[2], ctx)
    elif len(a) >= 3 and a[0] == "refs":
        cmd_refs(a[1], [_int(x) for x in a[2:]])
    elif len(a) >= 4 and a[0] == "asm":
        mc = a[a.index("--llvm-mc") + 1] if "--llvm-mc" in a else "llvm-mc"
        cmd_asm(a[1], _int(a[2]), int(a[3], 0), mc)
    else:
        sys.exit(__doc__)


if __name__ == "__main__":
    for s in (sys.stdout, sys.stderr):
        s.reconfigure(encoding="utf-8", errors="replace")
    main()
