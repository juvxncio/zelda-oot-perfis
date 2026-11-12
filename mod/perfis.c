#include "perfis.h"

#define PREFIXO      PERFIS_MONTAGEM ":"     /* "save:" */
#define PREFIXO_LEN  (sizeof(PREFIXO) - 1)

static bool comeca_com(const char *s, const char *p)
{
  while (*p)
    if (*s++ != *p++)
      return false;
  return true;
}

static bool iguais(const char *a, const char *b)
{
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

/* True se o caminho está na montagem do save: "save:" seguido de '/' ou do fim. */
static bool no_save(const char *caminho)
{
  return comeca_com(caminho, PREFIXO) &&
         (caminho[PREFIXO_LEN] == '/' || caminho[PREFIXO_LEN] == 0);
}

/* Copia s para buf a partir de *len; false se não couber (com o zero final). */
static bool anexar(char *buf, size_t cap, size_t *len, const char *s)
{
  while (*s) {
    if (*len + 1 >= cap)
      return false;
    buf[(*len)++] = *s++;
  }
  buf[*len] = 0;
  return true;
}

bool perfis_pasta(int perfil, char *buf, size_t cap)
{
  if (perfil < 2 || perfil > PERFIS_MAX || !buf || cap == 0)
    return false;
  char n[2] = { (char)('0' + perfil), 0 };
  size_t len = 0;
  buf[0] = 0;
  return anexar(buf, cap, &len, PREFIXO "/perfil") && anexar(buf, cap, &len, n);
}

perfis_acao_t perfis_reescrever(int perfil, bool bloqueado, const char *caminho, char *buf,
                                size_t cap)
{
  if (!caminho || !no_save(caminho) || iguais(caminho, PERFIS_ARQUIVO))
    return PERFIS_ORIGINAL;
  if (bloqueado)
    return PERFIS_BLOQUEAR;
  if (perfil == 1)
    return PERFIS_ORIGINAL;
  if (cap > PERFIS_CAMINHO_MAX)
    cap = PERFIS_CAMINHO_MAX;
  if (!perfis_pasta(perfil, buf, cap))
    return PERFIS_BLOQUEAR;   /* perfil inválido: na dúvida, não grava em lugar nenhum */
  size_t len = 0;
  while (buf[len])
    len++;
  const char *resto = caminho + PREFIXO_LEN;   /* "/X" ou "" */
  if (!anexar(buf, cap, &len, resto))
    return PERFIS_BLOQUEAR;
  return PERFIS_REESCRITO;
}

int perfis_ler_numero(const char *texto, size_t tamanho)
{
  if (!texto)
    return 1;
  int numero = 0, digitos = 0;
  for (size_t i = 0; i < tamanho; i++) {
    char c = texto[i];
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n')
      continue;
    if (c < '0' || c > '9' || ++digitos > 1)
      return 1;
    numero = c - '0';
  }
  return (digitos == 1 && numero >= 1 && numero <= PERFIS_MAX) ? numero : 1;
}
