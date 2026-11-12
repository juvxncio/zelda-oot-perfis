# zelda-oot-perfis: Save Profiles for Ocarina of Time (Switch 2)

*[Leia em português](README.pt-BR.md)*

A mod for **The Legend of Zelda: Ocarina of Time** on Switch 2 that adds **save profiles inside
the game**. The game has only one save per user. This mod adds a **"Save Profile"** row to the
game's own **Options** screen, with Profile 1 to Profile 9. Each profile is a separate game,
with its own progress, settings and account data.

![The Save Profile row in Options (game in Portuguese)](docs/img/opcoes.png)

*The game shown in Portuguese; the row follows the game's language.*

- **Game:** `0400486024DEC000`, update v131072 (**1.1.1**). On any other version the mod does
  nothing and the game runs as usual.
- **Emulator:** Fritchii (Lichii / Eden family). Tested on Fritchii 5.8.4.
- **Version:** 1.0.0.

## Install

1. Download `zelda-oot-perfis-v1.0.0.zip`.
2. In the emulator, right-click the game in the list and choose **Open Mod Data Location**.
   A folder opens, for example `%APPDATA%\fritchii\load\0400486024DEC000\`.
3. Extract the zip **into that folder**. You should end up with
   `...\0400486024DEC000\zelda-oot-perfis\exefs\` and two files inside it: `subsdk9` and
   `E0F3CCFAF50065AE4EA01EB5AEB13B1A45DA3348.ips`.
4. Start the game. In the game's **Properties → Add-Ons**, `zelda-oot-perfis` should be listed
   and checked.

To uninstall, delete the `zelda-oot-perfis` folder or uncheck it in Add-Ons. Your saves stay
where they are.

> **Back up your save first.** Profile 1 never moves and the mod was tested carefully on a real
> playthrough, but it is still a mod. Your save is in the emulator's data folder under
> `nand\user\save\0000000000000000\<user>\0400486024DEC000\`.

## How to use

1. Open **Options**, from the title screen or the pause menu, and scroll to the last row,
   **Save Profile**.
2. Pick a profile with left/right.
   - **On the title screen** (before Continue / New Game): when you leave Options, the game
     **restarts by itself** straight into the chosen profile.
   - **During a game**: nothing restarts, so no unsaved progress is lost. The change applies the
     next time the game starts.

   The description under the row tells you which of the two will happen.
3. **Profile 1 is your existing save**, in its usual place, untouched. The other profiles start
   empty: choose **New Game**. They are stored in `perfil2/` … `perfil9/` folders inside the
   game's save data.

The texts follow the game's language. 13 languages are supported: English, Portuguese, Spanish,
French, German, Italian, Dutch, Polish, Russian, Japanese, Korean, and Simplified and Traditional
Chinese. Unknown languages fall back to English. Only the Portuguese and English texts were
written by a native speaker; the others are translations, and corrections are welcome.

### Fallbacks

- **L + R + D-pad Down**, on any screen, opens a small panel drawn by the mod with the same
  profile list. It is meant in case the Options row ever stops working, for example after a
  game update.
- The profile is stored as a single digit in `perfil_ativo.txt` in the game's save data, so it
  can also be edited by hand with the emulator closed.

## How it works

- **`subsdk9`** is a code module the game loads by itself (the same approach as
  [sloperina](https://github.com/PracticeROM/sloperina)). It hooks the game's file functions
  (`nn::fs`) and redirects save paths, so that `slot_00/progress.sav` becomes
  `perfil2/slot_00/progress.sav`. If any of these hooks cannot be installed, profiles stay off.
  The mod never writes to a wrong place.
- **`<build id>.ips`** patches 7 internal game functions to jump into the module:
  - the Options list and its buttons, which get the new row;
  - the message lookup, which provides the row's texts;
  - the screen's close, which triggers the restart.

  Every patch is checked before use. The row reuses an option type that exists in the game but
  is never used, so none of your real settings in `option.sav` are touched.
- **The restart** is the system's own (`nn::oe::RestartProgram`). Memory starts clean, so
  nothing from one profile can leak into another.

Technical notes, with addresses and data layouts, are in
[`docs/investigacao.md`](docs/investigacao.md) (Portuguese).

## Building from source

You need devkitPro (devkitA64) and Python 3.

```
make dist PYTHON=python3          # builds dist/load/0400486024DEC000/zelda-oot-perfis/exefs/
make test HOSTCC=cc               # runs the pure-logic tests on the PC
powershell -File tools\empacotar.ps1   # makes the release zips
```

## Credits and licenses

- Code: GPL-3.0 (`LICENSE`). Parts are adapted from
  [sloperina](https://github.com/PracticeROM/sloperina), also GPL-3.0: the module header, the
  GOT hooks and the IPS hook technique.
- The fallback panel uses the Press Start 2P font, SIL Open Font License 1.1
  (`mod/font-OFL.txt`).
- Community research: [oot-remake-modding](https://github.com/Kabosune/oot-remake-modding) and
  [Hookshot](https://github.com/NotRexed/Hookshot).
