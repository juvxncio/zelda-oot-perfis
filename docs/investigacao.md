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

- O id do usuário atual é `<id do usuário>`.
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

## Diagnóstico no jogo (2026-11-12, módulo 0.1.0-diag, Fritchii 5.7)

O módulo carregou, interceptou as 14 funções e o jogo rodou normalmente. Sequência observada
(segundos desde o início do emulador):

| Tempo | Chamada | Observação |
|---|---|---|
| 71,7 | `MountRom content` | romfs do jogo montado como `content:` |
| 76,0 | `MountSaveData save uid=...` | **save montado como `save:`**, usuário `<id do usuário>` |
| 76,1 | `MountTemporaryStorage savt` | armazenamento temporário (`savt:`) |
| 80,4–81,5 | `OpenFile save:/slot_NN/caption.sav` (leitura) | **lê `slot_00` a `slot_11`**: 12 slots; 06 a 11 dão `0x202` (não existe) |
| 81,5–81,6 | `OpenFile save:/account.sav`, `save:/option.sav` (leitura) | |
| 149,1 | `GetEntryType` + `OpenFile save:/account.sav` modo 6, `Commit save` | ao carregar o jogo: grava `account.sav` |
| 149,9 | `OpenFile save:/slot_03/progress.sav` (leitura) | carrega o save mais recente |
| 276,8–276,9 | `GetEntryType save:/slot_05`, `OpenFile .../progress.sav`, `option.sav`, `.../caption.sav` (modo 6), `Commit save` | **salvamento automático** no próximo slot do rodízio |

- Modo 1 = leitura; modo 6 = escrita + permitir aumentar (`Write | AllowAppend`).
- `0x202` = `ResultPathNotFound`.
- Nenhuma chamada a `ReadDirectory`, `CreateFile`, `CreateDirectory` ou `DeleteFile`: o jogo
  não lista a pasta nem olha datas. Ele acha o save mais recente **pelo conteúdo dos
  `caption.sav`**, então redirecionar os caminhos basta para cada perfil ter o seu.
- Antes de gravar, o jogo testa se a pasta do slot existe (`GetEntryType save:/slot_05`).

## Primeiro teste de perfil novo (2026-11-12, módulo 0.2.0-perfis-teste)

Perfil 2 escolhido com `tools/escolher-perfil.ps1 -Perfil 2`; jogo novo até 4 salvamentos
automáticos.

- **Perfil 1 intacto:** os 14 arquivos do save original conferidos por SHA-256 antes e
  depois, 0 diferenças, nenhum arquivo novo fora de `perfil2/` (além de `perfil_ativo.txt`).
- `save:/perfil2` criado ao montar o save (`CreateDirectory -> 0x0`, tipo pasta).
- 82 linhas no log, **0 bloqueios**, todo caminho em `save:` redirecionado.
- Save novo, como o jogo faz: `OpenFile` de `account.sav`/`option.sav` dá `0x202`, então
  `GetEntryType` + `CreateFile` (tamanho 0) para cada um; para cada slot,
  `GetEntryType save:/slot_NN` → `CreateDirectory` → `CreateFile` de `progress.sav` e
  `caption.sav` → `OpenFile` modo 6 → `Commit`.
- Resultado em `perfil2/`: `account.sav` (320 B), `option.sav` (42.100 B), `slot_00` a
  `slot_03` com `progress.sav` (~120 KB) e `caption.sav` (~40 KB).

## Controle e tela (2026-11-12, módulos 0.3.x e 0.4.0, Fritchii 5.7 e 5.8.4)

- **Controle:** os 5 `nn::hid::GetNpadStates` do main foram interceptados. O teclado do testador
  chega como estilo `FullKey`, npad 0. O layout do estado (botões em `+0x08`) bate com o do
  sloperina. Mapeamento observado: X → B (`0x2`), L (`0x40`), R (`0x80`), ZL (`0x100`),
  ZR (`0x200`), baixo no direcional (`0x8000`). L+R+baixo (`0x80c0`) detectado.
- **Tela:** `nvnBootstrapLoader` interceptado; swapchain de 3 texturas **2560x1440, formato 37
  (RGBA8)**, área visível = textura inteira. Painel de 36x9 células na escala 3x (888x294 px,
  1020 KiB) desenhado em 836,288, legível na tela inicial e em jogo, sem erros no log.
- Funcionou também no **Fritchii 5.8.4** (`oot-gather-fix-4f5c53149b`), para onde o testador
  atualizou.

## Ainda falta descobrir

- Para que servem `slot_06` a `slot_11` (talvez o modo Master Quest). O mod redireciona os 12.

## Menus do jogo (2026-11-12)

Objetivo: pôr a escolha de perfil dentro de um menu do próprio jogo. Analisado com
`tools/romfs.py` (romfs extraído em `Documents\zelda-romfs`, apagar no fim) e
`tools/analisar_main.py refs/asm`.

