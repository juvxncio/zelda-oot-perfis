/* Cabeçalho de módulo Horizon para um NSO carregado como subsdkN.
 * Adaptado de sloperina (src/ootgz/crt0.S, GPL-3.0).
 *
 * O rtld acha o MOD0 pela palavra no offset 4 do módulo, reloca o módulo, liga os imports
 * aos outros módulos (main, subsdk*, sdk) e chama o DT_INIT (modulo_init, definido no
 * Makefile). A última palavra do MOD0 aponta para um espaço no bss que o rtld usa para o
 * próprio objeto do módulo: o Switch 1 precisa de 0xD0 bytes; reservamos 0x200, como o
 * sloperina, caso o objeto do rtld do Switch 2 seja maior.
 */
.section .text.crt0, "ax", %progbits
.global __module_start
.hidden __module_start
__module_start:
  .word   0
  .word   __nx_mod0 - __module_start  /* offset 4: onde o rtld acha o MOD0 */
  .ascii  "PERFIS\0\0"

.section .rodata.mod0, "a", %progbits
.align  2
.global __nx_mod0
.hidden __nx_mod0
__nx_mod0:
  .ascii  "MOD0"
  .word   __dynamic_start__       - __nx_mod0
  .word   __bss_start__           - __nx_mod0
  .word   __bss_end__             - __nx_mod0
  .word   __eh_frame_hdr_start__  - __nx_mod0
  .word   __eh_frame_hdr_end__    - __nx_mod0
  .word   __nx_module_runtime     - __nx_mod0

.section .bss.module_runtime, "aw", %nobits
.align  3
.global __nx_module_runtime
.hidden __nx_module_runtime
__nx_module_runtime:
  .space  0x200
