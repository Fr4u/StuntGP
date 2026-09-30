# Stunt GP NEW

An unofficial fan reconstruction of **Stunt GP** (Team17, 2001) for 64-bit Windows, written in
modern C++ with a DirectX 12 renderer. The game logic is recovered from the original executable;
the original game files are not included.

- Website: https://fr4u.github.io/StuntGP/
- Downloads: https://github.com/Fr4u/StuntGP/releases

## Features

- The complete single-player game: menu, all modes, races, results and saves
- Music and all sound effects
- Widescreen support (Hor+) and an optional original 4:3 view
- Smooth frame interpolation up to the display refresh rate (e.g. 240 Hz), with frame pacing
  locked to the monitor
- Graphics options: MSAA, anisotropic filtering, texture upscaling, bloom, FXAA, display modes
  (windowed, borderless, exclusive full screen)

## Playing

1. Download `StuntGP_NEW-<version>-program.zip` from the releases page and unpack it.
2. From your own copy of Stunt GP (CD or installation), copy the folders `wads`, `graphics24`,
   `meshdata` and `sound` into `data\retail`, next to `Game.cfg`.
3. Run `START.cmd`.

Requires Windows 10/11 x64, a DirectX 12 GPU and the Microsoft Visual C++ 2015–2022 x64 runtime.

## Building

Requires Visual Studio 2022 (Desktop development with C++) and CMake 3.25 or newer.

```bat
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTING=OFF
cmake --build build --config Release --target StuntGP_NEW --parallel
```

The executable is written to `out\Release\StuntGP_NEW.exe`. To run it, place it next to the files from `packaging\`
(`START.cmd`, `START-MENU.cmd`, `data\retail\Game.cfg`) and add the original game files as described above.

## Repository layout

| Path | Contents |
|---|---|
| `src/`, `include/` | Game and engine source code |
| `cmake/` | CMake modules |
| `tests/` | Test programs |
| `packaging/` | Launchers and the start-up configuration used by release packages |
| `docs/PORT-FIXES.md` | Fixes for bugs of the original game, each with its evidence |
| `website/`, `website-src/` | The project website (published to GitHub Pages) |

Stunt GP © 2001 Team17 Software Ltd. This project is not affiliated with Team17.
