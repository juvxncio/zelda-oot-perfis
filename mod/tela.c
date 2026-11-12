/* Painel de texto por cima da imagem do jogo. Método adaptado de sloperina (src/ootgz/nvn.c,
 * GPL-3.0): o jogo pega nvnDeviceGetProcAddress pelo nvnBootstrapLoader e busca toda função NVN
 * pelo nome; envolvendo os dois, trocamos:
 *   nvnDeviceInitialize          guardar o device
 *   nvnWindowBuilderSetTextures  guardar as texturas da tela (swapchain)
 *   nvnQueuePresentTexture       antes do present do jogo, copiar o painel para a textura
 * O painel é desenhado na CPU num buffer no formato da tela e copiado com
 * nvnCommandBufferCopyBufferToTexture. Sem shaders.
 *
 * Qualquer falha desliga só o desenho (o jogo e os perfis seguem). O present do jogo nunca
 * espera mais que ESPERA_QUADRO_NS pela nossa cópia anterior. */
#include "tela.h"

#include <stddef.h>

#include "font.h"
#include "got.h"
#include "sys.h"

/* --- tipos do NVN (como o sloperina: objetos opacos em espaço de sobra) ----------------- */

typedef struct nvn_device nvn_device_t;
typedef struct nvn_queue nvn_queue_t;
typedef struct nvn_window nvn_window_t;
typedef struct nvn_texture nvn_texture_t;
typedef uint64_t nvn_addr_t;
typedef uint64_t nvn_handle_t;
typedef struct { int32_t x, y, z, w, h, d; } nvn_region_t;
typedef struct { int32_t x, y, w, h; } nvn_rect_t;
#define OBJ_STORAGE 0x400   /* NVN 1: pool builder 0x40, pool 0x100, command buffer 0xA0, sync 0x40 */
typedef struct { _Alignas(16) uint8_t b[OBJ_STORAGE]; } nvn_obj_t;
typedef void *(*getproc_t)(nvn_device_t *dev, const char *name);

enum {
  POOL_CPU_UNCACHED = 0x02,
  POOL_GPU_UNCACHED = 0x10,
  POOL_GPU_CACHED = 0x20,
  SYNC_ALL_GPU_COMMANDS_COMPLETE = 0,
  SYNC_TIMEOUT_EXPIRED = 2,
  FMT_RGBA8 = 37,
  FMT_RGBA16F = 41,
  FMT_RGBX8_SRGB = 55,
  FMT_RGBA8_SRGB = 56,
  FMT_RGB10A2 = 61,
};

#define ESPERA_QUADRO_NS 100000000ull   /* um décimo de segundo */
#define ESPERA_LONGA_NS 1000000000ull   /* antes de reescrever os pixels */
#define PAGINA 0x1000
#define CMD_TAM 0x10000
#define CTRL_TAM 0x4000
#define ESCALA_MAX 3
#define ALTURA_POR_ESCALA 360           /* 1x a 360 linhas, 2x a 720, 3x a 1080 */
#define CELULA_H (FONT_H + 2)
#define BORDA 4                         /* pixels de fonte em volta do texto */
#define PAINEL_W(esc) ((TELA_COLUNAS * FONT_W + 2 * BORDA) * (esc))
#define PAINEL_H(esc) ((TELA_LINHAS * CELULA_H + 2 * BORDA) * (esc))

typedef void *(*bootstrap_t)(const char *name);
extern void *nvnBootstrapLoader(const char *name);   /* do sdk */
void *aligned_alloc(size_t align, size_t size);      /* do main */
void free(void *p);                                  /* do main */

/* funções NVN que usamos, pela ordem de nomes_fn */
static struct {
  void (*pool_builder_defaults)(nvn_obj_t *);
  void (*pool_builder_device)(nvn_obj_t *, nvn_device_t *);
  void (*pool_builder_flags)(nvn_obj_t *, int);
  void (*pool_builder_storage)(nvn_obj_t *, void *, size_t);
  bool (*pool_init)(nvn_obj_t *, const nvn_obj_t *);
  nvn_addr_t (*pool_address)(const nvn_obj_t *);
  bool (*cmd_init)(nvn_obj_t *, nvn_device_t *);
  void (*cmd_add_command)(nvn_obj_t *, const nvn_obj_t *pool, ptrdiff_t offset, size_t size);
  void (*cmd_add_control)(nvn_obj_t *, void *, size_t);
  void (*cmd_begin)(nvn_obj_t *);
  nvn_handle_t (*cmd_end)(nvn_obj_t *);
  void (*cmd_copy_to_texture)(nvn_obj_t *, nvn_addr_t, const nvn_texture_t *, const void *view,
                              const nvn_region_t *, int flags);
  void (*queue_submit)(nvn_queue_t *, int, const nvn_handle_t *);
  void (*queue_fence)(nvn_queue_t *, nvn_obj_t *sync, int condition, int flags);
  bool (*sync_init)(nvn_obj_t *, nvn_device_t *);
  int (*sync_wait)(const nvn_obj_t *, uint64_t timeout_ns);
  int (*tex_width)(const nvn_texture_t *);
  int (*tex_height)(const nvn_texture_t *);
  int (*tex_format)(const nvn_texture_t *);
  void (*window_crop)(const nvn_window_t *, nvn_rect_t *);
} fn;

