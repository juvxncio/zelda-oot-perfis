/* Liga o menu de perfis (menu.c) ao jogo: controle, tela e arquivo de controle. */
#ifndef MENU_JOGO_H
#define MENU_JOGO_H

#include <stdint.h>

/* Para input_install: a combinação abre/fecha; os botões navegam enquanto aberto. */
void menu_jogo_combo(void);
void menu_jogo_botoes(uint32_t apertados);

#endif
