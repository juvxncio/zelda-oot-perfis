#include "opcoes.h"

#include <stdbool.h>
#include <stdint.h>

#include "diag.h"
#include "ganchos.h"
#include "perfis.h"
#include "sys.h"

/* A lista de OptionSetTable.bgyml, já lida pelo jogo (docs/investigacao.md):
 * {int32 n; entrada *e}, entradas de 40 bytes:
 *   +0  tipo da opção (enum de 18 valores)   +4  tipo do botão
 *   +16 quantas escolhas                     +24 as escolhas
 *   +32 hash do nome (membro de OptionData)  +36 flag de visibilidade (0 = sempre)
 * A linha nova é uma cópia da de "Idioma das vozes" (9 escolhas, botão Select) com o tipo
 * MessageWindowTransparency, que existe no enum mas não em nenhuma linha nem em OptionData:
 * nada que o jogo grave tem esse nome, então a linha não mexe em configuração nenhuma. */
#define ENTRADA_TAM          40
#define TIPO_TRANSPARENCIA   9u
#define TIPO_IDIOMA_VOZ      10u
#define HASH_TRANSPARENCIA   0x916ac217u   /* MurmurHash3("MessageWindowTransparency") */
#define HASH_IDIOMA_VOZ      0x74b1edd9u   /* MurmurHash3("VoiceLanguage") */

/* Botão Select: +88 tipo da opção, +92 escolha atual. UISystemOptionScreen: +824 linhas. */
#define SELECT_TIPO          88
#define SELECT_ESCOLHA       92
#define OPCOES_N_LINHAS      824

typedef uint64_t (*fn8)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
#define ARGS8 uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7
#define REPASSA(f) (f)(a0, a1, a2, a3, a4, a5, a6, a7)

static fn8 criar_original, conta_original, pega_original, por_tipo_original, texto_original;

static uint64_t lista;                         /* a lista que recebeu a linha */
static _Alignas(8) uint8_t entrada[ENTRADA_TAM];
static bool pronta;
static uint64_t botao_atual;                  /* o botão Select da linha nesta tela */
static int32_t escolha_atual;

static uint32_t le32(uint64_t p) { return *(const volatile uint32_t *)p; }
static void gr32(uint64_t p, uint32_t v) { *(volatile uint32_t *)p = v; }

/* Monta a linha a partir da de "Idioma das vozes", conferindo que a lista é a esperada. */
static bool preparar(uint64_t l)
{
  if (pronta)
    return l == lista;
  if (!diag_perfis_ligados())
    return false;
  uint64_t voz = por_tipo_original(l, TIPO_IDIOMA_VOZ, 0, 0, 0, 0, 0, 0);
  if (!voz || !sys_mapped(voz, ENTRADA_TAM, SYS_PERM_R))
    return false;
  if (le32(voz) != TIPO_IDIOMA_VOZ || le32(voz + 32) != HASH_IDIOMA_VOZ || le32(voz + 16) < PERFIS_MAX) {
    log_raw("opcoes: a linha de Idioma das vozes nao e a esperada; nada foi adicionado");
    lista = l;   /* (não tenta de novo) */
    pronta = true;
    return false;
  }
  for (int i = 0; i < ENTRADA_TAM; i++)
    entrada[i] = ((const uint8_t *)voz)[i];
  gr32((uint64_t)entrada + 0, TIPO_TRANSPARENCIA);
  gr32((uint64_t)entrada + 16, PERFIS_MAX);
  gr32((uint64_t)entrada + 32, HASH_TRANSPARENCIA);
  gr32((uint64_t)entrada + 36, 0);
  lista = l;
  pronta = true;
  log_raw("opcoes: linha de perfil pronta");
  return true;
}

static bool nossa(uint64_t l) { return preparar(l) && le32((uint64_t)entrada) == TIPO_TRANSPARENCIA; }

static uint64_t conta(ARGS8)
{
  uint64_t n = REPASSA(conta_original);
  return nossa(a0) ? (uint32_t)n + 1 : n;
}

static uint64_t pega(ARGS8)
{
  if (nossa(a0) && (int32_t)a1 == (int32_t)conta_original(a0, 0, 0, 0, 0, 0, 0, 0))
    return (uint64_t)entrada;
  return REPASSA(pega_original);
}

static uint64_t por_tipo(ARGS8)
{
  if ((uint32_t)a1 == TIPO_TRANSPARENCIA && nossa(a0))
    return (uint64_t)entrada;
  return REPASSA(por_tipo_original);
}

static void log_escolha(const char *msg, int32_t escolha)
{
  log_line_t l;
  log_begin(&l);
  log_str(&l, "opcoes: ");
  log_str(&l, msg);
  log_str(&l, " Perfil ");
  log_dec(&l, escolha + 1);
  log_end(&l);
}

/* O jogo chama ao montar o botão e a cada mudança de escolha (setas). */
static uint64_t texto(ARGS8)
{
  uint64_t b = a0;
  if (pronta && le32(b + SELECT_TIPO) == TIPO_TRANSPARENCIA) {
    if (b != botao_atual) {
      /* botão novo (a tela abriu): mostra o perfil escolhido */
      int n = diag_perfil_escolhido();
      if (n < 1 || n > PERFIS_MAX)
        n = 1;
      botao_atual = b;
      escolha_atual = n - 1;
      gr32(b + SELECT_ESCOLHA, (uint32_t)escolha_atual);
      log_escolha("linha mostrada com", escolha_atual);
    } else {
      int32_t e = (int32_t)le32(b + SELECT_ESCOLHA);
      if (e >= 0 && e < PERFIS_MAX && e != escolha_atual) {
        escolha_atual = e;
        if (diag_gravar_perfil(e + 1))
          log_escolha("escolhido e gravado:", e);
        else
          log_escolha("FALHOU ao gravar", e);
      }
    }
  }
  return REPASSA(texto_original);
}

static uint64_t criar(ARGS8)
{
  botao_atual = 0;
  uint64_t r = REPASSA(criar_original);
  log_line_t l;
  log_begin(&l);
  log_str(&l, "opcoes: tela com ");
  log_dec(&l, *(const int32_t *)(a0 + OPCOES_N_LINHAS));
  log_str(&l, " linhas");
  log_end(&l);
  return r;
}

void opcoes_install(void)
{
  /* Os quatro da lista e do botão juntos, ou nenhum: a linha não pode existir pela metade. */
  fn8 c = (fn8)gancho_ligar(GANCHO_LISTA_CONTA, (void *)conta);
  conta_original = c;
  pega_original = (fn8)gancho_ligar(GANCHO_LISTA_PEGA, (void *)pega);
  por_tipo_original = (fn8)gancho_ligar(GANCHO_LISTA_POR_TIPO, (void *)por_tipo);
  texto_original = (fn8)gancho_ligar(GANCHO_SELECT_TEXTO, (void *)texto);
  if (!conta_original || !pega_original || !por_tipo_original || !texto_original) {
    gancho_desligar(GANCHO_LISTA_CONTA);
    gancho_desligar(GANCHO_LISTA_PEGA);
    gancho_desligar(GANCHO_LISTA_POR_TIPO);
    gancho_desligar(GANCHO_SELECT_TEXTO);
    log_raw("opcoes: ganchos da lista indisponiveis; a tela de Opcoes segue sem mudancas");
    return;
  }
  criar_original = (fn8)gancho_ligar(GANCHO_OPCOES_CRIAR, (void *)criar);
}
