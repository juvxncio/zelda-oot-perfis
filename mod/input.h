/* Botões do controle, lidos de dentro das chamadas nn::hid::GetNpadStates do jogo.
 * Adaptado em parte de sloperina (src/ootgz/input.c, GPL-3.0). */
#ifndef INPUT_H
#define INPUT_H

#include <stdbool.h>
#include <stdint.h>

/* bits de nn::hid::NpadButton */
enum {
  BTN_A = 1u << 0, BTN_B = 1u << 1, BTN_X = 1u << 2, BTN_Y = 1u << 3,
  BTN_LSTICK = 1u << 4, BTN_RSTICK = 1u << 5, BTN_L = 1u << 6, BTN_R = 1u << 7,
  BTN_ZL = 1u << 8, BTN_ZR = 1u << 9, BTN_PLUS = 1u << 10, BTN_MINUS = 1u << 11,
  BTN_DLEFT = 1u << 12, BTN_DUP = 1u << 13, BTN_DRIGHT = 1u << 14, BTN_DDOWN = 1u << 15,
};

/* A combinação que abre o menu de perfis: L + R + direcional para baixo. */
#define INPUT_COMBO_MENU (BTN_L | BTN_R | BTN_DDOWN)

/* Chamada a cada vez que a combinação do menu é apertada (todas as teclas seguradas, e a
 * última acabou de descer). Roda na thread do jogo que leu o controle. */
typedef void (*input_combo_fn)(void);

/* Aponta as 5 variantes de nn::hid::GetNpadStates do main para os nossos substitutos, que
 * chamam a original e só leem o resultado. Devolve quantas foram interceptadas. */
int input_install(uintptr_t main_base, input_combo_fn on_combo);

#endif
