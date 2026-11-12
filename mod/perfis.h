/* Perfis de save: qual perfil está ativo e como os caminhos do save são reescritos.
 *
 * Só lógica pura (sem chamadas ao sistema), para poder ser testada no PC
 * (tests/test_perfis.c). Quem chama as funções de arquivo é o diag.c. */
#ifndef PERFIS_H
#define PERFIS_H

#include <stdbool.h>
#include <stddef.h>

#define PERFIS_MAX            9
#define PERFIS_MONTAGEM       "save"
#define PERFIS_ARQUIVO        "save:/perfil_ativo.txt"   /* fica sempre no caminho original */
#define PERFIS_CAMINHO_MAX    0x301                       /* nn::fs: 0x300 caracteres + o zero */

/* Resultado de perfis_reescrever */
typedef enum {
  PERFIS_ORIGINAL = 0,    /* use o caminho como veio */
  PERFIS_REESCRITO = 1,   /* use o caminho em buf */
  PERFIS_BLOQUEAR = -1,   /* não deixe a operação acontecer (falha fechada) */
} perfis_acao_t;

/* Decide o que fazer com `caminho` no perfil `perfil` (1 a PERFIS_MAX).
 * - Perfil 1, ou caminho fora de "save:" , ou o próprio arquivo de controle: ORIGINAL.
 * - `bloqueado`: tudo em "save:" é BLOQUEAR.
 * - Senão "save:/X" vira "save:/perfilN/X" em buf (cap bytes); se não couber: BLOQUEAR,
 *   nunca o caminho original, para um perfil novo jamais gravar por cima do Perfil 1. */
perfis_acao_t perfis_reescrever(int perfil, bool bloqueado, const char *caminho, char *buf,
                                size_t cap);

/* A pasta do perfil ("save:/perfilN") em buf. Devolve false se perfil < 2 ou não couber. */
bool perfis_pasta(int perfil, char *buf, size_t cap);

/* Lê o número do perfil do conteúdo do arquivo de controle: só um dígito de 1 a PERFIS_MAX,
 * com espaços ou quebras de linha opcionais em volta. Qualquer outra coisa: 1. */
int perfis_ler_numero(const char *texto, size_t tamanho);

#endif
