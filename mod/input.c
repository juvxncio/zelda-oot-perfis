/* Adaptado em parte de sloperina (src/ootgz/input.c, GPL-3.0). */
#include "input.h"

#include "got.h"
#include "sys.h"

/* Estado do nn::hid (layout do Switch 1: sampling number, botões, dois analógicos,
 * atributos). Só os campos do começo são lidos; o sloperina usa o mesmo layout neste jogo. */
typedef struct {
  int64_t  sampling_number;
  uint64_t buttons;
  int32_t  lx, ly, rx, ry;
  uint32_t attributes;
} npad_state_t;

#define NPAD_ATTR_CONNECTED 1u
#define INPUT_MAX_MUDANCAS_LOG 300

/* As variantes públicas, na ordem dos substitutos abaixo. */
#define PUBLIC(style) "_ZN2nn3hid13GetNpadStatesEPNS0_" style "EiRKj"
typedef int (*hid_fn)(void *out, int count, const uint32_t *id);

int hid_fullkey(void *, int, const uint32_t *)  __asm__(PUBLIC("16NpadFullKeyState"));
int hid_handheld(void *, int, const uint32_t *) __asm__(PUBLIC("17NpadHandheldState"));
int hid_joydual(void *, int, const uint32_t *)  __asm__(PUBLIC("16NpadJoyDualState"));
int hid_joyleft(void *, int, const uint32_t *)  __asm__(PUBLIC("16NpadJoyLeftState"));
int hid_joyright(void *, int, const uint32_t *) __asm__(PUBLIC("17NpadJoyRightState"));

enum { ESTILO_FULLKEY, ESTILO_HANDHELD, ESTILO_JOYDUAL, ESTILO_JOYLEFT, ESTILO_JOYRIGHT, N_ESTILOS };
static const char *const nomes_estilo[N_ESTILOS] = { "FullKey", "Handheld", "JoyDual", "JoyLeft",
                                                     "JoyRight" };

#define NPAD_STATE_SIZE 0x28   /* estado do Switch 1: o passo entre estados (como o sloperina) */
#define ESPERA_SOLTAR_S 2      /* no máximo isto esperando soltar os botões ao desbloquear */

static hid_fn next_hid[N_ESTILOS];
static uint64_t segurados[N_ESTILOS];   /* botões do último estado lido, por estilo (0 se desconectado) */
static bool visto[N_ESTILOS];           /* já anotou no log que este estilo tem botões */
static input_combo_fn combo_cb;
static input_botoes_fn botoes_cb;

static bool bloqueado;          /* o menu está aberto */
static bool esperando_soltar;   /* menu fechado, mas ainda há botão segurado */
static uint64_t espera_desde;   /* tick em que começou a esperar */

static uint64_t tick(void)
{
  uint64_t t;
  __asm__ volatile("mrs %0, cntpct_el0" : "=r"(t));
  return t;
}

static uint64_t ticks_por_segundo(void)
{
  uint64_t f;
  __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(f));
  return f;
}

void input_bloquear_jogo(bool b)
{
  if (!b && __atomic_load_n(&bloqueado, __ATOMIC_ACQUIRE)) {
    espera_desde = tick();
    __atomic_store_n(&esperando_soltar, true, __ATOMIC_RELEASE);
  }
  __atomic_store_n(&bloqueado, b, __ATOMIC_RELEASE);
}

static bool algum_segurado(void)
{
  for (int i = 0; i < N_ESTILOS; i++)
    if (segurados[i])
      return true;
  return false;
}

/* O jogo deve ver o controle parado agora? (menu aberto, ou esperando soltar) */
static bool esconder_do_jogo(void)
{
  if (__atomic_load_n(&bloqueado, __ATOMIC_ACQUIRE))
    return true;
  if (!__atomic_load_n(&esperando_soltar, __ATOMIC_ACQUIRE))
    return false;
  if (!algum_segurado() || tick() - espera_desde > ESPERA_SOLTAR_S * ticks_por_segundo()) {
    __atomic_store_n(&esperando_soltar, false, __ATOMIC_RELEASE);
    return false;
  }
  return true;
}

/* Zera botões e analógicos de cada estado que o jogo acabou de receber. */
static void apagar(void *out, int lidos)
{
  for (int i = 0; i < lidos; i++) {
    npad_state_t *s = (npad_state_t *)((char *)out + (size_t)i * NPAD_STATE_SIZE);
    s->buttons = 0;
    s->lx = s->ly = s->rx = s->ry = 0;
  }
}

