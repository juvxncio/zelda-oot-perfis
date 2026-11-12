/* Ganchos em funções internas do main, por um patch IPS32 estático (tools/mkips.py) e ponteiros
 * escritos em tempo de execução. Mesma técnica do sloperina (src/host/mkips.c, GPL-3.0):
 *
 *   no ponto do gancho      b desvio_i                      (era: a instrução original)
 *   desvio_i (na caverna)   adrp x16, slot_i ; ldr x16, [x16, #lo]
 *                           cbz  x16, 1f     ; br  x16          (vai para o substituto)
 *                       1:  instrução original ; b ponto+4      (= a função original)
 *
 * Enquanto o slot está zerado o jogo segue igual. A caverna é o fim do .text (zeros até a
 * página do .rodata) e os slots ficam no fim do .bss, que nenhum código do jogo acessa
 * (docs/investigacao.md). O sloperina usa o começo de ambos; este mod usa o fim. */
#ifndef GANCHOS_H
#define GANCHOS_H

#include <stdbool.h>
#include <stdint.h>

#define GANCHOS_CAVERNA   0x3C46C00u   /* marcador (16 bytes) e depois os desvios */
#define GANCHOS_DESVIOS   (GANCHOS_CAVERNA + 0x40u)
#define GANCHOS_DESVIO_TAM 0x20u
#define GANCHOS_FIM       0x3C47000u
#define GANCHOS_SLOTS     0x4E9F000u   /* um ponteiro (8 bytes) por gancho */
#define GANCHOS_MARCADOR  "PERFISGC"
#define GANCHOS_VERSAO    1u

enum {
#define GANCHO(nome, offset, original) GANCHO_##nome,
#include "ganchos.def"
#undef GANCHO
  GANCHOS_TOTAL
};

/* Confere que o patch certo foi aplicado ao main: o marcador, a versão, a quantidade e o desvio
 * em cada ponto. Sem isso nenhum gancho é ligado. Devolve quantos pontos estão certos, ou -1
 * se o marcador não confere (patch ausente ou de outra versão). */
int ganchos_iniciar(uintptr_t main_base);

/* Liga o gancho: daqui em diante o jogo chama `substituto`. Devolve o endereço que o
 * substituto chama para executar a função original, ou 0 se o gancho não pôde ser ligado. */
void *gancho_ligar(int id, void *substituto);

/* Desliga: o jogo volta a usar só a função original. */
void gancho_desligar(int id);

#endif
