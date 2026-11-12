/* Parte pura de textos.h: sem nada do Switch, compilada também nos testes do PC. */
#include <stdbool.h>

#include "textos.h"

#define PREFIXO "MessageWindowTransparency_"

/* Os textos em cada idioma do jogo. `escolha` recebe o número do perfil logo depois. */
typedef struct {
  const char *titulo;
  const char *escolha;
  const char *legenda;            /* primeira linha da legenda */
  const char *legenda_reinicia;   /* segunda linha: reinicia ao sair das Opções */
  const char *legenda_depois;     /* segunda linha: vale ao abrir o jogo de novo */
} idioma_textos_t;

static const idioma_textos_t textos[TEXTOS_N_IDIOMAS] = {
  [TEXTOS_EN] = {"Save Profile", "Profile ",
                 "Chooses which save the game uses.",
                 "The game restarts when you leave Options.",
                 "Takes effect the next time the game starts."},
  [TEXTOS_PT] = {"Perfil de save", "Perfil ",
                 "Escolhe qual save o jogo vai usar.",
                 "Ao sair das Opções, o jogo reinicia nele.",
                 "Vale quando o jogo for aberto de novo."},
  [TEXTOS_ES] = {"Perfil de guardado", "Perfil ",
                 "Elige qué datos de guardado usa el juego.",
                 "Al salir de Opciones, el juego se reinicia.",
                 "Se aplica al volver a iniciar el juego."},
  [TEXTOS_FR] = {"Profil de sauvegarde", "Profil ",
                 "Choisit la sauvegarde utilisée par le jeu.",
                 "Le jeu redémarre en quittant les options.",
                 "Prendra effet au prochain lancement du jeu."},
  [TEXTOS_DE] = {"Speicherprofil", "Profil ",
                 "Wählt, welchen Spielstand das Spiel nutzt.",
                 "Das Spiel startet beim Verlassen neu.",
                 "Gilt ab dem nächsten Spielstart."},
  [TEXTOS_IT] = {"Profilo di salvataggio", "Profilo ",
                 "Sceglie quale salvataggio usa il gioco.",
                 "Uscendo dalle opzioni, il gioco si riavvia.",
                 "Avrà effetto al prossimo avvio del gioco."},
  [TEXTOS_NL] = {"Opslagprofiel", "Profiel ",
                 "Kiest welke opslag het spel gebruikt.",
                 "Het spel herstart bij het verlaten.",
                 "Geldt bij de volgende start van het spel."},
  [TEXTOS_PL] = {"Profil zapisu", "Profil ",
                 "Wybiera zapis używany przez grę.",
                 "Po wyjściu z opcji gra uruchomi się ponownie.",
                 "Zadziała przy następnym uruchomieniu gry."},
  [TEXTOS_RU] = {"Профиль сохранения", "Профиль ",
                 "Выбирает сохранение, которое использует игра.",
                 "При выходе из настроек игра перезапустится.",
                 "Вступит в силу при следующем запуске."},
  [TEXTOS_JA] = {"セーブプロファイル", "プロファイル",
                 "ゲームで使うセーブデータを選びます。",
                 "オプションを閉じると、ゲームが再起動します。",
                 "次にゲームを起動したときに反映されます。"},
  [TEXTOS_KO] = {"세이브 프로필", "프로필 ",
                 "게임에서 사용할 세이브 데이터를 선택합니다.",
                 "옵션을 닫으면 게임이 다시 시작됩니다.",
                 "다음에 게임을 시작할 때 적용됩니다."},
  [TEXTOS_ZHCN] = {"存档配置", "配置",
                   "选择游戏使用的存档。",
                   "退出选项后，游戏将重新启动。",
                   "将在下次启动游戏时生效。"},
  [TEXTOS_ZHTW] = {"存檔設定", "設定",
                   "選擇遊戲使用的存檔。",
                   "離開選項後，遊戲將重新啟動。",
                   "將在下次啟動遊戲時生效。"},
};

/* Textos montados na primeira vez (o jogo pode guardar o ponteiro: ficam para sempre). */
#define MONTADO_CAP 160
static char escolhas[TEXTOS_N_IDIOMAS][9][MONTADO_CAP / 4];
static char legendas[TEXTOS_N_IDIOMAS][2][MONTADO_CAP];

