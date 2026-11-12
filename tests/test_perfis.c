/* Testes da lógica de perfis (mod/perfis.c), compilados e rodados no PC: make test */
#include <stdio.h>
#include <string.h>

#include "../mod/perfis.h"

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

static void caso(int perfil, int bloqueado, const char *entrada, perfis_acao_t esperado,
                 const char *saida)
{
  char buf[PERFIS_CAMINHO_MAX];
  memset(buf, 'X', sizeof(buf));
  perfis_acao_t a = perfis_reescrever(perfil, bloqueado, entrada, buf, sizeof(buf));
  CHECA(a == esperado, "perfil %d, \"%s\": acao %d, esperava %d", perfil,
        entrada ? entrada : "(null)", a, esperado);
  if (a == PERFIS_REESCRITO && saida)
    CHECA(strcmp(buf, saida) == 0, "\"%s\" -> \"%s\", esperava \"%s\"", entrada, buf, saida);
}

int main(void)
{
  /* Perfil 1: nada muda, nunca. */
  caso(1, 0, "save:/slot_00/progress.sav", PERFIS_ORIGINAL, 0);
  caso(1, 0, "save:/option.sav", PERFIS_ORIGINAL, 0);

  /* Perfil 2 em diante: tudo em save: vai para save:/perfilN. */
  caso(2, 0, "save:/slot_00/progress.sav", PERFIS_REESCRITO, "save:/perfil2/slot_00/progress.sav");
  caso(2, 0, "save:/slot_11/caption.sav", PERFIS_REESCRITO, "save:/perfil2/slot_11/caption.sav");
  caso(2, 0, "save:/account.sav", PERFIS_REESCRITO, "save:/perfil2/account.sav");
  caso(2, 0, "save:/option.sav", PERFIS_REESCRITO, "save:/perfil2/option.sav");
  caso(2, 0, "save:/slot_05", PERFIS_REESCRITO, "save:/perfil2/slot_05");
  caso(9, 0, "save:/slot_00/progress.sav", PERFIS_REESCRITO, "save:/perfil9/slot_00/progress.sav");
  caso(3, 0, "save:/", PERFIS_REESCRITO, "save:/perfil3/");
  caso(3, 0, "save:", PERFIS_REESCRITO, "save:/perfil3");

  /* O arquivo de controle fica sempre no lugar original. */
  caso(2, 0, PERFIS_ARQUIVO, PERFIS_ORIGINAL, 0);
  caso(2, 1, PERFIS_ARQUIVO, PERFIS_ORIGINAL, 0);

  /* Outras montagens não são tocadas (romfs, temporário, nomes parecidos). */
  caso(2, 0, "content:/Pack/Actor.pack.zs", PERFIS_ORIGINAL, 0);
  caso(2, 0, "savt:/TemporaryGame.sav", PERFIS_ORIGINAL, 0);
  caso(2, 0, "saved:/x", PERFIS_ORIGINAL, 0);
  caso(2, 0, "save", PERFIS_ORIGINAL, 0);
  caso(2, 0, 0, PERFIS_ORIGINAL, 0);

  /* Bloqueado: nada em save: passa, em nenhum perfil; o resto segue normal. */
  caso(2, 1, "save:/slot_00/progress.sav", PERFIS_BLOQUEAR, 0);
  caso(1, 1, "save:/slot_00/progress.sav", PERFIS_BLOQUEAR, 0);
  caso(2, 1, "content:/x", PERFIS_ORIGINAL, 0);

  /* Perfil inválido: bloqueia em vez de usar o caminho original. */
  caso(0, 0, "save:/slot_00/progress.sav", PERFIS_BLOQUEAR, 0);
  caso(10, 0, "save:/slot_00/progress.sav", PERFIS_BLOQUEAR, 0);
  caso(-1, 0, "save:/slot_00/progress.sav", PERFIS_BLOQUEAR, 0);

  /* Caminho longo demais depois de reescrito: bloqueia (nunca cai no Perfil 1). */
  {
    char longo[PERFIS_CAMINHO_MAX];
    memcpy(longo, "save:/", 6);
    memset(longo + 6, 'a', sizeof(longo) - 6 - 1);
    longo[sizeof(longo) - 1] = 0;   /* 0x300 caracteres: cabe sozinho, não com "/perfil2" */
    caso(2, 0, longo, PERFIS_BLOQUEAR, 0);
    caso(1, 0, longo, PERFIS_ORIGINAL, 0);
  }

  /* Buffer pequeno: bloqueia. */
  {
    char pequeno[12];
    perfis_acao_t a = perfis_reescrever(2, 0, "save:/slot_00/progress.sav", pequeno, sizeof(pequeno));
    CHECA(a == PERFIS_BLOQUEAR, "buffer pequeno: acao %d", a);
  }

  /* Pasta do perfil */
  {
    char b[32];
    CHECA(perfis_pasta(2, b, sizeof(b)) && strcmp(b, "save:/perfil2") == 0, "pasta 2: %s", b);
    CHECA(!perfis_pasta(1, b, sizeof(b)), "perfil 1 nao tem pasta");
    CHECA(!perfis_pasta(10, b, sizeof(b)), "perfil 10 invalido");
    CHECA(!perfis_pasta(2, b, 5), "pasta nao cabe em 5 bytes");
  }

  /* Leitura do arquivo de controle */
  struct { const char *texto; int esperado; } numeros[] = {
    { "1", 1 }, { "2", 2 }, { "9", 9 }, { "2\n", 2 }, { "2\r\n", 2 }, { "  3  ", 3 },
    { "", 1 }, { "0", 1 }, { "10", 1 }, { "22", 1 }, { "a", 1 }, { "2a", 1 }, { "-2", 1 },
    { "\xEF\xBB\xBF" "2", 1 },   /* BOM do UTF-8: não aceito; o script grava sem BOM */
  };
  for (size_t i = 0; i < sizeof(numeros) / sizeof(*numeros); i++) {
    int n = perfis_ler_numero(numeros[i].texto, strlen(numeros[i].texto));
    CHECA(n == numeros[i].esperado, "ler \"%s\": %d, esperava %d", numeros[i].texto, n,
          numeros[i].esperado);
  }
  CHECA(perfis_ler_numero(0, 5) == 1, "texto nulo");

  printf("%d de %d verificacoes passaram\n", total - falhas, total);
  return falhas != 0;
}
