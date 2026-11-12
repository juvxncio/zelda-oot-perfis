/* Um painel de texto desenhado por cima da imagem do jogo, pelo NVN do próprio jogo.
 *
 * O painel é desenhado na CPU (fundo escuro, texto 8x8) num buffer no formato da tela e, antes
 * de cada quadro ser apresentado, copiado para a textura que vai para a tela. Método adaptado de
 * sloperina (src/ootgz/nvn.c, GPL-3.0), só com a parte opaca e de um painel. */
#ifndef TELA_H
#define TELA_H

#include <stdbool.h>
#include <stdint.h>

#define TELA_COLUNAS 36
#define TELA_LINHAS  13   /* título, linha vazia, 9 perfis, linha vazia, ajuda */

enum { TELA_NORMAL, TELA_SELECIONADA, TELA_TITULO, TELA_APAGADA, TELA_AVISO, TELA_CORES };

typedef struct {
  char texto[TELA_LINHAS][TELA_COLUNAS + 1];   /* ASCII; o que passar de TELA_COLUNAS é cortado */
  uint8_t cor[TELA_LINHAS];                    /* TELA_NORMAL ... */
  int linhas;                                  /* quantas linhas usar (1 a TELA_LINHAS) */
} tela_conteudo_t;

/* Mostra (ou troca) o painel. Pode ser chamada de qualquer thread; copia o conteúdo. */
void tela_mostrar(const tela_conteudo_t *conteudo);
void tela_esconder(void);
bool tela_visivel(void);

/* Aponta o nvnBootstrapLoader do main para o nosso (no DT_INIT). Devolve 0 ou uma mensagem. */
const char *tela_install(uintptr_t main_base);

#endif
