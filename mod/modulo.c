/* Módulo zelda-oot-perfis: carregado pelo rtld do jogo como subsdk9.
 *
 * O rtld chama modulo_init (DT_INIT) antes de o jogo começar. Aqui só se lê memória e se
 * escreve no log: nenhuma chamada ao sdk, que ainda não foi inicializado. */
#include <stdint.h>

#include "diag.h"
#include "input.h"
#include "sys.h"

#define VERSAO "0.3.2-input-teste"

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
  /* Independente dos perfis: se o controle não puder ser lido, os perfis seguem funcionando. */
  input_install(modulo_main_base, 0);
}
