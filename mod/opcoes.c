#include "opcoes.h"

#include "ganchos.h"
#include "sys.h"

/* UISystemOptionScreen (docs/investigacao.md): a ordem das linhas fica em +824 (quantidade) e
 * +832 (entradas {tipo, índice}). */
#define OPCOES_N_LINHAS 824

typedef uint64_t (*criar_fn)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t,
                             uint64_t);
static criar_fn criar_original;

/* Só de teste (passo 1): registra quando a tela de Opções cria as linhas, e quantas são. */
static uint64_t criar(uint64_t self, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5,
                      uint64_t a6, uint64_t a7)
{
  log_raw("opcoes: criando as linhas");
  uint64_t r = criar_original(self, a1, a2, a3, a4, a5, a6, a7);
  log_line_t l;
  log_begin(&l);
  log_str(&l, "opcoes: tela ");
  log_hex(&l, self);
  log_str(&l, " com ");
  log_dec(&l, *(const int32_t *)(self + OPCOES_N_LINHAS));
  log_str(&l, " linhas");
  log_end(&l);
  return r;
}

void opcoes_install(void)
{
  criar_original = (criar_fn)gancho_ligar(GANCHO_OPCOES_CRIAR, (void *)criar);
  if (!criar_original)
    log_raw("opcoes: gancho indisponivel; a tela de Opcoes segue sem mudancas");
}
