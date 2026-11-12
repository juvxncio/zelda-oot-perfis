/* Interceptação das funções de arquivo do jogo: perfis de save + registro no log. */
#ifndef DIAG_H
#define DIAG_H

#include <stdbool.h>
#include <stdint.h>

/* Intercepta as funções nn::fs na GOT do main. Só lê memória e reescreve entradas da GOT:
 * pode rodar dentro do DT_INIT. Os perfis só são ligados se todas as funções forem
 * interceptadas; o perfil é lido quando o jogo monta o save (veja perfis.h). */
void diag_install(uintptr_t main_base);

/* Para o menu. Só funcionam depois que o jogo montou o save. */
bool diag_perfis_ligados(void);   /* false: perfis desligados (nem tudo foi interceptado) */
int diag_perfil_em_uso(void);     /* o perfil carregado quando o jogo montou o save */
int diag_perfil_escolhido(void);  /* o que está gravado em perfil_ativo.txt agora */
/* True se o perfil n (1 a 9) tem algum save (algum slot_NN/caption.sav). */
bool diag_perfil_tem_save(int n);
/* Grava n em perfil_ativo.txt e faz Commit do save. Vale na próxima vez que o jogo abrir. */
bool diag_gravar_perfil(int n);

/* True depois que o jogo abriu algum progress.sav (Continuar, ou o salvamento de um jogo
 * novo): daí em diante reiniciar perderia progresso. */
bool diag_jogo_carregado(void);
/* Milissegundos desde o último acesso do jogo a arquivo (abrir, criar, Commit). */
uint64_t diag_ms_desde_atividade(void);

#endif
