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
  CHECA(textos_para_rotulo("MessageWindowTransparency_Title", true, TEXTOS_PT) &&
        strcmp(textos_para_rotulo("MessageWindowTransparency_Title", true, TEXTOS_PT), "Perfil de save") == 0, "titulo");
  CHECA(textos_para_rotulo("MessageWindowTransparency_Caption", true, TEXTOS_PT) != 0, "legenda");
  CHECA(strstr(textos_para_rotulo("MessageWindowTransparency_Caption", true, TEXTOS_PT), "reinicia") != 0, "legenda com reinicio");
  CHECA(strstr(textos_para_rotulo("MessageWindowTransparency_Caption", false, TEXTOS_PT), "aberto de novo") != 0, "legenda sem reinicio");
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_03", false, TEXTOS_PT), "Perfil 4") == 0, "escolha nao depende do reinicio");
  for (int i = 0; i < 9; i++) {
    char r[64], esperado[16];
    snprintf(r, sizeof r, "MessageWindowTransparency_%02d", i);
    snprintf(esperado, sizeof esperado, "Perfil %d", i + 1);
    const char *t = textos_para_rotulo(r, true, TEXTOS_PT);
    CHECA(t && strcmp(t, esperado) == 0, "%s -> %s", r, t ? t : "(nada)");
  }
  CHECA(!textos_para_rotulo("MessageWindowTransparency_09", true, TEXTOS_PT), "09 nao existe");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_0", true, TEXTOS_PT), "0 sozinho");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_000", true, TEXTOS_PT), "000");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_", true, TEXTOS_PT), "so o prefixo");
  CHECA(!textos_para_rotulo("MessageWindowTransparency", true, TEXTOS_PT), "sem _");
  CHECA(!textos_para_rotulo("VoiceLanguage_Title", true, TEXTOS_PT), "rotulo do jogo");
  CHECA(!textos_para_rotulo("ControllerRumble_00", true, TEXTOS_PT), "escolha do jogo");
  CHECA(!textos_para_rotulo("", true, TEXTOS_PT), "vazio");
  CHECA(!textos_para_rotulo(0, true, TEXTOS_PT), "nulo");
  CHECA(!textos_para_rotulo("MessageWindowTransparency_TitleX", true, TEXTOS_PT), "titulo com sobra");

  /* Idiomas */
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_Title", true, TEXTOS_EN), "Save Profile") == 0, "titulo en");
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_02", true, TEXTOS_EN), "Profile 3") == 0, "escolha en");
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_00", true, TEXTOS_JA), "プロファイル1") == 0, "escolha ja");
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_08", true, TEXTOS_RU), "Профиль 9") == 0, "escolha ru");
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_Title", true, TEXTOS_IDIOMA_NENHUM), "Save Profile") == 0, "sem idioma: ingles");
  CHECA(strcmp(textos_para_rotulo("MessageWindowTransparency_Title", true, (textos_idioma_t)99), "Save Profile") == 0, "idioma invalido: ingles");
  for (int i = 0; i < TEXTOS_N_IDIOMAS; i++) {
    const char *c1 = textos_para_rotulo("MessageWindowTransparency_Caption", true, (textos_idioma_t)i);
    const char *c2 = textos_para_rotulo("MessageWindowTransparency_Caption", false, (textos_idioma_t)i);
    const char *t1 = textos_para_rotulo("MessageWindowTransparency_Title", true, (textos_idioma_t)i);
    const char *e9 = textos_para_rotulo("MessageWindowTransparency_08", true, (textos_idioma_t)i);
    CHECA(t1 && *t1 && c1 && c2 && strcmp(c1, c2) != 0 && strchr(c1, '\n') && strchr(c2, '\n'),
          "idioma %d: textos", i);
    CHECA(e9 && e9[strlen(e9) - 1] == '9', "idioma %d: escolha 9 termina em 9: %s", i, e9 ? e9 : "(nada)");
    uint16_t u[160];
    CHECA(textos_utf16(c1, u, 160) < 159, "idioma %d: legenda cabe no UTF-16", i);
  }
  CHECA(textos_idioma_de_caminho("content:/Mals/USpt.Product.110.sarc.zs") == TEXTOS_PT, "mals pt");
  CHECA(textos_idioma_de_caminho("content:/Mals/EUen.Product.110.sarc.zs") == TEXTOS_EN, "mals euen");
  CHECA(textos_idioma_de_caminho("content:/Mals/USes.Product.110.sarc.zs") == TEXTOS_ES, "mals uses");
  CHECA(textos_idioma_de_caminho("content:/Mals/TWzh.Product.110.sarc.zs") == TEXTOS_ZHTW, "mals tw");
  CHECA(textos_idioma_de_caminho("content:/Mals/CNzh.Product.110.sarc.zs") == TEXTOS_ZHCN, "mals cn");
  CHECA(textos_idioma_de_caminho("content:/Mals/JPja.Product.110.sarc.zs") == TEXTOS_JA, "mals ja");
  CHECA(textos_idioma_de_caminho("content:/Mals/Msg_USen.d.sarc") == TEXTOS_IDIOMA_NENHUM, "Msg_ nao conta");
  CHECA(textos_idioma_de_caminho("content:/Pack/Bootup.pack.zs") == TEXTOS_IDIOMA_NENHUM, "outro arquivo");
  CHECA(textos_idioma_de_caminho("content:/Mals/XXxx.sarc") == TEXTOS_IDIOMA_NENHUM, "codigo desconhecido");
  CHECA(textos_idioma_de_caminho(0) == TEXTOS_IDIOMA_NENHUM, "caminho nulo");
  CHECA(textos_idioma_de_codigo("pt-BR") == TEXTOS_PT, "codigo pt-BR");
  CHECA(textos_idioma_de_codigo("en-US") == TEXTOS_EN, "codigo en-US");
  CHECA(textos_idioma_de_codigo("es-419") == TEXTOS_ES, "codigo es-419");
  CHECA(textos_idioma_de_codigo("zh-Hant") == TEXTOS_ZHTW, "codigo zh-Hant");
  CHECA(textos_idioma_de_codigo("zh-TW") == TEXTOS_ZHTW, "codigo zh-TW");
  CHECA(textos_idioma_de_codigo("zh-Hans") == TEXTOS_ZHCN, "codigo zh-Hans");
  CHECA(textos_idioma_de_codigo("zh-CN") == TEXTOS_ZHCN, "codigo zh-CN");
  CHECA(textos_idioma_de_codigo("ja") == TEXTOS_JA, "codigo ja");
  CHECA(textos_idioma_de_codigo("ko") == TEXTOS_KO, "codigo ko");
  CHECA(textos_idioma_de_codigo("xx") == TEXTOS_IDIOMA_NENHUM, "codigo desconhecido");
  CHECA(textos_idioma_de_codigo("") == TEXTOS_IDIOMA_NENHUM, "codigo vazio");

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
