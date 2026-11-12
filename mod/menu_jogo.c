#include "menu_jogo.h"

#include <stdbool.h>

#include "diag.h"
#include "input.h"
#include "menu.h"
#include "sys.h"
#include "tela.h"

static menu_t menu;          /* começa zerado: MENU_FECHADO */
static menu_info_t info;
static int trava;            /* as chamadas vêm das leituras do controle do jogo */

static bool tentar_travar(void)
{
  return !__atomic_exchange_n(&trava, 1, __ATOMIC_ACQUIRE);
}

static void destravar(void)
{
  __atomic_store_n(&trava, 0, __ATOMIC_RELEASE);
}

static void consultar(void)
{
  info.ligado = diag_perfis_ligados();
  info.em_uso = diag_perfil_em_uso();
  info.escolhido = diag_perfil_escolhido();
  for (int n = 1; n <= PERFIS_MAX; n++)
    info.tem_save[n] = info.ligado && diag_perfil_tem_save(n);
}

static void mostrar(void)
{
  tela_conteudo_t t;
  menu_montar(&menu, &info, &t);
  tela_mostrar(&t);
}

static void fechar(void)
{
  menu.estado = MENU_FECHADO;
  tela_esconder();
  input_bloquear_jogo(false);
  log_raw("menu: fechado");
}

void menu_jogo_combo(void)
{
  if (!tentar_travar())
    return;
  if (menu.estado != MENU_FECHADO) {
    fechar();
  } else {
    consultar();
    menu_abrir(&menu, &info);
    input_bloquear_jogo(true);
    mostrar();
    log_line_t l;
    log_begin(&l);
    log_str(&l, "menu: aberto (em uso ");
    log_dec(&l, info.em_uso);
    log_str(&l, ", escolhido ");
    log_dec(&l, info.escolhido);
    log_str(&l, info.ligado ? ")" : ", perfis indisponiveis)");
    log_end(&l);
  }
  destravar();
}

void menu_jogo_botoes(uint32_t apertados)
{
  if (!tentar_travar())
    return;
  if (menu.estado != MENU_FECHADO) {
    switch (menu_botoes(&menu, apertados)) {
      case MENU_GRAVAR:
        if (info.ligado) {
          bool ok = diag_gravar_perfil(menu.cursor);
          if (ok)
            info.escolhido = menu.cursor;
          menu_resultado(&menu, ok);
        }
        mostrar();
        break;
      case MENU_FECHOU:
        fechar();
        break;
      default:
        mostrar();
        break;
    }
  }
  destravar();
}
