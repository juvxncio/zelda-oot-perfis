# zelda-oot-perfis

Mod para **The Legend of Zelda: Ocarina of Time (Switch 2)** que adiciona **perfis de save
dentro do jogo**. O jogo original só tem um save por usuário; este mod deixa escolher, na tela
inicial, entre vários jogos independentes (Perfil 1, 2, 3...).

- Jogo: `0400486024DEC000`, atualização v131072 (1.1.1), build `E0F3CCFAF50065AE`.
- Emulador: Fritchii (Lichii).
- Estado: **em desenvolvimento, ainda não funciona.**

## Como vai funcionar

1. Um módulo (`subsdk9`) que o jogo carrega sozinho, no mesmo esquema do
   [sloperina](https://github.com/PracticeROM/sloperina).
2. O módulo intercepta as funções de arquivo (`nn::fs`) que o jogo chama e redireciona os
   caminhos do save: `slot_00/progress.sav` vira `perfil2/slot_00/progress.sav`.
3. Um menu na tela inicial escolhe o perfil. Só dá para trocar na tela inicial, nunca no meio
   de uma partida.
4. **Perfil 1 = o save que já existe**, nos caminhos originais, sem nenhuma mudança.

## Regras de segurança (valem para todo o projeto)

- Antes de qualquer teste: backup dos saves (`tools/backup-saves.ps1`), com verificação por hash.
- O dump do jogo (`.nup`) só é lido, nunca alterado.
- Os primeiros testes usam só perfis novos; o Perfil 1 (save atual) só entra quando o
  redirecionamento estiver comprovado.
- Nada do jogo (código, arquivos, saves) entra neste repositório. Veja o `.gitignore`.

## Documentação

- [`docs/investigacao.md`](docs/investigacao.md): o que já se sabe sobre o sistema de save.
