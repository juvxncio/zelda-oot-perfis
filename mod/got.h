/* Redireciona os imports de um módulo reescrevendo as entradas da GOT (depois de o rtld ligá-lo).
 * Adaptado de sloperina (src/ootgz/got.h, GPL-3.0). */
#ifndef GOT_H
#define GOT_H

#include <stdint.h>

/* Endereço da entrada da GOT de `module` para o símbolo importado `name` (achado pelo MOD0 e
 * pela seção dynamic), ou 0. */
uintptr_t *got_slot(uintptr_t module, const char *name);

/* Aponta a entrada para `replacement`. Antes, guarda em *next o que o substituto deve chamar:
 * o valor antigo da entrada, se já for código de outro módulo (a própria função, ou o
 * substituto de outro mod), ou senão `function`, a definição do import ligada a este módulo.
 * O main liga os imports de forma preguiçosa: até a primeira chamada do jogo, a entrada aponta
 * para o PLT do próprio main, que só funciona entrando pelo stub daquele import (x16 = a
 * entrada), então um substituto nunca deve chamá-lo.
 * Devolve 0, ou uma mensagem de erro (e a entrada fica como estava). */
const char *got_hook(uintptr_t module, const char *name, void *replacement, void *function,
                     void **next);

#endif
