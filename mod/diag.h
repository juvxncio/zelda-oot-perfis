/* Diagnóstico: registra no log cada chamada de arquivo do jogo, sem mudar nada. */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>

/* Intercepta as funções nn::fs na GOT do main. Só lê memória e reescreve entradas da GOT:
 * pode rodar dentro do DT_INIT. */
void diag_install(uintptr_t main_base);

#endif
