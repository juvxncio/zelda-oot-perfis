#include "textos.h"

#include <stdbool.h>

#include "ganchos.h"
#include "opcoes.h"
#include "sys.h"

/* Saída da busca (inicializada pelo jogo em main+0xa66f70): +0 texto, +8 tamanho (int32),
 * +16 int64 (-1). Os argumentos de arquivo e rótulo começam com um char*. */
typedef uint64_t (*fn8)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);
#define ARGS8 uint64_t a0, uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7

static fn8 pegar_original;
static volatile int formato;   /* textos_formato_t, aprendido das buscas do próprio jogo */
static int avisos;

/* O idioma do jogo: o do Mals/<XXyy> que ele abriu; senão, o do sistema (nn::oe). */
static volatile int idioma = TEXTOS_IDIOMA_NENHUM;
uint64_t oe_get_desired_language(void) __asm__("_ZN2nn2oe18GetDesiredLanguageEv");

static void log_idioma(const char *de_onde_veio, int i)
{
  log_line_t l;
  log_begin(&l);
  log_str(&l, "textos: idioma ");
  log_dec(&l, i);
  log_str(&l, de_onde_veio);
  log_end(&l);
}

void textos_arquivo_aberto(const char *caminho)
{
  if (idioma != TEXTOS_IDIOMA_NENHUM)
    return;
  textos_idioma_t i = textos_idioma_de_caminho(caminho);
  if (i != TEXTOS_IDIOMA_NENHUM) {
    idioma = i;
    log_idioma(" (pelo arquivo de textos que o jogo abriu)", i);
  }
}

static textos_idioma_t idioma_atual(void)
{
  int i = idioma;
  if (i != TEXTOS_IDIOMA_NENHUM)
    return (textos_idioma_t)i;
  uint64_t c = oe_get_desired_language();   /* nn::settings::LanguageCode: 8 bytes de texto */
  char s[9];
  for (int k = 0; k < 8; k++)
    s[k] = (char)(c >> (8 * k));
  s[8] = 0;
  i = textos_idioma_de_codigo(s);
  if (i == TEXTOS_IDIOMA_NENHUM)
    i = TEXTOS_EN;
  idioma = i;
  log_idioma(" (pelo idioma do sistema)", i);
  return (textos_idioma_t)i;
}

#define TEXTOS_MAX 16           /* Title, as 2 legendas e 00..08 (o idioma não muda) */
#define TEXTO_CAP  128
static uint16_t em_utf16[TEXTOS_MAX][TEXTO_CAP];
static const char *de_onde[TEXTOS_MAX];

static size_t strlen8(const char *s)
{
  size_t n = 0;
  while (s[n])
    n++;
  return n;
}

/* O mesmo texto em UTF-16, guardado (estático: o jogo pode guardar o ponteiro). */
static const uint16_t *utf16_de(const char *s, size_t *n)
{
  for (int i = 0; i < TEXTOS_MAX; i++) {
    if (de_onde[i] == s) {
      *n = 0;
      while (em_utf16[i][*n])
        (*n)++;
      return em_utf16[i];
    }
    if (!de_onde[i]) {
      *n = textos_utf16(s, em_utf16[i], TEXTO_CAP);
      de_onde[i] = s;
      return em_utf16[i];
    }
  }
  return 0;
}

static void aprender(uint64_t saida)
{
  const uint8_t *p = *(const uint8_t *const *)saida;
  int32_t n = *(const int32_t *)(saida + 8);
  if (!p || !sys_mapped((uintptr_t)p, 4, SYS_PERM_R))
    return;
  /* lê até o fim da região mapeada, no máximo 2 KiB */
  sys_meminfo_t mi;
  sys_query_memory(&mi, (uintptr_t)p);
  size_t max = (size_t)(mi.addr + mi.size - (uintptr_t)p);
  if (max > 2048)
    max = 2048;
  textos_formato_t f = textos_deduzir_formato(p, n, max);
  if (f == TEXTOS_FORMATO_DESCONHECIDO)
    return;
  formato = f;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "textos: formato das mensagens = ");
  log_dec(&l, f);
  log_str(&l, " (tamanho ");
  log_dec(&l, n);
  log_str(&l, ")");
  log_end(&l);
}

static uint64_t pegar(ARGS8)
{
  const char *rotulo = 0;
  if (a2 && sys_mapped(a2, 8, SYS_PERM_R))
    rotulo = *(const char *const *)a2;
  const char *nosso = (rotulo && sys_mapped((uintptr_t)rotulo, 1, SYS_PERM_R)) ? textos_para_rotulo(rotulo, opcoes_reinicia_ao_sair(), idioma_atual()) : 0;
  if (!nosso) {
    uint64_t r = pegar_original(a0, a1, a2, a3, a4, a5, a6, a7);
    if ((r & 0xFF) && formato == TEXTOS_FORMATO_DESCONHECIDO)
      aprender(a0);
    return r;
  }
  int f = formato;
  size_t n;
  switch (f) {
    case TEXTOS_FORMATO_U8:
    case TEXTOS_FORMATO_U8_ZERO:
      n = strlen8(nosso);
      *(const char **)a0 = nosso;
      *(int32_t *)(a0 + 8) = (int32_t)(f == TEXTOS_FORMATO_U8_ZERO ? n + 1 : n);
      return 1;
    case TEXTOS_FORMATO_U16:
    case TEXTOS_FORMATO_U16_ZERO:
    case TEXTOS_FORMATO_U16_BYTES: {
      const uint16_t *t = utf16_de(nosso, &n);
      if (!t)
        break;
      *(const uint16_t **)a0 = t;
      *(int32_t *)(a0 + 8) = (int32_t)(f == TEXTOS_FORMATO_U16 ? n : f == TEXTOS_FORMATO_U16_ZERO ? n + 1 : n * 2);
      return 1;
    }
    default:
      break;
  }
  if (avisos < 5) {
    avisos++;
    log_raw("textos: formato ainda desconhecido; o texto fica como o jogo mostrar");
  }
  return pegar_original(a0, a1, a2, a3, a4, a5, a6, a7);
}

void textos_install(void)
{
  pegar_original = (fn8)gancho_ligar(GANCHO_MSG_PEGAR, (void *)pegar);
  if (!pegar_original)
    log_raw("textos: gancho indisponivel; a linha de perfil fica sem texto");
}