static void juntar(char *dst, size_t cap, const char *a, const char *sep, const char *b)
{
  size_t n = 0;
  for (const char *p = a; *p && n + 1 < cap; p++)
    dst[n++] = *p;
  for (const char *p = sep; *p && n + 1 < cap; p++)
    dst[n++] = *p;
  for (const char *p = b; *p && n + 1 < cap; p++)
    dst[n++] = *p;
  dst[n] = 0;
}

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

const char *textos_para_rotulo(const char *rotulo, bool reinicia, textos_idioma_t idioma)
{
  if (!rotulo || !comeca(rotulo, PREFIXO))
    return 0;
  if ((int)idioma < 0 || idioma >= TEXTOS_N_IDIOMAS)
    idioma = TEXTOS_EN;
  const idioma_textos_t *t = &textos[idioma];
  const char *s = rotulo + sizeof(PREFIXO) - 1;
  if (igual(s, "Title"))
    return t->titulo;
  if (igual(s, "Caption")) {
    char *d = legendas[idioma][reinicia ? 0 : 1];
    if (!d[0])
      juntar(d, MONTADO_CAP, t->legenda, "\n", reinicia ? t->legenda_reinicia : t->legenda_depois);
    return d;
  }
  if (s[0] == '0' && s[1] >= '0' && s[1] <= '8' && s[2] == 0) {
    int i = s[1] - '0';
    char *d = escolhas[idioma][i];
    if (!d[0]) {
      char num[2] = {(char)('1' + i), 0};
      juntar(d, sizeof(escolhas[0][0]), t->escolha, "", num);
    }
    return d;
  }
  return 0;
}

/* "XXyy" (como USen, EUpt, JPja, CNzh, TWzh) -> idioma. */
static textos_idioma_t de_mals(const char *c)
{
  char a = c[2], b = c[3];
  if (a == 'z' && b == 'h')
    return (c[0] == 'T' && c[1] == 'W') ? TEXTOS_ZHTW : TEXTOS_ZHCN;
  static const struct { char a, b; textos_idioma_t i; } tab[] = {
    {'e', 'n', TEXTOS_EN}, {'p', 't', TEXTOS_PT}, {'e', 's', TEXTOS_ES}, {'f', 'r', TEXTOS_FR},
    {'d', 'e', TEXTOS_DE}, {'i', 't', TEXTOS_IT}, {'n', 'l', TEXTOS_NL}, {'p', 'l', TEXTOS_PL},
    {'r', 'u', TEXTOS_RU}, {'j', 'a', TEXTOS_JA}, {'k', 'o', TEXTOS_KO},
  };
  for (size_t k = 0; k < sizeof(tab) / sizeof(tab[0]); k++)
    if (tab[k].a == a && tab[k].b == b)
      return tab[k].i;
  return TEXTOS_IDIOMA_NENHUM;
}

static bool maiuscula(char c) { return c >= 'A' && c <= 'Z'; }
static bool minuscula(char c) { return c >= 'a' && c <= 'z'; }

textos_idioma_t textos_idioma_de_caminho(const char *caminho)
{
  if (!caminho)
    return TEXTOS_IDIOMA_NENHUM;
  for (const char *p = caminho; *p; p++) {
    if (!comeca(p, "Mals/"))
      continue;
    const char *c = p + 5;
    if (maiuscula(c[0]) && maiuscula(c[1]) && minuscula(c[2]) && minuscula(c[3]) && c[4] == '.')
      return de_mals(c);
  }
  return TEXTOS_IDIOMA_NENHUM;
}

textos_idioma_t textos_idioma_de_codigo(const char *codigo)
{
  if (!codigo || !minuscula(codigo[0]) || !minuscula(codigo[1]))
    return TEXTOS_IDIOMA_NENHUM;
  if (codigo[0] == 'z' && codigo[1] == 'h')
    return (comeca(codigo, "zh-TW") || comeca(codigo, "zh-Hant")) ? TEXTOS_ZHTW : TEXTOS_ZHCN;
  char c[5] = {'X', 'X', codigo[0], codigo[1], 0};
  return de_mals(c);
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
