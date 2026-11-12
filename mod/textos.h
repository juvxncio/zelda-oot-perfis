/* Textos da linha de perfil na tela de Opções: respondidos pelo mod na busca de mensagem do
 * jogo, sem alterar os arquivos de texto do romfs. */
#ifndef TEXTOS_H
#define TEXTOS_H

#include <stddef.h>
#include <stdint.h>

/* Depois de ganchos_iniciar. */
void textos_install(void);

/* Lógica pura (testada no PC). O texto do mod para um rótulo de "MessageWindowTransparency_*",
 * em UTF-8, ou 0 se o rótulo não é nosso. `reinicia`: se mudar o perfil agora reinicia o jogo
 * ao fechar as Opções (muda só a legenda). */
#include <stdbool.h>
const char *textos_para_rotulo(const char *rotulo, bool reinicia);

/* Como o jogo representa o tamanho do texto, aprendido de uma busca bem-sucedida. */
typedef enum {
  TEXTOS_FORMATO_DESCONHECIDO = 0,
  TEXTOS_FORMATO_U8,          /* UTF-8, tamanho em bytes sem o zero */
  TEXTOS_FORMATO_U8_ZERO,     /* UTF-8, tamanho em bytes com o zero */
  TEXTOS_FORMATO_U16,         /* UTF-16, tamanho em unidades sem o zero */
  TEXTOS_FORMATO_U16_ZERO,    /* UTF-16, tamanho em unidades com o zero */
  TEXTOS_FORMATO_U16_BYTES,   /* UTF-16, tamanho em bytes sem o zero */
} textos_formato_t;

/* Deduz o formato de um texto devolvido pelo jogo (ponteiro, tamanho, até `max` bytes legíveis). */
textos_formato_t textos_deduzir_formato(const uint8_t *p, int32_t tamanho, size_t max);

/* Converte UTF-8 para UTF-16LE (só até U+FFFF). Devolve unidades escritas, sem o zero final
 * (que é escrito se couber). */
size_t textos_utf16(const char *s, uint16_t *saida, size_t cap);

#endif
