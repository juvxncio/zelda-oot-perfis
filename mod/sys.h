/* Chamadas de sistema do Horizon e log. Sem nenhuma chamada ao sdk: tudo aqui pode rodar
 * dentro do DT_INIT, antes de o jogo começar. */
#ifndef SYS_H
#define SYS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
  uint64_t addr;
  uint64_t size;
  uint32_t type;
  uint32_t attr;
  uint32_t perm;   /* bit 0 R, 1 W, 2 X */
  uint32_t ipc_refcount;
  uint32_t device_refcount;
  uint32_t padding;
} sys_meminfo_t;

#define SYS_PERM_R 1u
#define SYS_PERM_W 2u
#define SYS_PERM_X 4u

/* svcQueryMemory: a região que contém addr. Devolve o Result (0 = ok). */
uint32_t sys_query_memory(sys_meminfo_t *info, uint64_t addr);

/* True se [addr, addr + size) está mapeado com pelo menos `perm`. */
bool sys_mapped(uintptr_t addr, size_t size, uint32_t perm);

/* Escreve uma linha no log do emulador (svcOutputDebugString), com o prefixo "[perfis] ".
 * As funções log_* montam a linha num buffer local: podem ser chamadas de qualquer thread. */
typedef struct {
  char buf[512];
  size_t len;
} log_line_t;

void log_begin(log_line_t *l);
void log_str(log_line_t *l, const char *s);
void log_hex(log_line_t *l, uint64_t v);
void log_dec(log_line_t *l, int64_t v);
void log_end(log_line_t *l);

/* Atalho para uma linha só com texto. */
void log_raw(const char *s);

#endif
