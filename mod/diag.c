/* Diagnóstico: registra no log do emulador cada chamada de arquivo do jogo, sem mudar nada.
 *
 * Cada substituto chama a função original com os mesmos argumentos, anota o caminho e o
 * Result, e devolve ao jogo exatamente o que a original devolveu. Os arquivos do romfs (os
 * pontos de montagem passados a MountRom) não são anotados, para o log não encher. */
#include "diag.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "got.h"
#include "sys.h"

typedef uint32_t result_t;   /* nn::Result: um uint32, devolvido em w0 */

/* DirectoryEntry do nn::fs: nome (0x301), atributos, tipo e tamanho; 0x310 bytes. */
typedef struct {
  char name[0x301];
  uint8_t attributes;
  uint8_t pad0[2];
  int8_t type;   /* 0 = pasta, 1 = arquivo */
  uint8_t pad1[3];
  int64_t size;
} dir_entry_t;
_Static_assert(sizeof(dir_entry_t) == 0x310, "DirectoryEntry tem 0x310 bytes");

/* --- as funções originais, importadas do sdk ----------------------------------------- */

#define FS(name) __asm__(name)
result_t fs_mount_rom_cache(const char *, void *, size_t)      FS("_ZN2nn2fs8MountRomEPKcPvm");
result_t fs_mount_rom(const char *)                            FS("_ZN2nn2fs8MountRomEPKc");
result_t fs_mount_save(const char *, const void *uid)          FS("_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE");
result_t fs_mount_temp(const char *)                           FS("_ZN2nn2fs21MountTemporaryStorageEPKc");
void     fs_unmount(const char *)                              FS("_ZN2nn2fs7UnmountEPKc");
result_t fs_open_file(void *handle, const char *, int mode)    FS("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci");
result_t fs_create_file(const char *, int64_t size)            FS("_ZN2nn2fs10CreateFileEPKcl");
result_t fs_delete_file(const char *)                          FS("_ZN2nn2fs10DeleteFileEPKc");
result_t fs_entry_type(int *out, const char *)                 FS("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc");
result_t fs_open_dir(uint64_t *handle, const char *, int mode) FS("_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci");
result_t fs_read_dir(int64_t *count, dir_entry_t *, uint64_t handle, int64_t max) FS("_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl");
result_t fs_create_dir(const char *)                           FS("_ZN2nn2fs15CreateDirectoryEPKc");
result_t fs_delete_dir_rec(const char *)                       FS("_ZN2nn2fs26DeleteDirectoryRecursivelyEPKc");
result_t fs_commit(const char *)                               FS("_ZN2nn2fs6CommitEPKc");

/* o que cada substituto chama (preenchido por got_hook) */
static result_t (*next_mount_rom_cache)(const char *, void *, size_t);
static result_t (*next_mount_rom)(const char *);
static result_t (*next_mount_save)(const char *, const void *);
static result_t (*next_mount_temp)(const char *);
static void     (*next_unmount)(const char *);
static result_t (*next_open_file)(void *, const char *, int);
static result_t (*next_create_file)(const char *, int64_t);
static result_t (*next_delete_file)(const char *);
static result_t (*next_entry_type)(int *, const char *);
static result_t (*next_open_dir)(uint64_t *, const char *, int);
static result_t (*next_read_dir)(int64_t *, dir_entry_t *, uint64_t, int64_t);
static result_t (*next_create_dir)(const char *);
static result_t (*next_delete_dir_rec)(const char *);
static result_t (*next_commit)(const char *);

/* --- pontos de montagem do romfs, para não anotar seus arquivos ----------------------- */

#define MAX_ROM_MOUNTS 4
#define MOUNT_NAME_LEN 32
static char rom_mounts[MAX_ROM_MOUNTS][MOUNT_NAME_LEN];
static uint32_t rom_mount_count;

static void remember_rom_mount(const char *name)
{
  uint32_t i = __atomic_load_n(&rom_mount_count, __ATOMIC_ACQUIRE);
  if (!name || i >= MAX_ROM_MOUNTS)
    return;
  size_t k = 0;
  for (; name[k] && k < MOUNT_NAME_LEN - 1; k++)
    rom_mounts[i][k] = name[k];
  rom_mounts[i][k] = 0;
  __atomic_store_n(&rom_mount_count, i + 1, __ATOMIC_RELEASE);
}

/* True se `path` é "<montagem do romfs>:..." */
static bool is_rom_path(const char *path)
{
  if (!path)
    return false;
  uint32_t n = __atomic_load_n(&rom_mount_count, __ATOMIC_ACQUIRE);
  for (uint32_t i = 0; i < n; i++) {
    const char *m = rom_mounts[i], *p = path;
    while (*m && *m == *p) {
      m++;
      p++;
    }
    if (!*m && *p == ':')
      return true;
  }
  return false;
}

/* Pastas fora do romfs abertas por último: só a listagem delas é anotada. */
#define WATCHED_DIRS 16
static uint64_t watched_dirs[WATCHED_DIRS];
static uint32_t watched_next;

