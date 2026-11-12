#include "menu.h"

#include "input.h"

#define BTN_CIMA  (BTN_DUP | BTN_LS_UP)
#define BTN_BAIXO (BTN_DDOWN | BTN_LS_DOWN)

static int limitar(int n)
{
  return n < 1 ? 1 : n > PERFIS_MAX ? PERFIS_MAX : n;
}

void menu_abrir(menu_t *m, const menu_info_t *info)
{
  m->estado = MENU_LISTA;
  m->cursor = limitar(info->escolhido);
  m->gravado = 0;
}

menu_acao_t menu_botoes(menu_t *m, uint32_t apertados)
{
  switch (m->estado) {
    case MENU_LISTA:
      if (apertados & BTN_B) {
        m->estado = MENU_FECHADO;
        return MENU_FECHOU;
      }
      if (apertados & BTN_A)
        return MENU_GRAVAR;
      if (apertados & BTN_CIMA)
        m->cursor = m->cursor <= 1 ? PERFIS_MAX : m->cursor - 1;   /* dá a volta */
      else if (apertados & BTN_BAIXO)
        m->cursor = m->cursor >= PERFIS_MAX ? 1 : m->cursor + 1;
      return MENU_NADA;
    case MENU_CONFIRMADO:
    case MENU_ERRO:
      if (apertados & (BTN_A | BTN_B)) {
        m->estado = MENU_FECHADO;
        return MENU_FECHOU;
      }
      return MENU_NADA;
    default:
      return MENU_NADA;
  }
}

void menu_resultado(menu_t *m, bool gravou)
{
  if (gravou) {
    m->estado = MENU_CONFIRMADO;
    m->gravado = m->cursor;
  } else {
    m->estado = MENU_ERRO;
  }
}

/* --- texto ------------------------------------------------------------------------- */

typedef struct {
  tela_conteudo_t *t;
  int linha;
} escritor_t;

static void anexar(char *dst, int *len, const char *s)
{
  while (*s && *len < TELA_COLUNAS)
    dst[(*len)++] = *s++;
  dst[*len] = 0;
}

/* Uma linha: até três pedaços de texto (o do meio pode ser um número). */
static void linha(escritor_t *e, uint8_t cor, const char *a, int numero, const char *b)
{
  if (e->linha >= TELA_LINHAS)
    return;
  char *dst = e->t->texto[e->linha];
  int len = 0;
  dst[0] = 0;
  anexar(dst, &len, a);
  if (numero > 0) {
    char n[3] = { (char)('0' + numero % 10), 0, 0 };
    if (numero >= 10) {
      n[0] = (char)('0' + numero / 10 % 10);
      n[1] = (char)('0' + numero % 10);
    }
    anexar(dst, &len, n);
  }
  if (b)
    anexar(dst, &len, b);
  e->t->cor[e->linha] = cor;
  e->linha++;
}

static void vazia(escritor_t *e)
{
  linha(e, TELA_NORMAL, "", 0, 0);
}

static const char *situacao(int n, const menu_info_t *info)
{
  if (n == info->em_uso)
    return "  em uso";
  if (n == info->escolhido)
    return "  proximo";
  return info->tem_save[n] ? "  com save" : "  vazio";
}

void menu_montar(const menu_t *m, const menu_info_t *info, tela_conteudo_t *out)
{
  escritor_t e = { out, 0 };
  linha(&e, TELA_TITULO, "PERFIS DE SAVE", 0, 0);
  vazia(&e);

  if (!info->ligado) {
    linha(&e, TELA_AVISO, "Perfis indisponiveis agora.", 0, 0);
    linha(&e, TELA_NORMAL, "Detalhes no log do emulador.", 0, 0);
    vazia(&e);
    linha(&e, TELA_APAGADA, "B fechar", 0, 0);
  } else if (m->estado == MENU_LISTA) {
    for (int n = 1; n <= PERFIS_MAX; n++) {
      bool sel = n == m->cursor;
      uint8_t cor = sel ? TELA_SELECIONADA
                        : (n == info->em_uso || info->tem_save[n]) ? TELA_NORMAL : TELA_APAGADA;
      linha(&e, cor, sel ? "> Perfil " : "  Perfil ", n, situacao(n, info));
    }
    vazia(&e);
    linha(&e, TELA_APAGADA, "A escolher   B fechar", 0, 0);
  } else if (m->estado == MENU_CONFIRMADO && m->gravado == info->em_uso) {
    linha(&e, TELA_AVISO, "Voce ja esta no Perfil ", m->gravado, ".");
    vazia(&e);
    linha(&e, TELA_NORMAL, "Nada muda ao reabrir o jogo.", 0, 0);
    vazia(&e);
    linha(&e, TELA_APAGADA, "A ou B fechar", 0, 0);
  } else if (m->estado == MENU_CONFIRMADO) {
    linha(&e, TELA_AVISO, "Perfil ", m->gravado, " escolhido.");
    vazia(&e);
    linha(&e, TELA_NORMAL, "Feche o jogo e abra de novo", 0, 0);
    linha(&e, TELA_NORMAL, "para jogar nesse perfil.", 0, 0);
    vazia(&e);
    linha(&e, TELA_APAGADA, "Ate la, o jogo continua no", 0, 0);
    linha(&e, TELA_APAGADA, "Perfil ", info->em_uso, ".");
    vazia(&e);
    linha(&e, TELA_APAGADA, "A ou B fechar", 0, 0);
  } else {
    linha(&e, TELA_AVISO, "Nao foi possivel gravar.", 0, 0);
    linha(&e, TELA_NORMAL, "Nada mudou.", 0, 0);
    vazia(&e);
    linha(&e, TELA_APAGADA, "A ou B fechar", 0, 0);
  }

  out->linhas = e.linha;
  while (e.linha < TELA_LINHAS)
    vazia(&e);
}
