#include "sys.h"

#define LOG_PREFIX "[perfis] "
#define NULL_PAGE_END 0x1000

/* --- chamadas de sistema (svc) ---------------------------------------------------------- */

uint32_t sys_query_memory(sys_meminfo_t *info, uint64_t addr)
{
  register uint64_t x0 __asm__("x0") = (uint64_t)info;
  register uint64_t x1 __asm__("x1");
  register uint64_t x2 __asm__("x2") = addr;
  __asm__ volatile("svc 0x6" : "+r"(x0), "=r"(x1), "+r"(x2) : : "memory", "x3", "x4", "x5",
                   "x6", "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16",
                   "x17", "x18");
  return (uint32_t)x0;
}

static void sys_output_debug_string(const char *text, size_t len)
{
  register uint64_t x0 __asm__("x0") = (uint64_t)text;
  register uint64_t x1 __asm__("x1") = len;
  __asm__ volatile("svc 0x27" : "+r"(x0), "+r"(x1) : : "memory", "x2", "x3", "x4", "x5", "x6",
                   "x7", "x8", "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17",
                   "x18");
}

bool sys_mapped(uintptr_t addr, size_t size, uint32_t perm)
{
  if (addr < NULL_PAGE_END || addr + size < addr)
    return false;
  uintptr_t end = addr + size;
  while (addr < end) {
    sys_meminfo_t region;
    if (sys_query_memory(&region, addr) != 0 || (region.perm & perm) != perm || !region.size)
      return false;
    addr = region.addr + region.size;
  }
  return true;
}

/* --- log ------------------------------------------------------------------------------ */

void log_begin(log_line_t *l)
{
  l->len = 0;
  log_str(l, LOG_PREFIX);
}

void log_str(log_line_t *l, const char *s)
{
  if (!s)
    s = "(null)";
  while (*s && l->len < sizeof(l->buf) - 1)
    l->buf[l->len++] = *s++;
}

void log_hex(log_line_t *l, uint64_t v)
{
  char tmp[19];
  int i = sizeof(tmp);
  do {
    tmp[--i] = "0123456789abcdef"[v & 15];
    v >>= 4;
  } while (v);
  tmp[--i] = 'x';
  tmp[--i] = '0';
  while (i < (int)sizeof(tmp) && l->len < sizeof(l->buf) - 1)
    l->buf[l->len++] = tmp[i++];
}

void log_dec(log_line_t *l, int64_t v)
{
  char tmp[21];
  int i = sizeof(tmp);
  uint64_t u = v < 0 ? 0 - (uint64_t)v : (uint64_t)v;
  do {
    tmp[--i] = (char)('0' + u % 10);
    u /= 10;
  } while (u);
  if (v < 0)
    tmp[--i] = '-';
  while (i < (int)sizeof(tmp) && l->len < sizeof(l->buf) - 1)
    l->buf[l->len++] = tmp[i++];
}

void log_end(log_line_t *l)
{
  l->buf[l->len] = 0;
  sys_output_debug_string(l->buf, l->len);
}

void log_raw(const char *s)
{
  log_line_t l;
  log_begin(&l);
  log_str(&l, s);
  log_end(&l);
}
