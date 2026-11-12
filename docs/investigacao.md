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

## Análise do `main` (2026-11-12)

Extraído do `.nup` com `tools/extrair-exefs.ps1` e analisado com `tools/analisar_main.py`.

- Build ID `E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348`: o mesmo da comunidade, então os
  endereços de `oot-remake-modding` valem.
- Segmentos: text `0x0..0x3c461d0`, ro `0x3c47000..0x484c080`, data `0x484d000..0x4d46b80`,
  bss até `0x4ea0000`.
- O `main` importa 1183 símbolos. Os de arquivo que o mod precisa interceptar (todos pela GOT):
  `nn::fs::MountSaveData(const char*, const nn::account::Uid&)`, `OpenFile`, `CreateFile`,
  `DeleteFile`, `GetEntryType`, `OpenDirectory`, `ReadDirectory`, `CreateDirectory`,
  `DeleteDirectoryRecursively`, `Commit`, `Unmount`. Também `nn::account::GetUserId` e
  `TryOpenPreselectedUser`.
- **Os nomes `slot_`, `progress.sav`, `caption.sav`, `option.sav` e `account.sav` não estão no
  executável**, nem em ASCII nem em UTF-16. Só aparece `TemporaryGame.sav` (`main+0x3cb4e4b`).
  Os nomes devem vir dos dados (provavelmente a tabela `GameData/GameDataList` do romfs).
- Há um formato `%s:/%s` em `main+0x3d4e167`, provavelmente para montar
  "ponto de montagem + caminho".

## Ainda falta descobrir

- O formato exato dos caminhos em tempo de execução (ex.: `save:/slot_00/progress.sav`).
  Como os nomes não estão no código, o caminho mais simples é um módulo de diagnóstico que só
  registra no log cada caminho passado para as funções `nn::fs`, sem alterar nada.
- Se o jogo acha o `slot_NN` mais recente listando a pasta (`OpenDirectory`/`ReadDirectory`)
  ou por um índice interno (`QueryGetLatestSaveIndex`). O log do diagnóstico responde isso.
