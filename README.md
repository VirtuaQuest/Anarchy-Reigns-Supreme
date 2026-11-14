# Anarchy Reigns Recompiled

<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

A native PC port of Anarchy Reigns (Xbox 360) by static recompilation, for Windows with Direct3D 12. This project contains no game code or art. You build the game on your own PC from your own legally obtained copy. The downloads on the Releases page hold only this project's source and its Setup, never the game.

This is an unofficial fan project, not affiliated with or endorsed by SEGA, PlatinumGames or Microsoft. All game names and trademarks belong to their owners.

## Status

| | |
|---|---|
| Campaign, Tutorial, Training, Gallery | Playable |
| Frame rate | 30, 60 or 120 fps setting |
| Keyboard and mouse | Built in, with keys you can change in Options |
| Controllers | Xbox, PlayStation, Switch, Steam and most others, read as an Xbox 360 controller, with rumble. The launcher's **Controller types** setting, *Xbox only*, goes back to reading only Xbox controllers |
| DLC | Bayonetta, the extra game modes and Jack's golden chainsaw all work if you have them |
| Multiplayer | **Work in progress**, off by default: see [Multiplayer](#multiplayer) |

Known problems:

- The heaviest fights can fall below the 120 fps setting. They reached 56-85 fps on an i9-10900 with an RTX 3080 Ti.
- The Tutorial is limited to 30 fps, because its target can't be reached at higher rates. In Training, a glow on the floor flickers above 30 fps.
- Some launches from fans and vents may still run too fast above 30 fps.
- Occasional short slowdowns of about 2 seconds, mostly with other games running.

## Building

You need:

- Your own Xbox 360 copy of the game: a disc image (`.iso`) or a folder of its files. DLC packages are optional.
- [Visual Studio](https://visualstudio.microsoft.com/) 2022 or its Build Tools, with "Desktop development with C++" and a Windows SDK. That includes CMake and Ninja.
- [LLVM](https://llvm.org/) (`clang-cl`), or "C++ Clang tools for Windows" in the Visual Studio installer.
- About 5 GB of free disk space.

Get this project. It includes everything the build needs, the recompiler too, so cloning it or downloading it as a ZIP (**Code**, **Download ZIP**) are both fine:

```
git clone https://github.com/VirtuaQuest/Anarchy-Reigns-Supreme.git
```

Then run `Setup.bat` in `Anarchy-Reigns-Supreme`. Choose your game and, if you have it, your DLC folder, and press **Build**. Setup shows what it found and how far along it is. On a 10 core PC the whole build took about 6 minutes.

When it's finished, press **Open launcher**, or start `out\build\ARRecomp\ARLauncher.exe`, to choose your settings and play.

### With RecompLauncher

The release download carries a `recomplauncher.json`, so [RecompLauncher](https://recomplauncher.com) can install it. You still need the tools above. Choose your disc image or the game's folder when it asks, and the launcher builds the game on its setup screen, once. Play then starts the game. All the settings of the game's launcher are on the game's page in RecompLauncher, apart from the player name and host's IP for multiplayer: to reach those, set **Play opens** to the game's launcher. The game's logs are in `run\logs`.

To make a release: commit, then `python tools/make_release.py v1.2.3`, and attach `out\release\Anarchy-Reigns-Supreme-v1.2.3-Windows-x64.zip` to a GitHub release with that tag.

### What Setup does

All of this stays on your PC, and nothing is downloaded:

1. Takes the game's executable from your copy, and checks it's the version this project was made for.
2. Unpacks your DLC into `run\user\content`, checking every block against the package's own hashes.
3. Recompiles the game's code into `ARRecompLib\ppc`.
4. Compiles the game and the launcher into `out\build\ARRecomp`.
5. Runs the game for a moment, unseen and silent, to cut the window and program icon from its logo.

## Controls

These are the keyboard and mouse defaults. Change them under Keyboard and Mouse Controls in Options.

| Action | Key | Action | Key |
|---|---|---|---|
| Move | W A S D | Normal Attack | Left click |
| Camera | Mouse | Heavy Attack | Tap right click |
| Jump | Space | 360 Attack | Left + right click |
| Grab | E | Use Killer Weapon | Hold right click or Shift |
| Guard | Ctrl or C | Strong Killer Weapon Attack | R |
| Evade | Guard + Space | Weak Killer Weapon Attack | Right click + left click |
| Lock-On | Tab or middle click | Unleash Rampage | Q |
| Taunts | T, V | Use Items | 1, 2 |
| Start Mission | F | Pause | Esc |

To equip the golden chainsaw, open Character in a multiplayer lobby, highlight Jack and press R. Only some game types allow it.

## Multiplayer

**This is a work in progress.** It has no online service behind it: copies of the game connect to each other directly. Xbox Live is gone, so Ranked Match and Leaderboards are greyed out.

To play, turn **Multiplayer** on in the launcher and give yourself a player name. One player chooses Player Match, then Create Match. The others choose Quick Match, and if they aren't on the same local network as the host, enter the host's IP in the launcher first. When everyone is Ready, the host's game fills the match with CPU players up to the game type's minimum. Private Match can only be played alone with CPU players for now.

What to expect:

- Hits can look out of step between players: an attacked player's animation may jump or restart on the attacker's screen. Damage has agreed between the two copies in tests.
- Matches are limited to 30 fps at any setting. A slower PC drops frames to keep up, and only slows down below about 7.5 fps.
- Most testing has been between two copies on one PC, with some between two PCs. Play over the internet hasn't been tried.
- Use the same build on every PC. Each copy uses a block of 16 UDP and TCP ports from 36000 up, which a firewall has to allow. When Multiplayer is on, the launcher checks Windows Firewall and offers to let the game through.

## Sharing

The programs you build contain the game's code, and their icon comes from the game. Don't share them. Everything made from the game is in `ARRecompLib\ppc`, `ARRecompLib\private`, `out` and `run`, which git ignores.

## Credits and license

GPL-3.0, see [LICENSE](LICENSE). Built on [XenonRecomp](https://github.com/hedge-dev/XenonRecomp) (MIT; this project's fork of it, [VirtuaQuest/XenonRecomp](https://github.com/VirtuaQuest/XenonRecomp), is copied into `thirdparty/XenonRecomp` with its libraries and their licenses) and the runtime of [Unleashed Recompiled](https://github.com/hedge-dev/UnleashedRecomp), with parts of [Xenia](https://github.com/xenia-project/xenia); see `thirdparty` for the others. The keyboard and mouse prompts are original drawings, with text in [Noto Sans](https://fonts.google.com/noto/specimen/Noto+Sans) (SIL Open Font License).

More for developers: [docs/LESSONS.md](docs/LESSONS.md) and [docs/DEBUGGING.md](docs/DEBUGGING.md).
