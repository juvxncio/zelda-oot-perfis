/* Testes da lógica do menu (mod/menu.c), compilados e rodados no PC: make test */
#include <stdio.h>
#include <string.h>

#include "../mod/input.h"
#include "../mod/menu.h"

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

static menu_info_t info_padrao(void)
{
  menu_info_t i;
  memset(&i, 0, sizeof(i));
  i.ligado = true;
  i.em_uso = 1;
  i.escolhido = 1;
  i.tem_save[1] = true;
  i.tem_save[2] = true;
  return i;
}

static int achar(const tela_conteudo_t *t, const char *trecho)
{
  for (int i = 0; i < TELA_LINHAS; i++)
    if (strstr(t->texto[i], trecho))
      return i;
  return -1;
}

static void sem_estouro(const tela_conteudo_t *t)
{
  for (int i = 0; i < TELA_LINHAS; i++)
    CHECA(strlen(t->texto[i]) <= TELA_COLUNAS, "linha %d com %zu colunas", i, strlen(t->texto[i]));
  CHECA(t->linhas >= 1 && t->linhas <= TELA_LINHAS, "linhas=%d", t->linhas);
}

int main(void)
{
  menu_t m;
  tela_conteudo_t t;
  menu_info_t info = info_padrao();

  /* Abre na lista, com o cursor no perfil escolhido. */
  menu_abrir(&m, &info);
  CHECA(m.estado == MENU_LISTA && m.cursor == 1, "abrir: estado %d cursor %d", m.estado, m.cursor);
  info.escolhido = 3;
  menu_abrir(&m, &info);
  CHECA(m.cursor == 3, "abrir no escolhido: cursor %d", m.cursor);
  info.escolhido = 0;   /* inválido: vai para 1 */
  menu_abrir(&m, &info);
  CHECA(m.cursor == 1, "escolhido invalido: cursor %d", m.cursor);
  info = info_padrao();

  /* Navegação, com volta nas pontas; direcional e analógico. */
  menu_abrir(&m, &info);
  CHECA(menu_botoes(&m, BTN_DDOWN) == MENU_NADA && m.cursor == 2, "baixo: %d", m.cursor);
  menu_botoes(&m, BTN_LS_DOWN);
  CHECA(m.cursor == 3, "analogico baixo: %d", m.cursor);
  menu_botoes(&m, BTN_DUP);
  menu_botoes(&m, BTN_LS_UP);
  CHECA(m.cursor == 1, "cima duas vezes: %d", m.cursor);
  menu_botoes(&m, BTN_DUP);
  CHECA(m.cursor == PERFIS_MAX, "cima no 1 vai para o ultimo: %d", m.cursor);
  menu_botoes(&m, BTN_DDOWN);
  CHECA(m.cursor == 1, "baixo no ultimo volta ao 1: %d", m.cursor);
  menu_botoes(&m, BTN_L | BTN_R | BTN_X);
  CHECA(m.cursor == 1 && m.estado == MENU_LISTA, "outros botoes nao fazem nada");

  /* Lista: texto */
  menu_abrir(&m, &info);
  menu_botoes(&m, BTN_DDOWN);
  menu_montar(&m, &info, &t);
  sem_estouro(&t);
  CHECA(achar(&t, "PERFIS DE SAVE") == 0 && t.cor[0] == TELA_TITULO, "titulo");
  int l1 = achar(&t, "Perfil 1"), l2 = achar(&t, "Perfil 2"), l3 = achar(&t, "Perfil 3"), l9 = achar(&t, "Perfil 9");
  CHECA(l1 > 0 && l2 == l1 + 1 && l9 == l1 + 8, "9 perfis em ordem: %d %d %d", l1, l2, l9);
  CHECA(strstr(t.texto[l1], "em uso") != 0, "perfil 1 em uso: %s", t.texto[l1]);
  CHECA(t.texto[l2][0] == '>' && t.cor[l2] == TELA_SELECIONADA, "perfil 2 selecionado: %s", t.texto[l2]);
  CHECA(strstr(t.texto[l2], "com save") != 0, "perfil 2 com save: %s", t.texto[l2]);
  CHECA(strstr(t.texto[l3], "vazio") != 0 && t.cor[l3] == TELA_APAGADA, "perfil 3 vazio: %s", t.texto[l3]);
  CHECA(achar(&t, "A escolher") > l9, "ajuda no fim");
  CHECA(achar(&t, "Perfil 10") < 0, "nao existe perfil 10");

  /* "proximo": escolhido diferente do em uso */
  info.escolhido = 2;
  menu_abrir(&m, &info);
  menu_montar(&m, &info, &t);
  CHECA(strstr(t.texto[achar(&t, "Perfil 2")], "proximo") != 0, "perfil 2 proximo");
  info = info_padrao();

  /* A pede para gravar o perfil do cursor; sucesso -> confirmado */
  menu_abrir(&m, &info);
  menu_botoes(&m, BTN_DDOWN);
  CHECA(menu_botoes(&m, BTN_A) == MENU_GRAVAR && m.cursor == 2, "A grava o 2");
  menu_resultado(&m, true);
  CHECA(m.estado == MENU_CONFIRMADO && m.gravado == 2, "confirmado 2");
  menu_montar(&m, &info, &t);
  sem_estouro(&t);
  CHECA(achar(&t, "Perfil 2 escolhido.") > 0, "mensagem de escolhido");
  CHECA(achar(&t, "Feche o jogo e abra de novo") > 0, "mensagem de reabrir");
  CHECA(achar(&t, "continua no") > 0 && achar(&t, "Perfil 1.") > 0, "continua no perfil 1");
  CHECA(menu_botoes(&m, BTN_DDOWN) == MENU_NADA && m.estado == MENU_CONFIRMADO, "confirmado ignora setas");
  CHECA(menu_botoes(&m, BTN_A) == MENU_FECHOU && m.estado == MENU_FECHADO, "A fecha o confirmado");

  /* Escolher o mesmo perfil em uso */
  menu_abrir(&m, &info);
  menu_botoes(&m, BTN_A);
  menu_resultado(&m, true);
  menu_montar(&m, &info, &t);
  CHECA(achar(&t, "ja esta no Perfil 1.") > 0, "ja esta no perfil 1");
  CHECA(achar(&t, "Feche o jogo") < 0, "sem pedir para reabrir");

  /* Falha ao gravar */
  menu_abrir(&m, &info);
  menu_botoes(&m, BTN_A);
  menu_resultado(&m, false);
  CHECA(m.estado == MENU_ERRO, "erro");
  menu_montar(&m, &info, &t);
  CHECA(achar(&t, "Nao foi possivel gravar.") > 0 && achar(&t, "Nada mudou.") > 0, "mensagem de erro");
  CHECA(menu_botoes(&m, BTN_B) == MENU_FECHOU, "B fecha o erro");

  /* B fecha a lista */
  menu_abrir(&m, &info);
  CHECA(menu_botoes(&m, BTN_B) == MENU_FECHOU && m.estado == MENU_FECHADO, "B fecha a lista");
  CHECA(menu_botoes(&m, BTN_A) == MENU_NADA, "fechado ignora botoes");

  /* Perfis desligados */
  info.ligado = false;
  menu_abrir(&m, &info);
  menu_montar(&m, &info, &t);
  sem_estouro(&t);
  CHECA(achar(&t, "indisponiveis") > 0 && achar(&t, "Perfil 1") < 0, "desligado: aviso, sem lista");

  printf("%d de %d verificacoes passaram\n", total - falhas, total);
  return falhas != 0;
}
