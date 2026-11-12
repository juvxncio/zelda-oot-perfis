/* Parte pura de textos.h: sem nada do Switch, compilada também nos testes do PC. */
#include <stdbool.h>

#include "textos.h"

#define PREFIXO "MessageWindowTransparency_"

static const char *const escolhas[] = {
  "Perfil 1", "Perfil 2", "Perfil 3", "Perfil 4", "Perfil 5",
  "Perfil 6", "Perfil 7", "Perfil 8", "Perfil 9",
};

/* (sem strcmp: o módulo não importa a libc) */
static bool comeca(const char *s, const char *p)
{
  while (*p)
    if (*s++ != *p++)
      return false;
  return true;
}

static bool igual(const char *a, const char *b)
{
  while (*a && *a == *b)
    a++, b++;
  return *a == *b;
}

const char *textos_para_rotulo(const char *rotulo)
{
  if (!rotulo || !comeca(rotulo, PREFIXO))
    return 0;
  const char *s = rotulo + sizeof(PREFIXO) - 1;
  if (igual(s, "Title"))
    return "Perfil de save";
  if (igual(s, "Caption"))
    return "Escolhe qual save o jogo vai usar.\n"
           "A troca vale quando o jogo for aberto de novo.";
  if (s[0] == '0' && s[1] >= '0' && s[1] <= '8' && s[2] == 0)
    return escolhas[s[1] - '0'];
  return 0;
}

textos_formato_t textos_deduzir_formato(const uint8_t *p, int32_t tamanho, size_t max)
{
  if (!p || tamanho <= 0 || max < 4)
    return TEXTOS_FORMATO_DESCONHECIDO;
  /* Só decide com dois caracteres ASCII no começo: em UTF-16 são "x\0y\0", em UTF-8 "xy".
   * (Um texto de uma letra só, ou que começa com código de controle, não decide nada.) */
  bool ascii0 = p[0] >= 0x20 && p[0] < 0x7F;
  if (!ascii0)
    return TEXTOS_FORMATO_DESCONHECIDO;
  bool u16 = p[1] == 0 && p[2] >= 0x20 && p[2] < 0x7F && p[3] == 0;
  if (!u16 && !(p[1] >= 0x20 && p[1] < 0x7F))
    return TEXTOS_FORMATO_DESCONHECIDO;
  if (u16) {
    size_t n = 0;
    while ((n + 1) * 2 <= max && (p[n * 2] | p[n * 2 + 1]))
      n++;
    if ((n + 1) * 2 > max)
      return TEXTOS_FORMATO_DESCONHECIDO;
    if ((size_t)tamanho == n)
      return TEXTOS_FORMATO_U16;
    if ((size_t)tamanho == n + 1)
      return TEXTOS_FORMATO_U16_ZERO;
    if ((size_t)tamanho == n * 2)
      return TEXTOS_FORMATO_U16_BYTES;
    return TEXTOS_FORMATO_DESCONHECIDO;
  }
  size_t n = 0;
  while (n < max && p[n])
    n++;
  if (n >= max)
    return TEXTOS_FORMATO_DESCONHECIDO;
  if ((size_t)tamanho == n)
    return TEXTOS_FORMATO_U8;
  if ((size_t)tamanho == n + 1)
    return TEXTOS_FORMATO_U8_ZERO;
  return TEXTOS_FORMATO_DESCONHECIDO;
}

size_t textos_utf16(const char *s, uint16_t *saida, size_t cap)
{
  const uint8_t *u = (const uint8_t *)s;
  size_t n = 0;
  while (*u && n + 1 < cap) {
    uint32_t c = *u++;
    if (c >= 0xE0 && u[0] && u[1]) {
      c = ((c & 0x0F) << 12) | ((uint32_t)(u[0] & 0x3F) << 6) | (u[1] & 0x3F);
      u += 2;
    } else if (c >= 0xC0 && u[0]) {
      c = ((c & 0x1F) << 6) | (u[0] & 0x3F);
      u += 1;
    } else if (c >= 0x80) {
      c = '?';
    }
    saida[n++] = (uint16_t)c;
  }
  if (cap)
    saida[n] = 0;
  return n;
}
