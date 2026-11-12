/* Testes da lógica de textos (mod/textos_logica.c), no PC: make test */
#include <stdio.h>
#include <string.h>

#include "../mod/textos.h"

static int falhas, total;

#define CHECA(cond, ...)                                   \
  do {                                                     \
    total++;                                               \
    if (!(cond)) {                                         \
      falhas++;                                            \
      printf("FALHOU (linha %d): ", __LINE__);             \
      printf(__VA_ARGS__);                                 \
      printf("\n");                                        \
    }                                                      \
  } while (0)

int main(void)
{
  /* Rótulos */
  CHECA(textos_para_rotulo("MessageWindowTransparency_Title") &&
        strcmp(textos_para_rotulo("MessageWindowTransparency_Title"), "Perfil de save") == 0, "titulo");
  CHECA(textos_para_rotulo("MessageWindowTransparency_Caption") != 0, "legenda");
  for (int i = 0; i < 9; i++) {
    char r[64], esperado[16];
    snprintf(r, sizeof r, "MessageWindowTransparency_%02d", i);
    snprintf(esperado, sizeof esperado, "Perfil %d", i + 1);
    const char *t = textos_para_rotulo(r);
    CHECA(t && strcmp(t, esperado) == 0, "%s -> %s", r, t ? t : "(nada)");
  }
  CHECA(!textos_para_rotulo("MessageWindowTransparency_09"), "09 nao existe");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_0"), "0 sozinho");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_000"), "000");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_"), "so o prefixo");
  CHECA(!textos_para_rotulo("MessageWindowTransparency"), "sem _");
  CHECA(!textos_para_rotulo("VoiceLanguage_Title"), "rotulo do jogo");
  CHECA(!textos_para_rotulo("ControllerRumble_00"), "escolha do jogo");
  CHECA(!textos_para_rotulo(""), "vazio");
  CHECA(!textos_para_rotulo(0), "nulo");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_TitleX"), "titulo com sobra");

  /* Formato: UTF-8 */
  const char *u8 = "Idioma das vozes";
  CHECA(textos_deduzir_formato((const uint8_t *)u8, 16, 17) == TEXTOS_FORMATO_U8, "u8");
  CHECA(textos_deduzir_formato((const uint8_t *)u8, 17, 17) == TEXTOS_FORMATO_U8_ZERO, "u8 com zero");
  CHECA(textos_deduzir_formato((const uint8_t *)u8, 9, 17) == TEXTOS_FORMATO_DESCONHECIDO, "u8 tamanho errado");
  CHECA(textos_deduzir_formato((const uint8_t *)u8, 16, 10) == TEXTOS_FORMATO_DESCONHECIDO, "u8 sem zero no alcance");
  const char *pt = "Portugu\xc3\xaas (Brasil)";   /* 19 bytes */
  CHECA(textos_deduzir_formato((const uint8_t *)pt, 19, 20) == TEXTOS_FORMATO_U8, "u8 com acento");

  /* Formato: UTF-16 */
  uint16_t u16[32];
  size_t n = textos_utf16("Portugu\xc3\xaas (Brasil)", u16, 32);
  CHECA(n == 18 && u16[7] == 0xEA && u16[18] == 0, "utf16 conversao: %zu", n);
  const uint8_t *b = (const uint8_t *)u16;
  CHECA(textos_deduzir_formato(b, 18, 64) == TEXTOS_FORMATO_U16, "u16");
  CHECA(textos_deduzir_formato(b, 19, 64) == TEXTOS_FORMATO_U16_ZERO, "u16 com zero");
  CHECA(textos_deduzir_formato(b, 36, 64) == TEXTOS_FORMATO_U16_BYTES, "u16 em bytes");
  CHECA(textos_deduzir_formato(b, 5, 64) == TEXTOS_FORMATO_DESCONHECIDO, "u16 tamanho errado");

  /* Casos que não decidem */
  const uint8_t ctl[] = {0x0E, 0x00, 0x02, 0x00, 'a', 0};
  CHECA(textos_deduzir_formato(ctl, 5, 6) == TEXTOS_FORMATO_DESCONHECIDO, "comeca com controle");
  const uint8_t um[] = {'A', 0, 0, 0};
  CHECA(textos_deduzir_formato(um, 1, 4) == TEXTOS_FORMATO_DESCONHECIDO, "uma letra so");
  CHECA(textos_deduzir_formato(0, 3, 4) == TEXTOS_FORMATO_DESCONHECIDO, "nulo");
  CHECA(textos_deduzir_formato((const uint8_t *)u8, 0, 17) == TEXTOS_FORMATO_DESCONHECIDO, "tamanho 0");

  /* UTF-16: capacidade */
  uint16_t pequeno[4];
  n = textos_utf16("Perfil", pequeno, 4);
  CHECA(n == 3 && pequeno[3] == 0, "corta na capacidade: %zu", n);
  n = textos_utf16("a\nb", u16, 32);
  CHECA(n == 3 && u16[1] == '\n', "quebra de linha");

  printf("%d de %d verificacoes passaram\n", total - falhas, total);
  return falhas != 0;
}
