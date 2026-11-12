"""Leitura (so leitura) dos arquivos do romfs do jogo: zstd com os dicionarios do jogo, SARC e BYML.

    python -I tools/romfs.py zs    <romfs> <arquivo.zs> <saida>        descompacta
    python -I tools/romfs.py sarc  <romfs> <arquivo> [<pasta saida>]   lista (ou extrai) um SARC
    python -I tools/romfs.py byml  <romfs> <arquivo> [<saida.json>]    BYML em JSON
    python -I tools/romfs.py msbt  <romfs> <Mals/xx.sarc.zs> <nome.msbt|regex>   textos (rotulo = texto)

<arquivo> pode ser relativo ao romfs; .zs e descompactado sozinho. Nada no romfs e alterado.
Precisa do modulo zstandard.
"""
import json
import os
import struct
import sys

import zstandard


class Romfs:
    def __init__(self, raiz):
        self.raiz = raiz
        self._dics = None

    def caminho(self, rel):
        return rel if os.path.isabs(rel) else os.path.join(self.raiz, rel)

    def dicionarios(self):
        if self._dics is None:
            with open(os.path.join(self.raiz, "Pack", "ZsDic.pack.zs"), "rb") as f:
                pack = zstandard.ZstdDecompressor().decompress(f.read(), max_output_size=1 << 28)
            self._dics = {}
            for nome, dados in sarc_arquivos(pack):
                d = zstandard.ZstdCompressionDict(dados)
                self._dics[struct.unpack_from("<I", dados, 4)[0]] = d
        return self._dics

    def ler(self, rel):
        with open(self.caminho(rel), "rb") as f:
            dados = f.read()
        if not rel.endswith(".zs"):
            return dados
        # O id do dicionario (se houver) vem no cabecalho do frame zstd.
        params = zstandard.get_frame_parameters(dados)
        dic = self.dicionarios().get(params.dict_id) if params.dict_id else None
        z = zstandard.ZstdDecompressor(dict_data=dic) if dic else zstandard.ZstdDecompressor()
        return z.decompress(dados, max_output_size=1 << 30)


def sarc_arquivos(dados):
    """(nome, bytes) de cada arquivo de um SARC (little-endian)."""
    if dados[:4] != b"SARC":
        raise ValueError("nao e SARC")
    bom = dados[6:8]
    e = "<" if bom == b"\xff\xfe" else ">"
    hdr_len, = struct.unpack_from(e + "H", dados, 4)
    data_off, = struct.unpack_from(e + "I", dados, 0x0C)
    o = hdr_len
    assert dados[o:o + 4] == b"SFAT"
    n, = struct.unpack_from(e + "H", dados, o + 6)
    nos = [struct.unpack_from(e + "IIII", dados, o + 0xC + i * 16) for i in range(n)]
    sfnt = o + 0xC + n * 16
    assert dados[sfnt:sfnt + 4] == b"SFNT"
    nomes = sfnt + 8
    vistos = {}
    for _hash, attr, ini, fim in nos:
        if attr & 0x01000000:
            p = nomes + (attr & 0xFFFF) * 4
            nome = dados[p:dados.index(b"\0", p)].decode("utf-8")
        else:
            nome = "%08x" % _hash
        # (o mesmo nome pode aparecer varias vezes: as copias ganham .1, .2, ...)
        k = vistos.get(nome, 0)
        vistos[nome] = k + 1
        if k:
            base, ext = os.path.splitext(nome)
            nome = "%s.%d%s" % (base, k, ext)
        yield nome, dados[data_off + ini:data_off + fim]


def byml(dados):
    """BYML v2..v7 em objetos Python (dict, list, str, int, float, bool, None)."""
    e = "<" if dados[:2] == b"YB" else ">"
    _ver, chaves, textos, raiz = struct.unpack_from(e + "HIII", dados, 2)

    def tabela(off):
        if not off:
            return []
        n = struct.unpack_from(e + "I", dados, off)[0] & 0xFFFFFF
        offs = struct.unpack_from(e + "%dI" % n, dados, off + 4)
        return [dados[off + x:dados.index(b"\0", off + x)].decode("utf-8") for x in offs]

    k = tabela(chaves)
    t = tabela(textos)

    def valor(tipo, raw, at):
        if tipo == 0xA0:
            return t[raw]
        if tipo in (0xC0, 0xC1, 0xC2, 0x21):
            return no(raw)
        if tipo == 0xD0:
            return bool(raw)
        if tipo == 0xD1:
            return struct.unpack(e + "i", struct.pack(e + "I", raw))[0]
        if tipo == 0xD2:
            return struct.unpack(e + "f", struct.pack(e + "I", raw))[0]
        if tipo == 0xD3:
            return raw
        if tipo in (0xD4, 0xD5, 0xD6):
            fmt = {0xD4: "q", 0xD5: "Q", 0xD6: "d"}[tipo]
            return struct.unpack_from(e + fmt, dados, raw)[0]
        if tipo == 0xFF:
            return None
        if tipo == 0xA1:
            n, = struct.unpack_from(e + "I", dados, raw)
            return dados[raw + 4:raw + 4 + n].hex()
        return "<tipo %02x %08x>" % (tipo, raw)

    def no(off):
        tipo = dados[off]
        n = struct.unpack_from(e + "I", dados, off)[0] >> 8 if e == ">" else struct.unpack_from("<I", dados, off)[0] >> 8
        if tipo == 0xC1:
            r = {}
            for i in range(n):
                p = off + 4 + i * 8
                ki = struct.unpack_from(e + "I", dados, p)[0] & 0xFFFFFF
                ti = dados[p + 3]
                raw, = struct.unpack_from(e + "I", dados, p + 4)
                r[k[ki]] = valor(ti, raw, p + 4)
            return r
        if tipo == 0xC0:
            tipos = dados[off + 4:off + 4 + n]
            base = off + 4 + ((n + 3) & ~3)
            return [valor(tipos[i], struct.unpack_from(e + "I", dados, base + i * 4)[0], base + i * 4)
                    for i in range(n)]
        if tipo == 0x21:   # mapa por hash (v7)
            r = {}
            for i in range(n):
                p = off + 4 + i * 8
                h, raw = struct.unpack_from(e + "II", dados, p)
                r["%08x" % h] = raw
            return r
        return "<no %02x>" % tipo

    return no(raiz) if raiz else None