static const char *const nomes_fn[] = {
  "nvnMemoryPoolBuilderSetDefaults", "nvnMemoryPoolBuilderSetDevice", "nvnMemoryPoolBuilderSetFlags",
  "nvnMemoryPoolBuilderSetStorage", "nvnMemoryPoolInitialize", "nvnMemoryPoolGetBufferAddress",
  "nvnCommandBufferInitialize", "nvnCommandBufferAddCommandMemory", "nvnCommandBufferAddControlMemory",
  "nvnCommandBufferBeginRecording", "nvnCommandBufferEndRecording",
  "nvnCommandBufferCopyBufferToTexture", "nvnQueueSubmitCommands", "nvnQueueFenceSync",
  "nvnSyncInitialize", "nvnSyncWait", "nvnTextureGetWidth", "nvnTextureGetHeight",
  "nvnTextureGetFormat", "nvnWindowGetCrop",
};
#define N_FN (sizeof(nomes_fn) / sizeof(*nomes_fn))
_Static_assert(N_FN == sizeof(fn) / sizeof(void *), "tabela fn");

/* --- conteúdo publicado (qualquer thread) -> lido no present -------------------------- */

static tela_conteudo_t publicado;
static uint32_t versao_publicada;   /* muda a cada tela_mostrar */
static bool visivel;
static int trava;                   /* spinlock curto em volta de `publicado` */

static void travar(void)
{
  while (__atomic_exchange_n(&trava, 1, __ATOMIC_ACQUIRE))
    ;
}

static void destravar(void)
{
  __atomic_store_n(&trava, 0, __ATOMIC_RELEASE);
}

void tela_mostrar(const tela_conteudo_t *c)
{
  travar();
  publicado = *c;
  if (publicado.linhas < 1)
    publicado.linhas = 1;
  if (publicado.linhas > TELA_LINHAS)
    publicado.linhas = TELA_LINHAS;
  versao_publicada++;
  destravar();
  __atomic_store_n(&visivel, true, __ATOMIC_RELEASE);
}

void tela_esconder(void)
{
  __atomic_store_n(&visivel, false, __ATOMIC_RELEASE);
}

bool tela_visivel(void)
{
  return __atomic_load_n(&visivel, __ATOMIC_ACQUIRE);
}

/* --- ganchos no NVN ---------------------------------------------------------------- */

static void *next_bootstrap;
static getproc_t real_getproc;
static void (*real_present)(nvn_queue_t *, nvn_window_t *, int);
static void (*real_set_textures)(void *builder, int n, nvn_texture_t *const *textures);
static bool (*real_device_init)(nvn_device_t *, const void *builder);

static nvn_device_t *device;
#define MAX_TEXTURAS 8
static nvn_texture_t *texturas[MAX_TEXTURAS];
static int n_texturas;

