#include "ganchos.h"

#include "sys.h"

static uintptr_t base;
static bool ponto_ok[GANCHOS_TOTAL > 0 ? GANCHOS_TOTAL : 1];

static const uint32_t pontos[] = {
#define GANCHO(nome, offset, original) offset,
#include "ganchos.def"
#undef GANCHO
};

static const uint32_t originais[] = {
#define GANCHO(nome, offset, original) original,
#include "ganchos.def"
#undef GANCHO
};

/* As mesmas codificações de tools/mkips.py. */
static uint32_t adrp_x16(uint32_t de, uint32_t para)
{
  int32_t pag = (int32_t)((para & ~0xFFFu) - (de & ~0xFFFu)) >> 12;
  return 0x90000000u | ((uint32_t)pag & 3u) << 29 | (((uint32_t)pag >> 2) & 0x7FFFFu) << 5 | 16u;
}

/* O desvio i está inteiro como o mkips.py gerou? O emulador aplica patches próprios ao main
 * (o Fritchii tem os "built-in": resolução, FPS, água) e um deles poderia cair na caverna. */
static bool desvio_intacto(uintptr_t main_base, int i, uint32_t desvio)
{
  const uint32_t *d = (const uint32_t *)(main_base + desvio);
  uint32_t slot = GANCHOS_SLOTS + (uint32_t)i * 8;
  uint32_t esperado[6] = {
    adrp_x16(desvio, slot),
    0xF9400000u | ((slot & 0xFFFu) / 8) << 10 | 16u << 5 | 16u,   /* ldr x16, [x16, #lo] */
    0xB4000000u | (2u << 5) | 16u,                                   /* cbz x16, +8 */
    0xD61F0200u,                                                      /* br x16 */
    originais[i],
    0,
  };
  esperado[5] = 0x14000000u | ((uint32_t)((int32_t)(pontos[i] + 4 - (desvio + 20)) >> 2) & 0x3FFFFFFu);
  if (!sys_mapped((uintptr_t)d, sizeof(esperado), SYS_PERM_R))
    return false;
  for (int k = 0; k < 6; k++)
    if (d[k] != esperado[k])
      return false;
  return true;
}

_Static_assert(GANCHOS_DESVIOS + GANCHOS_TOTAL * GANCHOS_DESVIO_TAM <= GANCHOS_FIM,
               "a caverna nao comporta tantos ganchos");

static uint32_t b_para(uint32_t de, uint32_t para)
{
  return 0x14000000u | ((uint32_t)((int32_t)(para - de) >> 2) & 0x3FFFFFFu);
}

static void log_gancho(const char *msg, int id)
{
  log_line_t l;
  log_begin(&l);
  log_str(&l, "ganchos: ");
  log_str(&l, msg);
  log_str(&l, " ");
  log_dec(&l, id);
  log_str(&l, " (main+");
  log_hex(&l, id >= 0 && id < GANCHOS_TOTAL ? pontos[id] : 0);
  log_str(&l, ")");
  log_end(&l);
}

int ganchos_iniciar(uintptr_t main_base)
{
  base = 0;
  if (!sys_mapped(main_base + GANCHOS_CAVERNA, 16, SYS_PERM_R) ||
      !sys_mapped(main_base + GANCHOS_SLOTS, GANCHOS_TOTAL * 8, SYS_PERM_R | SYS_PERM_W)) {
    log_raw("ganchos: caverna ou slots fora do mapa: nenhum gancho");
    return -1;
  }
  const char *m = (const char *)(main_base + GANCHOS_CAVERNA);
  static const char esperado[] = GANCHOS_MARCADOR;
  _Static_assert(sizeof(esperado) == 9, "o marcador tem 8 letras");
  for (int i = 0; i < 8; i++) {
    if (m[i] != esperado[i]) {
      log_raw("ganchos: patch IPS ausente (marcador nao confere): nenhum gancho");
      return -1;
    }
  }
  const uint32_t *v = (const uint32_t *)(m + 8);
  if (v[0] != GANCHOS_VERSAO || v[1] != GANCHOS_TOTAL) {
    log_raw("ganchos: patch IPS de outra versao do mod: nenhum gancho");
    return -1;
  }
  int ok = 0;
  for (int i = 0; i < GANCHOS_TOTAL; i++) {
    uint32_t desvio = GANCHOS_DESVIOS + (uint32_t)i * GANCHOS_DESVIO_TAM;
    const uint32_t *ponto = (const uint32_t *)(main_base + pontos[i]);
    bool no_ponto = sys_mapped((uintptr_t)ponto, 4, SYS_PERM_R) && *ponto == b_para(pontos[i], desvio);
    bool intacto = desvio_intacto(main_base, i, desvio);
    ponto_ok[i] = no_ponto && intacto;
    if (ponto_ok[i])
      ok++;
    else
      log_gancho(no_ponto ? "desvio alterado na caverna" : "ponto sem o desvio", i);
  }
  base = main_base;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "ganchos: patch IPS ok, ");
  log_dec(&l, ok);
  log_str(&l, " de ");
  log_dec(&l, GANCHOS_TOTAL);
  log_str(&l, " pontos");
  log_end(&l);
  return ok;
}

void *gancho_ligar(int id, void *substituto)
{
  if (!base || id < 0 || id >= GANCHOS_TOTAL || !ponto_ok[id] || !substituto)
    return 0;
  uintptr_t *slot = (uintptr_t *)(base + GANCHOS_SLOTS) + id;
  __atomic_store_n(slot, (uintptr_t)substituto, __ATOMIC_RELEASE);
  log_gancho("ligado", id);
  return (void *)(base + GANCHOS_DESVIOS + (uintptr_t)id * GANCHOS_DESVIO_TAM + 16);
}

void gancho_desligar(int id)
{
  if (!base || id < 0 || id >= GANCHOS_TOTAL)
    return;
  uintptr_t *slot = (uintptr_t *)(base + GANCHOS_SLOTS) + id;
  __atomic_store_n(slot, 0, __ATOMIC_RELEASE);
  log_gancho("desligado", id);
}