def msbt(dados):
    """[(rotulo, texto)] de um MSBT. Codigos de controle (0x0E..) aparecem como {grupo:tipo:hex}."""
    assert dados[:8] == b"MsgStdBn"
    e = "<" if dados[8:10] == bytes((0xFF, 0xFE)) else ">"
    utf16 = dados[0x0C] == 1
    nsec, = struct.unpack_from(e + "H", dados, 0x0E)
    o, secs = 0x20, {}
    for _ in range(nsec):
        mag = dados[o:o + 4]
        tam, = struct.unpack_from(e + "I", dados, o + 4)
        secs[mag] = (o + 0x10, tam)
        o = (o + 0x10 + tam + 15) & ~15
    rotulos = {}
    if b"LBL1" in secs:
        b, _t = secs[b"LBL1"]
        n, = struct.unpack_from(e + "I", dados, b)
        for i in range(n):
            q, off = struct.unpack_from(e + "II", dados, b + 4 + i * 8)
            p = b + off
            for _ in range(q):
                ln = dados[p]
                nome = dados[p + 1:p + 1 + ln].decode("utf-8")
                idx, = struct.unpack_from(e + "I", dados, p + 1 + ln)
                rotulos[idx] = nome
                p += 1 + ln + 4
    b, tam = secs[b"TXT2"]
    n, = struct.unpack_from(e + "I", dados, b)
    offs = list(struct.unpack_from(e + "%dI" % n, dados, b + 4)) + [tam]
    saida = []
    for i in range(n):
        bruto = dados[b + offs[i]:b + offs[i + 1]]
        txt = []
        if utf16:
            cs = list(struct.unpack(e + "%dH" % (len(bruto) // 2), bruto))
            j = 0
            while j < len(cs):
                c = cs[j]
                if c == 0x0E:
                    g, t, ps = cs[j + 1], cs[j + 2], cs[j + 3]
                    txt.append("{%d:%d:%s}" % (g, t, bruto[(j + 4) * 2:(j + 4) * 2 + ps].hex()))
                    j += 4 + ps // 2
                    continue
                if c == 0x0F:
                    txt.append("{/%d:%d}" % (cs[j + 1], cs[j + 2]))
                    j += 3
                    continue
                if c:
                    txt.append(chr(c))
                j += 1
        else:
            j = 0
            while j < len(bruto):
                c = bruto[j]
                if c == 0x0E:
                    g, t, ps = struct.unpack_from(e + "HHH", bruto, j + 1)
                    txt.append("{%d:%d:%s}" % (g, t, bruto[j + 7:j + 7 + ps].hex()))
                    j += 7 + ps
                    continue
                if c == 0x0F:
                    g, t = struct.unpack_from(e + "HH", bruto, j + 1)
                    txt.append("{/%d:%d}" % (g, t))
                    j += 5
                    continue
                k = j + 1
                while k < len(bruto) and bruto[k] not in (0x0E, 0x0F):
                    k += 1
                txt.append(bruto[j:k].rstrip(bytes(1)).decode("utf-8", "replace"))
                j = k
        saida.append((rotulos.get(i, "#%d" % i), "".join(txt)))
    return saida


def main(argv):
    if len(argv) < 4:
        print(__doc__)
        return 2
    cmd, rom, alvo = argv[1], Romfs(argv[2]), argv[3]
    dados = rom.ler(alvo)
    if cmd == "zs":
        with open(argv[4], "wb") as f:
            f.write(dados)
    elif cmd == "sarc":
        for nome, d in sarc_arquivos(dados):
            print("%9d  %s" % (len(d), nome))
            if len(argv) > 4:
                p = os.path.join(argv[4], nome)
                os.makedirs(os.path.dirname(p) or ".", exist_ok=True)
                with open(p, "wb") as f:
                    f.write(d)
    elif cmd == "byml":
        texto = json.dumps(byml(dados), ensure_ascii=False, indent=1)
        if len(argv) > 4:
            with open(argv[4], "w", encoding="utf-8") as f:
                f.write(texto)
        else:
            print(texto)
    elif cmd == "msbt":
        import re
        rx = re.compile(argv[4])
        for nome, d in sarc_arquivos(dados):
            if nome.endswith(".msbt") and rx.search(nome):
                print("== " + nome)
                for rot, txt in msbt(d):
                    print("  %s = %s" % (rot, txt.replace(chr(10), "\\n")))
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.stdout.reconfigure(encoding="utf-8", errors="replace")
    sys.exit(main(sys.argv))