static bool iguais(const char *a, const char *b)
{
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

static void desenhar(nvn_queue_t *queue, nvn_window_t *window, int index);

static void hook_present(nvn_queue_t *queue, nvn_window_t *window, int index)
{
  desenhar(queue, window, index);
  real_present(queue, window, index);
}

static void hook_set_textures(void *builder, int count, nvn_texture_t *const *textures)
{
  n_texturas = 0;   /* nenhuma usável enquanto a lista é reescrita */
  for (int i = 0; i < count && i < MAX_TEXTURAS; i++)
    texturas[i] = textures[i];
  n_texturas = count < MAX_TEXTURAS ? count : MAX_TEXTURAS;
  real_set_textures(builder, count, textures);
}

static bool hook_device_init(nvn_device_t *dev, const void *builder)
{
  bool ok = real_device_init(dev, builder);
  if (ok)
    device = dev;
  return ok;
}

static void *hook_getproc(nvn_device_t *dev, const char *name);

static void *trocar(const char *name, void *proc)
{
  if (!proc || !name)
    return proc;
  if (iguais(name, "nvnDeviceGetProcAddress")) {
    real_getproc = (getproc_t)proc;
    return (void *)hook_getproc;
  }
  if (iguais(name, "nvnQueuePresentTexture")) {
    real_present = proc;
    return (void *)hook_present;
  }
  if (iguais(name, "nvnWindowBuilderSetTextures")) {
    real_set_textures = proc;
    return (void *)hook_set_textures;
  }
  if (iguais(name, "nvnDeviceInitialize")) {
    real_device_init = proc;
    return (void *)hook_device_init;
  }
  return proc;
}

static void *hook_getproc(nvn_device_t *dev, const char *name)
{
  if (dev && !device)
    device = dev;
  return trocar(name, real_getproc(dev, name));
}

static void *hook_bootstrap(const char *name)
{
  return trocar(name, ((bootstrap_t)next_bootstrap)(name));
}

const char *tela_install(uintptr_t main_base)
{
  const char *problema = got_hook(main_base, "nvnBootstrapLoader", (void *)hook_bootstrap,
                                  (void *)nvnBootstrapLoader, &next_bootstrap);
  log_raw(problema ? "tela: nvnBootstrapLoader NAO interceptado (sem menu na tela)"
                   : "tela: nvnBootstrapLoader interceptado");
  return problema;
}

/* --- desenho ----------------------------------------------------------------------- */

static enum { T_NADA, T_PRONTA, T_FALHOU } estado;
static nvn_obj_t cmd, pool_cmd, pool_pixels, sync_copia;
static uint8_t *mem_cmd, *mem_ctrl, *mem_pixels;
static size_t tam_pixels;
static int formato, bytes_px, escala;
static bool copia_pendente;          /* há uma cópia nossa que a GPU talvez ainda não terminou */
static uint32_t versao_desenhada = 0xFFFFFFFFu;

static void falhar(const char *por_que)
{
  estado = T_FALHOU;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "tela: desenho desligado: ");
  log_str(&l, por_que);
  log_end(&l);
}

static size_t arredondar_pagina(size_t n)
{
  return (n + PAGINA - 1) & ~(size_t)(PAGINA - 1);
}

static void zerar(void *p, size_t n)
{
  volatile uint8_t *b = p;
  for (size_t i = 0; i < n; i++)
    b[i] = 0;
}

static bool criar_pool(nvn_obj_t *pool, void *mem, size_t tam, int flags)
{
  nvn_obj_t builder;
  zerar(&builder, sizeof(builder));
  fn.pool_builder_defaults(&builder);
  fn.pool_builder_device(&builder, device);
  fn.pool_builder_flags(&builder, flags);
  fn.pool_builder_storage(&builder, mem, tam);
  return fn.pool_init(pool, &builder);
}

static int bytes_por_pixel(int f)
{
  switch (f) {
    case FMT_RGBA8: case FMT_RGBA8_SRGB: case FMT_RGBX8_SRGB: case FMT_RGB10A2: return 4;
    case FMT_RGBA16F: return 8;
    default: return 0;
  }
}

static bool iniciar(void)
{
  if (!real_getproc || !device || n_texturas < 1)
    return false;   /* ainda não: tenta no próximo quadro */
  void **slots = (void **)&fn;
  for (unsigned i = 0; i < N_FN; i++) {
    slots[i] = real_getproc(device, nomes_fn[i]);
    if (!slots[i]) {
      log_line_t l;
      log_begin(&l);
      log_str(&l, "tela: NVN sem ");
      log_str(&l, nomes_fn[i]);
      log_end(&l);
      falhar("funcao NVN faltando");
      return false;
    }
  }
  nvn_texture_t *t = texturas[0];
  int altura = fn.tex_height(t);
  formato = fn.tex_format(t);
  bytes_px = bytes_por_pixel(formato);
  escala = altura / ALTURA_POR_ESCALA;
  if (escala < 1)
    escala = 1;
  if (escala > ESCALA_MAX)
    escala = ESCALA_MAX;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "tela: swapchain ");
  log_dec(&l, n_texturas);
  log_str(&l, " texturas ");
  log_dec(&l, fn.tex_width(t));
  log_str(&l, "x");
  log_dec(&l, altura);
  log_str(&l, " formato ");
  log_dec(&l, formato);
  log_str(&l, " escala ");
  log_dec(&l, escala);
  log_end(&l);
  if (!bytes_px) {
    falhar("formato de tela nao suportado");
    return false;
  }
  tam_pixels = arredondar_pagina((size_t)PAINEL_W(escala) * PAINEL_H(escala) * bytes_px);
  mem_pixels = aligned_alloc(PAGINA, tam_pixels);
  mem_cmd = aligned_alloc(PAGINA, CMD_TAM);
  mem_ctrl = aligned_alloc(PAGINA, CTRL_TAM);
  if (!mem_pixels || !mem_cmd || !mem_ctrl) {
    free(mem_pixels);
    free(mem_cmd);
    free(mem_ctrl);
    mem_pixels = mem_cmd = mem_ctrl = 0;
    falhar("sem memoria");
    return false;
  }
  if (!criar_pool(&pool_pixels, mem_pixels, tam_pixels, POOL_CPU_UNCACHED | POOL_GPU_UNCACHED) ||
      !criar_pool(&pool_cmd, mem_cmd, CMD_TAM, POOL_CPU_UNCACHED | POOL_GPU_CACHED)) {
    falhar("memory pool");
    return false;
  }
  if (!fn.cmd_init(&cmd, device)) {
    falhar("command buffer");
    return false;
  }
  if (!fn.sync_init(&sync_copia, device)) {
    falhar("sync");
    return false;
  }
  log_begin(&l);
  log_str(&l, "tela: pronta (");
  log_dec(&l, (int64_t)(tam_pixels / 1024));
  log_str(&l, " KiB para o painel)");
  log_end(&l);
  estado = T_PRONTA;
  return true;
}

