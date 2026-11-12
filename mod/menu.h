/* O menu de perfis: só a lógica (o que mostrar, o que cada botão faz), sem chamadas ao
 * sistema, para ser testada no PC (tests/test_menu.c). Quem liga ao jogo é o menu_jogo.c. */
#ifndef MENU_H
#define MENU_H

#include <stdbool.h>
#include <stdint.h>

#include "perfis.h"
#include "tela.h"

/* O que o menu precisa saber, consultado quando ele abre. */
typedef struct {
  bool ligado;                       /* perfis funcionando (tudo interceptado, save montado) */
  int em_uso;                        /* o perfil carregado agora */
  int escolhido;                     /* o gravado em perfil_ativo.txt (o da próxima abertura) */
  bool tem_save[PERFIS_MAX + 1];     /* [1..PERFIS_MAX] */
} menu_info_t;

typedef enum { MENU_FECHADO, MENU_LISTA, MENU_CONFIRMADO, MENU_ERRO } menu_estado_t;

typedef struct {
  menu_estado_t estado;
  int cursor;   /* 1..PERFIS_MAX */
  int gravado;  /* o perfil gravado na última confirmação */
} menu_t;

typedef enum {
  MENU_NADA,       /* só redesenhar (ou nada) */
  MENU_GRAVAR,     /* gravar o perfil m->cursor; depois chamar menu_resultado */
  MENU_FECHOU,     /* esconder o painel e devolver o controle ao jogo */
} menu_acao_t;

/* Abre na lista, com o cursor no perfil escolhido. */
void menu_abrir(menu_t *m, const menu_info_t *info);

/* Botões que acabaram de descer (bits de input.h). */
menu_acao_t menu_botoes(menu_t *m, uint32_t apertados);

/* Depois de MENU_GRAVAR: se gravou ou não. */
void menu_resultado(menu_t *m, bool gravou);

/* O texto do painel para o estado atual. */
void menu_montar(const menu_t *m, const menu_info_t *info, tela_conteudo_t *out);

#endif