static void watch_dir(uint64_t handle)
{
  uint32_t i = __atomic_fetch_add(&watched_next, 1, __ATOMIC_RELAXED) % WATCHED_DIRS;
  __atomic_store_n(&watched_dirs[i], handle, __ATOMIC_RELEASE);
}

static bool is_watched_dir(uint64_t handle)
{
  for (int i = 0; i < WATCHED_DIRS; i++)
    if (handle && __atomic_load_n(&watched_dirs[i], __ATOMIC_ACQUIRE) == handle)
      return true;
  return false;
}

/* Limite de linhas, por segurança, caso o filtro do romfs não pegue algo. */
#define MAX_LOG_LINES 4000
static uint32_t log_lines;

static bool log_allowed(void)
{
  uint32_t n = __atomic_fetch_add(&log_lines, 1, __ATOMIC_RELAXED);
  if (n == MAX_LOG_LINES)
    log_raw("limite de linhas do diagnostico atingido: o resto nao sera anotado");
  return n < MAX_LOG_LINES;
}

/* "<op> <caminho> -> <result>" */
static void log_op(const char *op, const char *path, result_t r)
{
  if (is_rom_path(path) || !log_allowed())
    return;
  log_line_t l;
  log_begin(&l);
  log_str(&l, op);
  log_str(&l, " ");
  log_str(&l, path);
  log_str(&l, " -> ");
  log_hex(&l, r);
  log_end(&l);
}

/* --- substitutos ------------------------------------------------------------------- */

static result_t hook_mount_rom_cache(const char *name, void *cache, size_t size)
{
  result_t r = next_mount_rom_cache(name, cache, size);
  remember_rom_mount(name);
  log_op("MountRom", name, r);
  return r;
}

static result_t hook_mount_rom(const char *name)
{
  result_t r = next_mount_rom(name);
  remember_rom_mount(name);
  log_op("MountRom", name, r);
  return r;
}

static result_t hook_mount_save(const char *name, const void *uid)
{
  result_t r = next_mount_save(name, uid);
  if (log_allowed()) {
    const uint64_t *u = (const uint64_t *)uid;
    log_line_t l;
    log_begin(&l);
    log_str(&l, "MountSaveData ");
    log_str(&l, name);
    log_str(&l, " uid=");
    log_hex(&l, u ? u[0] : 0);
    log_str(&l, ",");
    log_hex(&l, u ? u[1] : 0);
    log_str(&l, " -> ");
    log_hex(&l, r);
    log_end(&l);
  }
  return r;
}

static result_t hook_mount_temp(const char *name)
{
  result_t r = next_mount_temp(name);
  log_op("MountTemporaryStorage", name, r);
  return r;
}

static void hook_unmount(const char *name)
{
  next_unmount(name);
  log_op("Unmount", name, 0);
}

static result_t hook_open_file(void *handle, const char *path, int mode)
{
  result_t r = next_open_file(handle, path, mode);
  if (!is_rom_path(path) && log_allowed()) {
    log_line_t l;
    log_begin(&l);
    log_str(&l, "OpenFile ");
    log_str(&l, path);
    log_str(&l, " modo=");
    log_dec(&l, mode);
    log_str(&l, " -> ");
    log_hex(&l, r);
    log_end(&l);
  }
  return r;
}

static result_t hook_create_file(const char *path, int64_t size)
{
  result_t r = next_create_file(path, size);
  if (!is_rom_path(path) && log_allowed()) {
    log_line_t l;
    log_begin(&l);
    log_str(&l, "CreateFile ");
    log_str(&l, path);
    log_str(&l, " tamanho=");
    log_dec(&l, size);
    log_str(&l, " -> ");
    log_hex(&l, r);
    log_end(&l);
  }
  return r;
}

static result_t hook_delete_file(const char *path)
{
  result_t r = next_delete_file(path);
  log_op("DeleteFile", path, r);
  return r;
}

static result_t hook_entry_type(int *out, const char *path)
{
  result_t r = next_entry_type(out, path);
  if (!is_rom_path(path) && log_allowed()) {
    log_line_t l;
    log_begin(&l);
    log_str(&l, "GetEntryType ");
    log_str(&l, path);
    log_str(&l, " tipo=");
    log_dec(&l, (r == 0 && out) ? *out : -1);
    log_str(&l, " -> ");
    log_hex(&l, r);
    log_end(&l);
  }
  return r;
}

static result_t hook_open_dir(uint64_t *handle, const char *path, int mode)
{
  result_t r = next_open_dir(handle, path, mode);
  if (!is_rom_path(path) && r == 0 && handle)
    watch_dir(*handle);
  if (!is_rom_path(path) && log_allowed()) {
    log_line_t l;
    log_begin(&l);
    log_str(&l, "OpenDirectory ");
    log_str(&l, path);
    log_str(&l, " modo=");
    log_dec(&l, mode);
    log_str(&l, " handle=");
    log_hex(&l, (r == 0 && handle) ? *handle : 0);
    log_str(&l, " -> ");
    log_hex(&l, r);
    log_end(&l);
  }
  return r;
}