/* uma cor opaca no formato da tela */
static uint16_t meio_float(uint8_t v)
{
  if (!v)
    return 0;
  float f = v / 255.0f;
  uint32_t bits;
  __builtin_memcpy(&bits, &f, 4);
  int exp = (int)((bits >> 23) & 0xFF) - 127 + 15;
  return (uint16_t)(exp << 10 | ((bits >> 13) & 0x3FF));
}

static uint64_t codificar(const uint8_t rgb[3])
{
  switch (formato) {
    case FMT_RGB10A2:
      return (uint32_t)(rgb[0] * 1023 / 255) | (uint32_t)(rgb[1] * 1023 / 255) << 10 |
             (uint32_t)(rgb[2] * 1023 / 255) << 20 | 3u << 30;
    case FMT_RGBA16F:
      return meio_float(rgb[0]) | (uint64_t)meio_float(rgb[1]) << 16 |
             (uint64_t)meio_float(rgb[2]) << 32 | (uint64_t)0x3C00 << 48;
    default:
      return (uint32_t)rgb[0] | (uint32_t)rgb[1] << 8 | (uint32_t)rgb[2] << 16 | 0xFFu << 24;
  }
}

static const uint8_t rgb_cor[TELA_CORES][3] = {
  [TELA_NORMAL] = { 0xFF, 0xFF, 0xFF },
  [TELA_SELECIONADA] = { 0x80, 0xFF, 0x80 },
  [TELA_TITULO] = { 0xA0, 0xC8, 0xFF },
  [TELA_APAGADA] = { 0x90, 0x90, 0x90 },
  [TELA_AVISO] = { 0xFF, 0xE0, 0x60 },
};
static const uint8_t rgb_fundo[3] = { 0x18, 0x18, 0x20 };
static const uint8_t rgb_moldura[3] = { 0x60, 0x70, 0x90 };

static bool pixel_da_fonte(unsigned char c, int gx, int gy)
{
  if (c < FONT_FIRST || c >= FONT_FIRST + FONT_COUNT)
    return false;
  return (font_8x8[c - FONT_FIRST][gy] >> (7 - gx)) & 1;
}

