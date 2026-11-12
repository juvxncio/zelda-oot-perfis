/* A linha "Perfil de save" na tela de Opções do jogo (em construção). */
#ifndef OPCOES_H
#define OPCOES_H

#include <stdbool.h>

/* Depois de ganchos_iniciar. Sem o patch IPS, não faz nada. */
void opcoes_install(void);

/* True se mudar o perfil agora faz o jogo reiniciar ao fechar as Opções (nenhum jogo carregado
 * e o gancho de fechar a tela ligado); senão a troca vale ao reabrir o jogo. */
bool opcoes_reinicia_ao_sair(void);

#endif