static result_t hook_read_dir(int64_t *count, dir_entry_t *entries, uint64_t handle, int64_t max)
{
  result_t r = next_read_dir(count, entries, handle, max);
  if (!is_watched_dir(handle) || !log_allowed())
    return r;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "ReadDirectory handle=");
  log_hex(&l, handle);
  log_str(&l, " max=");
  log_dec(&l, max);
  log_str(&l, " lidos=");
  int64_t n = (r == 0 && count) ? *count : -1;
  log_dec(&l, n);
  log_str(&l, " -> ");
  log_hex(&l, r);
  log_end(&l);
  for (int64_t i = 0; entries && i < n && i < 64 && log_allowed(); i++) {
    log_begin(&l);
    log_str(&l, "  ");
    log_str(&l, entries[i].type == 0 ? "[pasta] " : "[arq]   ");
    log_strn(&l, entries[i].name, sizeof(entries[i].name));
    log_str(&l, " ");
    log_dec(&l, entries[i].size);
    log_end(&l);
  }
  return r;
}

static result_t hook_create_dir(const char *path)
{
  result_t r = next_create_dir(path);
  log_op("CreateDirectory", path, r);
  return r;
}

static result_t hook_delete_dir_rec(const char *path)
{
  result_t r = next_delete_dir_rec(path);
  log_op("DeleteDirectoryRecursively", path, r);
  return r;
}

static result_t hook_commit(const char *mount)
{
  result_t r = next_commit(mount);
  log_op("Commit", mount, r);
  return r;
}

/* --- instalação -------------------------------------------------------------------- */

/* Os endereços das funções do sdk são tomados dentro do código (não numa tabela estática),
 * para virarem relocações GLOB_DAT, as que o rtld deste jogo comprovadamente resolve, e não
 * ABS64. */
static int install_one(uintptr_t main_base, const char *name, void *replacement, void *function,
                       void **next)
{
  const char *problem = got_hook(main_base, name, replacement, function, next);
  if (!problem)
    return 1;
  log_line_t l;
  log_begin(&l);
  log_str(&l, "nao interceptado: ");
  log_str(&l, name);
  log_str(&l, " (");
  log_str(&l, problem);
  log_str(&l, ")");
  log_end(&l);
  return 0;
}

#define INSTALL(sym, hook, fn, nxt) \
  (total++, ok += install_one(main_base, sym, (void *)(hook), (void *)(fn), (void **)&(nxt)))

void diag_install(uintptr_t main_base)
{
  int ok = 0, total = 0;
  INSTALL("_ZN2nn2fs8MountRomEPKcPvm", hook_mount_rom_cache, fs_mount_rom_cache, next_mount_rom_cache);
  INSTALL("_ZN2nn2fs8MountRomEPKc", hook_mount_rom, fs_mount_rom, next_mount_rom);
  INSTALL("_ZN2nn2fs13MountSaveDataEPKcRKNS_7account3UidE", hook_mount_save, fs_mount_save, next_mount_save);
  INSTALL("_ZN2nn2fs21MountTemporaryStorageEPKc", hook_mount_temp, fs_mount_temp, next_mount_temp);
  INSTALL("_ZN2nn2fs7UnmountEPKc", hook_unmount, fs_unmount, next_unmount);
  INSTALL("_ZN2nn2fs8OpenFileEPNS0_10FileHandleEPKci", hook_open_file, fs_open_file, next_open_file);
  INSTALL("_ZN2nn2fs10CreateFileEPKcl", hook_create_file, fs_create_file, next_create_file);
  INSTALL("_ZN2nn2fs10DeleteFileEPKc", hook_delete_file, fs_delete_file, next_delete_file);
  INSTALL("_ZN2nn2fs12GetEntryTypeEPNS0_18DirectoryEntryTypeEPKc", hook_entry_type, fs_entry_type, next_entry_type);
  INSTALL("_ZN2nn2fs13OpenDirectoryEPNS0_15DirectoryHandleEPKci", hook_open_dir, fs_open_dir, next_open_dir);
  INSTALL("_ZN2nn2fs13ReadDirectoryEPlPNS0_14DirectoryEntryENS0_15DirectoryHandleEl", hook_read_dir, fs_read_dir, next_read_dir);
  INSTALL("_ZN2nn2fs15CreateDirectoryEPKc", hook_create_dir, fs_create_dir, next_create_dir);
  INSTALL("_ZN2nn2fs26DeleteDirectoryRecursivelyEPKc", hook_delete_dir_rec, fs_delete_dir_rec, next_delete_dir_rec);
  INSTALL("_ZN2nn2fs6CommitEPKc", hook_commit, fs_commit, next_commit);

  log_line_t l;
  log_begin(&l);
  log_str(&l, "diagnostico ativo: ");
  log_dec(&l, ok);
  log_str(&l, " de ");
  log_dec(&l, total);
  log_str(&l, " funcoes interceptadas (so registro, nada e alterado)");
  log_end(&l);
}