/* Desenha `c` em mem_pixels (PAINEL_W x PAINEL_H na escala atual). */
static void rasterizar(const tela_conteudo_t *c)
{
  int w = PAINEL_W(escala), h = PAINEL_H(escala);
  uint64_t fundo = codificar(rgb_fundo), moldura = codificar(rgb_moldura), cores[TELA_CORES];
  for (int i = 0; i < TELA_CORES; i++)
    cores[i] = codificar(rgb_cor[i]);
  int tam_linha[TELA_LINHAS];
  for (int i = 0; i < TELA_LINHAS; i++) {
    int k = 0;
    while (k < TELA_COLUNAS && c->texto[i][k])
      k++;
    tam_linha[i] = k;
  }
  for (int y = 0; y < h; y++) {
    int fy = y / escala;   /* em pixels de fonte */
    int linha = (fy - BORDA) / CELULA_H, gy = (fy - BORDA) % CELULA_H;
    bool dentro_y = fy >= BORDA && linha < c->linhas && gy < FONT_H;
    uint8_t *dst = mem_pixels + (size_t)y * w * bytes_px;
    for (int x = 0; x < w; x++) {
      int fx = x / escala;
      uint64_t px = fundo;
      if (x < escala || y < escala || x >= w - escala || y >= h - escala) {
        px = moldura;
      } else if (dentro_y && fx >= BORDA) {
        int col = (fx - BORDA) / FONT_W, gx = (fx - BORDA) % FONT_W;
        if (col < tam_linha[linha]) {
          unsigned char ch = (unsigned char)c->texto[linha][col];
          if (pixel_da_fonte(ch, gx, gy)) {
            uint8_t cor = c->cor[linha] < TELA_CORES ? c->cor[linha] : TELA_NORMAL;
            px = cores[cor];
          }
        }
      }
      if (bytes_px == 8) {
        __builtin_memcpy(dst + (size_t)x * 8, &px, 8);
      } else {
        uint32_t p32 = (uint32_t)px;
        __builtin_memcpy(dst + (size_t)x * 4, &p32, 4);
      }
    }
  }
}

/* A parte da textura que aparece (o Lichii pode renderizar maior e cortar). */
static nvn_rect_t area_visivel(const nvn_window_t *window, int tw, int th)
{
  nvn_rect_t crop = { 0, 0, 0, 0 };
  if (window)
    fn.window_crop(window, &crop);
  if (crop.w <= 0 || crop.h <= 0 || crop.x < 0 || crop.y < 0 || crop.x + crop.w > tw ||
      crop.y + crop.h > th)
    crop = (nvn_rect_t){ 0, 0, tw, th };
  return crop;
}

static void desenhar(nvn_queue_t *queue, nvn_window_t *window, int index)
{
  if (!tela_visivel() || estado == T_FALHOU)
    return;
  if (index < 0 || index >= n_texturas)
    return;
  if (estado == T_NADA && !iniciar())
    return;
  nvn_texture_t *tex = texturas[index];
  if (fn.tex_format(tex) != formato)
    return;
  int tw = fn.tex_width(tex), th = fn.tex_height(tex);
  nvn_rect_t area = area_visivel(window, tw, th);
  int w = PAINEL_W(escala), h = PAINEL_H(escala);
  if (w > area.w || h > area.h)
    return;

  /* a cópia anterior tem de ter terminado antes de reusar a memória de comandos */
  if (copia_pendente && fn.sync_wait(&sync_copia, ESPERA_QUADRO_NS) >= SYNC_TIMEOUT_EXPIRED)
    return;   /* tenta no próximo quadro, sem segurar o jogo */
  copia_pendente = false;

  /* redesenha só quando o conteúdo mudou; a GPU já terminou de copiar o buffer (acima) */
  static tela_conteudo_t c;   /* (estático: só esta thread, a do present, usa) */
  travar();
  uint32_t v = versao_publicada;
  bool mudou = v != versao_desenhada;
  if (mudou)
    c = publicado;
  destravar();
  if (mudou) {
    rasterizar(&c);
    versao_desenhada = v;
    __asm__ volatile("dsb sy" ::: "memory");   /* os pixels chegam à memória antes da cópia */
  }

  nvn_region_t regiao = { area.x + (area.w - w) / 2, area.y + area.h / 5, 0, w, h, 1 };
  fn.cmd_add_command(&cmd, &pool_cmd, 0, CMD_TAM);
  fn.cmd_add_control(&cmd, mem_ctrl, CTRL_TAM);
  fn.cmd_begin(&cmd);
  fn.cmd_copy_to_texture(&cmd, fn.pool_address(&pool_pixels), tex, 0, &regiao, 0);
  nvn_handle_t handle = fn.cmd_end(&cmd);
  fn.queue_submit(queue, 1, &handle);
  fn.queue_fence(queue, &sync_copia, SYNC_ALL_GPU_COMMANDS_COMPLETE, 0);
  copia_pendente = true;

  static bool anotado;
  if (!anotado) {
    anotado = true;
    log_line_t l;
    log_begin(&l);
    log_str(&l, "tela: primeiro painel desenhado em ");
    log_dec(&l, regiao.x);
    log_str(&l, ",");
    log_dec(&l, regiao.y);
    log_str(&l, " (");
    log_dec(&l, w);
    log_str(&l, "x");
    log_dec(&l, h);
    log_str(&l, ") numa area de ");
    log_dec(&l, area.w);
    log_str(&l, "x");
    log_dec(&l, area.h);
    log_end(&l);
  }
}
