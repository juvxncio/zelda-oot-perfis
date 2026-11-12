# zelda-oot-perfis

Mod para **The Legend of Zelda: Ocarina of Time (Switch 2)** que adiciona **perfis de save
dentro do jogo**. O jogo original só tem um save por usuário. Com o mod, a tela de **Opções**
ganha uma linha **"Perfil de save"**, com Perfil 1 a Perfil 9. Cada perfil é um jogo
independente: progresso, configurações e conta.

- Jogo: `0400486024DEC000`, atualização v131072 (1.1.1), build `E0F3CCFAF50065AE`. Em outra
  versão do jogo o mod não faz nada e o jogo segue normal.
- Emulador: Fritchii (Lichii/Eden), testado na 5.8.4.
- Versão: **1.0.0**.

## Como usar

1. Abra **Opções** (na tela inicial ou no menu de pausa) e desça até a última linha,
   **"Perfil de save"**.
2. Escolha o perfil com as setas.
   - **Na tela inicial**, antes de carregar um jogo: ao sair das Opções, o jogo **reinicia
     sozinho** já no perfil escolhido.
   - **Dentro de um jogo**: nada reinicia, para não perder progresso. A troca vale na próxima
     vez que o jogo abrir.

   A legenda da linha diz qual dos dois vai acontecer.
3. **Perfil 1 é o save que já existia**, nos caminhos originais. Os outros ficam em
   `perfilN/` dentro do save do jogo e começam vazios: escolha "Novo jogo".

Os textos aparecem no idioma do jogo. Há 13 idiomas: português, inglês, espanhol, francês,
alemão, italiano, holandês, polonês, russo, japonês, coreano e chinês simplificado e
tradicional. Se o idioma não for reconhecido, o texto fica em inglês.

Reservas, caso a opção nativa falhe um dia (por exemplo, numa atualização do jogo):

- **L + R + direcional para baixo**, em qualquer tela, abre um painel do mod com os perfis.
- Por fora do jogo, `tools/escolher-perfil.ps1 -Perfil N`, com o emulador fechado.

## Instalar e remover

Compile com o devkitPro (devkitA64) e instale com o emulador fechado:

```
make dist PYTHON=<python 3>
powershell -ExecutionPolicy Bypass -File tools\instalar-mod.ps1           # instala
powershell -ExecutionPolicy Bypass -File tools\instalar-mod.ps1 -Remover  # remove
```

O instalador sempre faz backup dos saves antes de copiar o mod, conferido por hash, em
`Documents\BACKUP_ZELDA_NAO_APAGAR`. Com `-Diagnostico`, ele também liga as mensagens do mod
(`[perfis] ...`) no log do emulador.

## Como funciona

- **O módulo `subsdk9`**: o jogo carrega sozinho, no mesmo esquema do
  [sloperina](https://github.com/PracticeROM/sloperina). Ele intercepta as funções de arquivo
  (`nn::fs`) e redireciona os caminhos do save: `slot_00/progress.sav` vira
  `perfil2/slot_00/progress.sav`. Se algo não puder ser interceptado, os perfis ficam
  desligados, e o mod nunca grava no lugar errado.
- **O patch `<build id>.ips`**: desvia 7 funções internas do jogo para o módulo:
  - a lista e os botões da tela de Opções, que ganham a linha nova;
  - a busca de mensagens, que fornece os textos da linha;
  - o fechamento da tela, que dispara o reinício.

  O módulo confere cada desvio antes de usar. A linha usa um tipo de opção que o jogo tem mas
  não usa, então nenhuma configuração real do `option.sav` é alterada.
- **O reinício**: é o do próprio sistema (`nn::oe::RestartProgram`). A memória volta limpa,
  então nada de um perfil pode ser gravado em outro.

Os detalhes técnicos, com endereços e estruturas, estão em
[`docs/investigacao.md`](docs/investigacao.md).

## Regras de segurança do projeto

- Backup dos saves antes de cada instalação (`tools/backup-saves.ps1`), com verificação por hash.
- O dump do jogo (`.nup`) só é lido, nunca alterado.
- Nada do jogo (código, arquivos, saves) entra neste repositório. Veja o `.gitignore`.

## Testes

`make test HOSTCC=<compilador C do PC>` roda a lógica pura no PC: redirecionamento de caminhos,
menu do painel e textos e idiomas.

## Créditos e licenças

- Código sob GPL-3.0 (`LICENSE`). Partes adaptadas do
  [sloperina](https://github.com/PracticeROM/sloperina), também GPL-3.0: o cabeçalho do módulo,
  os ganchos na GOT e a técnica do patch IPS.
- Fonte do painel de reserva: Press Start 2P, SIL Open Font License 1.1 (`mod/font-OFL.txt`).
- Pesquisa da comunidade: [oot-remake-modding](https://github.com/Kabosune/oot-remake-modding)
  e [Hookshot](https://github.com/NotRexed/Hookshot).
