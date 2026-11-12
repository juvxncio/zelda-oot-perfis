/* Interceptação das funções de arquivo do jogo: perfis de save + registro no log. */
#ifndef DIAG_H
#define DIAG_H

#include <stdint.h>

/* Intercepta as funções nn::fs na GOT do main. Só lê memória e reescreve entradas da GOT:
 * pode rodar dentro do DT_INIT. Os perfis só são ligados se todas as funções forem
 * interceptadas; o perfil é lido quando o jogo monta o save (veja perfis.h). */
void diag_install(uintptr_t main_base);

#endif
