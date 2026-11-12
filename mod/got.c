/* Adaptado de sloperina (src/ootgz/got.c, GPL-3.0). */
#include "got.h"

#include <stdbool.h>

#include "sys.h"

/* Tags da seção dynamic do ELF */
enum { DT_NULL = 0, DT_PLTRELSZ = 2, DT_STRTAB = 5, DT_SYMTAB = 6, DT_RELA = 7, DT_RELASZ = 8,
       DT_JMPREL = 23 };

/* Tipos de relocação AArch64 que preenchem uma entrada da GOT com o endereço de um símbolo */
enum { R_AARCH64_ABS64 = 257, R_AARCH64_GLOB_DAT = 1025, R_AARCH64_JUMP_SLOT = 1026 };

#define MOD0_MAGIC          0x30444F4D /* "MOD0" */
#define MOD0_OFFSET_FIELD   4          /* module+4: offset do cabeçalho MOD0 */
#define MOD0_DYNAMIC_FIELD  4          /* MOD0+4: offset da seção dynamic, a partir do MOD0 */
#define MAX_DYNAMIC_ENTRIES 256        /* para se o DT_NULL nunca vier */
#define SHN_UNDEF           0          /* símbolo importado: definido em outro módulo */

typedef struct { int64_t tag; uint64_t val; } dyn_t;
typedef struct { uint32_t name; uint8_t info, other; uint16_t shndx; uint64_t value, size; } sym_t;
typedef struct { uint64_t offset, info; int64_t addend; } rela_t;

static bool same_string(const char *a, const char *b)
{
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}

uintptr_t *got_slot(uintptr_t module, const char *name)
{
  uintptr_t mod0 = module + *(const uint32_t *)(module + MOD0_OFFSET_FIELD);
  if (*(const uint32_t *)mod0 != MOD0_MAGIC)
    return 0;
  const dyn_t *dynamic = (const dyn_t *)(mod0 + *(const int32_t *)(mod0 + MOD0_DYNAMIC_FIELD));
  uint64_t symtab = 0, strtab = 0, rela = 0, relasz = 0, jmprel = 0, pltrelsz = 0;
  for (int i = 0; i < MAX_DYNAMIC_ENTRIES && dynamic[i].tag != DT_NULL; i++) {
    /* os endereços podem já estar relocados ou ainda ser offsets: vira offset nos dois casos */
    uint64_t offset = dynamic[i].val >= module ? dynamic[i].val - module : dynamic[i].val;
    switch (dynamic[i].tag) {
      case DT_SYMTAB: symtab = offset; break;
      case DT_STRTAB: strtab = offset; break;
      case DT_RELA: rela = offset; break;
      case DT_RELASZ: relasz = dynamic[i].val; break;
      case DT_JMPREL: jmprel = offset; break;
      case DT_PLTRELSZ: pltrelsz = dynamic[i].val; break;
    }
  }
  if (!symtab || !strtab)
    return 0;

  /* primeiro as relocações do PLT, depois as outras */
  const struct { uint64_t at, size; } tables[] = { { jmprel, pltrelsz }, { rela, relasz } };
  for (int t = 0; t < 2; t++) {
    if (!tables[t].at)
      continue;
    const rela_t *relocs = (const rela_t *)(module + tables[t].at);
    for (uint64_t i = 0; i < tables[t].size / sizeof(rela_t); i++) {
      uint32_t type = (uint32_t)relocs[i].info;
      uint32_t symbol_index = (uint32_t)(relocs[i].info >> 32);
      if (!symbol_index || (type != R_AARCH64_GLOB_DAT && type != R_AARCH64_JUMP_SLOT &&
                            type != R_AARCH64_ABS64))
        continue;
      const sym_t *symbol = (const sym_t *)(module + symtab) + symbol_index;
      if (symbol->shndx == SHN_UNDEF &&
          same_string((const char *)(module + strtab + symbol->name), name))
        return (uintptr_t *)(module + relocs[i].offset);
    }
  }
  return 0;
}

/* True se addr é código numa região mapeada que não contém o início de `module`. */
static bool code_outside(uintptr_t addr, uintptr_t module)
{
  sys_meminfo_t region;
  if (sys_query_memory(&region, addr) != 0 || !(region.perm & SYS_PERM_X))
    return false;
  return module < region.addr || module - region.addr >= region.size;
}

const char *got_hook(uintptr_t module, const char *name, void *replacement, void *function,
                     void **next)
{
  uintptr_t *slot = got_slot(module, name);
  if (!slot)
    return "import nao encontrado";
  if (!sys_mapped((uintptr_t)slot, 8, SYS_PERM_R | SYS_PERM_W))
    return "a GOT esta somente leitura";
  uintptr_t old = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
  void *target = code_outside(old, module) ? (void *)old : function;
  if (!target)
    return "o import ainda nao foi ligado e este modulo nao tem definicao para chamar";
  *next = target;
  __atomic_store_n(slot, (uintptr_t)replacement, __ATOMIC_RELEASE);
  return 0;
}
