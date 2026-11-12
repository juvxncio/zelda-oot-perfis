/* Módulo zelda-oot-perfis: carregado pelo rtld do jogo como subsdk9.
 *
 * O rtld chama modulo_init (DT_INIT) antes de o jogo começar. Aqui só se lê memória e se
 * escreve no log: nenhuma chamada ao sdk, que ainda não foi inicializado. */
#include <stdint.h>

#include "diag.h"
#include "input.h"
#include "sys.h"
#include "tela.h"

#define VERSAO "0.4.0-tela-teste"

/* Exportado pelo main do jogo; o rtld resolve este import. */
extern void nnMain(void);

/* Build E0F3CCFA (1.1.1, update v131072): offsets conferidos com tools/analisar_main.py. */
#define NNMAIN_OFFSET 0x1202CD0u
#define MOD0_OFFSET   0x3C4705Cu
#define MOD0_MAGIC    0x30444F4Du /* "MOD0" */

uintptr_t modulo_main_base;

/* Confere que o main carregado é o build esperado, pelo offset do MOD0. */
static uintptr_t detectar_main(void)
{
  uintptr_t base = (uintptr_t)&nnMain - NNMAIN_OFFSET;
  if (!sys_mapped(base, 8, SYS_PERM_R))
    return 0;
  if (*(const uint32_t *)(base + 4) != MOD0_OFFSET)
    return 0;
  if (!sys_mapped(base + MOD0_OFFSET, 4, SYS_PERM_R) ||
      *(const uint32_t *)(base + MOD0_OFFSET) != MOD0_MAGIC)
    return 0;
  return base;
}

static void copiar(char *dst, const char *src)
{
  int i = 0;
  for (; src[i] && i < TELA_COLUNAS; i++)
    dst[i] = src[i];
  dst[i] = 0;
}

/* Teste do desenho: a combinação liga e desliga um painel fixo. */
static void alternar_painel_teste(void)
{
  if (tela_visivel()) {
    tela_esconder();
    return;
  }
  static const char *const linhas[] = {
    "zelda-oot-perfis " VERSAO,
    "",
    "Teste de desenho na tela",
    "> linha selecionada (verde)",
    "  linha normal (branca)",
    "  linha apagada (cinza)",
    "  aviso (amarelo)",
    "",
    "L+R+baixo de novo: fechar",
  };
  static const uint8_t cores[] = { TELA_TITULO, TELA_NORMAL, TELA_NORMAL, TELA_SELECIONADA,
                                   TELA_NORMAL, TELA_APAGADA, TELA_AVISO, TELA_NORMAL,
                                   TELA_APAGADA };
  tela_conteudo_t c;
  for (int i = 0; i < TELA_LINHAS; i++) {
    copiar(c.texto[i], linhas[i]);
    c.cor[i] = cores[i];
  }
  c.linhas = TELA_LINHAS;
  tela_mostrar(&c);
}

__attribute__((used)) void modulo_init(void)
{
  log_raw("zelda-oot-perfis " VERSAO " carregado");
  modulo_main_base = detectar_main();
  if (!modulo_main_base) {
    log_raw("versao do jogo nao suportada (esperado build E0F3CCFA, 1.1.1): nada foi alterado");
    return;
  }
  log_line_t l;
  log_begin(&l);
  log_str(&l, "main em ");
  log_hex(&l, modulo_main_base);
  log_end(&l);
  diag_install(modulo_main_base);
  /* Independentes dos perfis: se o controle ou a tela falharem, os perfis seguem funcionando. */
  tela_install(modulo_main_base);
  input_install(modulo_main_base, alternar_painel_teste, 0);
}
