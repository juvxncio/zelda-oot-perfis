# Investigação: sistema de save

Levantado em 2026-11-12. Fontes: os saves no PC, o log do emulador e a documentação da
comunidade ([oot-remake-modding](https://github.com/Kabosune/oot-remake-modding),
[sloperina](https://github.com/PracticeROM/sloperina) `docs/FINDINGS.md`).

## Onde ficam os saves

```
%APPDATA%\fritchii\nand\user\save\0000000000000000\<id do usuário>\0400486024DEC000\
├── account.sav       (320 bytes)
├── option.sav        (42.100 bytes, configurações)
├── slot_00\  caption.sav + progress.sav
├── ...
└── slot_05\  caption.sav + progress.sav
```

- O id do usuário atual é `35092526F2729A1E6560A99581586E25`.
- `slot_00` a `slot_05`: um save manual e cinco automáticos que se revezam, segundo
  relatos. No PC, o `slot_00` tem data antiga e os slots 01 a 05 mudam juntos.
- Todos os `.sav` começam com o cabeçalho `04 03 02 01 EB 80 2D 00`.
- `progress.sav`: pares `(hash u32, valor u32)` a partir do offset 0x20, chave =
  MurmurHash3_32 (seed 0) do nome do campo. **Sem checksum.**
- `caption.sav`: local e data do save, também em pares hash/valor.

## Campos no código (GameData)

- Cada campo salvo tem um "save file index" no offset `+0x10`: -1 = não salvo,
  0 = o slot, 1 a 3 = outros arquivos (`option.sav`, `account.sav`...).
- Comandos do jogo com nomes ligados ao save (de `symbols/e0f3ccfa-commands.csv`):
  `SeqExecuteLoadSaveData`, `SeqExecuteSaveGameData`, `SeqExecuteCommitSave`,
  `QueryGetLatestSaveIndex`, `OneShotAutoSaveMgrRequestAutoSave`,
  `SeqOneShotRepairSaveData`.

## Por que redirecionar caminhos

O `main` do jogo chama as funções de arquivo do SDK (`nn::fs::OpenFile`, `CreateFile`,
`DeleteFile`...) pela tabela de imports (GOT). O sloperina já reescreve entradas dessa tabela
(`src/ootgz/got.c`). Interceptando essas funções, o mod troca o caminho do save sem precisar
entender a lógica de save do jogo.

## Ainda falta descobrir

- O nome exato do ponto de montagem e dos caminhos (ex.: `save:/slot_00/progress.sav`).
  Precisa extrair o `main` do `.nup` e procurar as strings.
- Quais funções `nn::fs` o jogo usa para o save: abrir, criar, apagar, renomear, listar
  pastas e `CommitSaveData`.
- Se o jogo lê o `slot_NN` mais recente olhando datas ou um índice interno
  (`QueryGetLatestSaveIndex`).