- Os layouts ficam em `UI/Layout2Archive/*.blarc.zs`, que são SARC de `.buil` com 4 cópias iguais
  de cada arquivo. Os textos ficam em `Mals/USpt.Product.110.sarc.zs`, em `LayoutMsg/<layout>.msbt`.
- **Tela inicial**, `Title_00.buil`:
  - 4 vagas, `Pa_Button_00`..`03`, com os textos "Novo jogo", "Continuar" e "Opções".
  - Classe `UITitleScreen`: vtable em `main+0x4bf06e8`, objeto de 0x2f0 bytes, fábrica em `main+0x34070e0`.
  - A montagem do layout fica em `main+0x34067d0` (vt+0x2e8): cria um botão por vaga e guarda
    a contagem em +712 e o array em +720.
- **Tela de Opções**, `SystemOption_00.buil`:
  - Classe `UISystemOptionScreen`, vtable em `main+0x4bef108`.
  - `main+0x34011c0` (vt+0x2f0) cria as linhas e `main+0x3402960` (vt+0x2e8) monta o layout.
  - Arrays por tipo: Toggle em +696/+704, Select em +712/+720, Level em +728/+736 e separadores
    em +744/+752. A ordem das linhas fica em +824/+832, com entradas `{tipo, índice}`.
- **A lista de Opções vem de dados**: `UI/OptionSetTable/OptionSetTable.game__ui__OptionSetTable.bgyml`,
  dentro de `Pack/Bootup.Nin_Ounce_NVN2.pack.zs`.
  - É um BYML `OptionSetArray` com 20 entradas. Cada uma tem `OptionType` (hash do nome do enum),
    `ButtonType` (hash do tipo de botão), `ChoiceType` (lista com o hash de cada escolha) e,
    opcionalmente, `CheckGameData` + `IsCheckDisplayGameData`, uma flag que decide se a linha aparece.
  - A lista é lida por `main+0x339c5a0` (contagem de linhas visíveis) e `main+0x339c630` (linha i).
    O objeto é `{int n; entrada *e}`, com entradas de 40 bytes: +4 é o tipo e +36 é o hash da
    flag de visibilidade. Fica em `[[GOT main+0x4e7cfa8]]+1280`.
  - O enum de opções tem 18 valores (`None` e mais 17 nomes), tabela em `main+0x3f137e0`. Não dá
    pra criar um tipo novo só com dados: uma linha nova teria que reaproveitar um tipo existente,
    e o código teria que tratar o texto e o valor dela.
  - `VoiceLanguage` tem 9 escolhas, o mesmo número de perfis.

### A linha "Perfil de save" nas Opções (2026-11-12, 0.6.0 a 0.8.0)

- **Ganchos em funções internas** (`mod/ganchos.*`, `tools/mkips.py`): um patch IPS32
  (`exefs/<build id>.ips`) troca a primeira instrução de cada função por um desvio na caverna
  `main+0x3c46c40..0x3c47000` (o fim do `.text`, zerado). O desvio salta para o ponteiro em
  `main+0x4e9f000 + 8*i` (o fim do `.bss`; nenhum código do jogo acessa nada depois de
  `main+0x4e9a4c8`) ou, enquanto o ponteiro é zero, executa a instrução original e volta.
  O Fritchii aplica patches próprios ao main (Max Resolution, Dynamic FPS, Water ripples). Por isso
  o módulo confere o marcador e cada desvio, instrução por instrução, antes de ligar o gancho.
- **A lista de Opções**:
  - `main+0x339c5a0` (conta as linhas visíveis), `main+0x339c630` (pega a i-ésima) e
    `main+0x339c790` (pega por tipo) só são chamadas pela tela de Opções e pelos botões dela.
  - A linha nova é uma cópia da de `VoiceLanguage` (9 escolhas, botão Select) com o tipo 9,
    `MessageWindowTransparency`, e o hash `0x916ac217`.
  - Esse tipo não é membro de `OptionData` (`0x834d9d9e`, 18 membros, conferido no
    `GameDataList`), então nada é gravado no `option.sav`.
- **O botão Select**: a escolha atual fica em +92 e o tipo da opção em +88. `main+0x33cf7f0`
  atualiza o texto, ao montar o botão e a cada seta. É ali que o mod mostra o perfil escolhido
  e grava a mudança.
- **Os textos**:
  - `main+0xa6afe0(saída, arquivo, rótulo, flag)` busca a mensagem de um layout e devolve 1 se
    achou. A saída é `{char *texto, int32 tamanho, int64 -1}`.
  - Os rótulos seguem o padrão `<Opção>_Title`, `<Opção>_Caption` e `<Opção>_%02d` (a escolha).
  - Os textos são UTF-8, com o tamanho em bytes sem o zero final (formato 1, aprendido no jogo).
- **Ao fechar o emulador**, aparece "Userspace PANIC" (resultado 0x1a80a). Isso só acontece
  quando o emulador força a parada ("Force stopping EmuThread"). A pilha fica toda no sdk e não
  passa pelo módulo nem pelas funções desviadas.