/* Lê o estado mais novo (o primeiro) que o jogo acabou de receber. Não muda nada. */
static void observar(int estilo, const void *out, int lidos, const uint32_t *id)
{
  if (lidos < 1 || !out)
    return;
  const npad_state_t *s = (const npad_state_t *)out;
  if (!(s->attributes & NPAD_ATTR_CONNECTED)) {
    segurados[estilo] = 0;   /* um controle que sumiu não prende o desbloqueio */
    return;
  }
  uint64_t antes = segurados[estilo];
  uint64_t agora = s->buttons;
  segurados[estilo] = agora;

  if (agora && !visto[estilo]) {
    visto[estilo] = true;
    log_line_t l;
    log_begin(&l);
    log_str(&l, "input: primeiro botao no estilo ");
    log_str(&l, nomes_estilo[estilo]);
    log_str(&l, " (npad ");
    log_hex(&l, id ? *id : 0xFFFFFFFFu);
    log_str(&l, "): botoes=");
    log_hex(&l, agora);
    log_end(&l);
  }

  /* Diagnóstico: cada mudança de botões, para ver o que o teclado/controle de cada pessoa manda
   * (bits 16-19: analógico esquerdo como direções; 20-23: o direito). Limitado. */
  static uint32_t mudancas;
  if (agora != antes && mudancas < INPUT_MAX_MUDANCAS_LOG) {
    mudancas++;
    log_line_t l;
    log_begin(&l);
    log_str(&l, "input: ");
    log_str(&l, nomes_estilo[estilo]);
    log_str(&l, " botoes=");
    log_hex(&l, agora);
    log_end(&l);
  }

  bool completa = (agora & INPUT_COMBO_MENU) == INPUT_COMBO_MENU;
  bool estava = (antes & INPUT_COMBO_MENU) == INPUT_COMBO_MENU;
  if (completa && !estava) {
    log_line_t l;
    log_begin(&l);
    log_str(&l, "input: combinacao do menu (L+R+baixo) no estilo ");
    log_str(&l, nomes_estilo[estilo]);
    log_end(&l);
    if (combo_cb)
      combo_cb();
    return;
  }

  uint32_t apertados = (uint32_t)(agora & ~antes);
  if (apertados && botoes_cb && __atomic_load_n(&bloqueado, __ATOMIC_ACQUIRE))
    botoes_cb(apertados);
}

#define WRAP(nome, estilo)                                                \
  static int wrap_##nome(void *out, int count, const uint32_t *id)        \
  {                                                                       \
    int lidos = next_hid[estilo](out, count, id);                         \
    int validos = lidos < count ? lidos : count;                          \
    observar(estilo, out, validos, id);                                   \
    if (out && validos > 0 && esconder_do_jogo())                         \
      apagar(out, validos);                                               \
    return lidos;                                                         \
  }
WRAP(fullkey, ESTILO_FULLKEY)
WRAP(handheld, ESTILO_HANDHELD)
WRAP(joydual, ESTILO_JOYDUAL)
WRAP(joyleft, ESTILO_JOYLEFT)
WRAP(joyright, ESTILO_JOYRIGHT)

static int um(uintptr_t main_base, const char *nome, void *wrap, void *fn, int estilo)
{
  const char *problema = got_hook(main_base, nome, wrap, fn, (void **)&next_hid[estilo]);
  if (!problema)
    return 1;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "input: nao interceptado ");
  log_str(&l, nome);
  log_str(&l, " (");
  log_str(&l, problema);
  log_str(&l, ")");
  log_end(&l);
  return 0;
}

int input_install(uintptr_t main_base, input_combo_fn on_combo, input_botoes_fn on_botoes)
{
  combo_cb = on_combo;
  botoes_cb = on_botoes;
  int ok = 0;
  /* endereços tomados no código: relocações GLOB_DAT, como em diag.c */
  ok += um(main_base, PUBLIC("16NpadFullKeyState"), (void *)wrap_fullkey, (void *)hid_fullkey, ESTILO_FULLKEY);
  ok += um(main_base, PUBLIC("17NpadHandheldState"), (void *)wrap_handheld, (void *)hid_handheld, ESTILO_HANDHELD);
  ok += um(main_base, PUBLIC("16NpadJoyDualState"), (void *)wrap_joydual, (void *)hid_joydual, ESTILO_JOYDUAL);
  ok += um(main_base, PUBLIC("16NpadJoyLeftState"), (void *)wrap_joyleft, (void *)hid_joyleft, ESTILO_JOYLEFT);
  ok += um(main_base, PUBLIC("17NpadJoyRightState"), (void *)wrap_joyright, (void *)hid_joyright, ESTILO_JOYRIGHT);
  log_line_t l;
  log_begin(&l);
  log_str(&l, "input: ");
  log_dec(&l, ok);
  log_str(&l, " de 5 leituras do controle interceptadas");
  log_end(&l);
  return ok;
}
