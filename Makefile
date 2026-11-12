# zelda-oot-perfis: o módulo subsdk9 do mod.
#
#   make          compila build/subsdk9
#   make dist     monta dist/load/0400486024DEC000/zelda-oot-perfis/exefs/subsdk9
#   make test     compila e roda no PC os testes da lógica de perfis (HOSTCC=...)
#   make clean
#
# Precisa do devkitA64 (devkitPro, opção "Switch Development"). Rode pelo terminal
# "MSYS2" que o devkitPro instala, ou com DEVKITPRO definido.

DEVKITPRO   ?= /opt/devkitpro
TOOLS        = $(DEVKITPRO)/devkitA64/bin/aarch64-none-elf-
CC           = $(TOOLS)gcc
READELF      = $(TOOLS)readelf
ELF2NSO      = $(DEVKITPRO)/tools/bin/elf2nso

PROGRAM_ID   = 0400486024DEC000
NOME         = zelda-oot-perfis
SUBSDK       = subsdk9

SRCDIR       = mod
LDSCRIPT     = $(SRCDIR)/modulo.ld
IMPORTS      = $(SRCDIR)/imports.txt
OBJDIR       = build/obj
BINDIR       = build
DISTDIR      = dist/load/$(PROGRAM_ID)/$(NOME)/exefs

ARCHFLAGS    = -march=armv8-a+crc+crypto -mtune=cortex-a78 -mtp=soft -fPIC
CFLAGS       = -std=gnu11 -O2 -g -Wall -Wextra -Werror -Wno-unused-parameter $(ARCHFLAGS) \
               -fvisibility=hidden -ffunction-sections -fdata-sections \
               -fno-asynchronous-unwind-tables -fno-unwind-tables \
               -fno-stack-protector -fno-plt -fno-common -ffreestanding
ASFLAGS      = $(ARCHFLAGS)
LDFLAGS      = -shared -nostdlib -nostartfiles -T $(LDSCRIPT) \
               -Wl,--gc-sections -Wl,-z,text -Wl,-z,max-page-size=0x1000 \
               -Wl,--hash-style=sysv -Wl,-Bsymbolic -Wl,--build-id=sha1 \
               -Wl,-init=modulo_init -Wl,--exclude-libs=ALL
LDLIBS       = -lgcc

CSRC         = $(wildcard $(SRCDIR)/*.c)
SSRC         = $(wildcard $(SRCDIR)/*.S)
HDRS         = $(wildcard $(SRCDIR)/*.h)
OBJS         = $(patsubst $(SRCDIR)/%,$(OBJDIR)/%.o,$(basename $(SSRC) $(CSRC)))

all: $(BINDIR)/$(SUBSDK)

$(OBJDIR)/%.o: $(SRCDIR)/%.c $(HDRS) | $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
$(OBJDIR)/%.o: $(SRCDIR)/%.S | $(OBJDIR)
	$(CC) $(ASFLAGS) -c $< -o $@

$(BINDIR)/modulo.elf: $(OBJS) $(LDSCRIPT) $(IMPORTS) | $(BINDIR)
	$(CC) $(ARCHFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)
	@# Recusa símbolos indefinidos que o rtld do jogo não conseguiria resolver.
	@missing=$$($(READELF) --dyn-syms -W $@ | awk '$$7=="UND" && $$8!="" {print $$8}' | LC_ALL=C sort -u | LC_ALL=C comm -23 - $(IMPORTS)); \
	if [ -n "$$missing" ]; then echo "erro: import desconhecido (veja $(IMPORTS)):"; echo "$$missing"; rm -f $@; exit 1; fi

$(BINDIR)/$(SUBSDK): $(BINDIR)/modulo.elf
	$(ELF2NSO) $< $@

dist: $(BINDIR)/$(SUBSDK)
	mkdir -p $(DISTDIR)
	cp $< $(DISTDIR)/$(SUBSDK)

$(OBJDIR) $(BINDIR):
	mkdir -p $@

# Testes no PC: só a lógica pura (mod/perfis.c), sem nada do Switch.
HOSTCC      ?= cc
$(BINDIR)/test_perfis.exe: tests/test_perfis.c $(SRCDIR)/perfis.c $(SRCDIR)/perfis.h | $(BINDIR)
	$(HOSTCC) -std=gnu11 -O2 -Wall -Wextra -Werror -o $@ tests/test_perfis.c $(SRCDIR)/perfis.c
test: $(BINDIR)/test_perfis.exe
	$<

clean:
	rm -rf build dist

.PHONY: all dist test clean
