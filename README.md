# MFR-Tannhäuser

[![Build](https://github.com/music-for-robots/MFR-Tannhauser/actions/workflows/build.yml/badge.svg)](https://github.com/music-for-robots/MFR-Tannhauser/actions/workflows/build.yml)

A free, open-source guitar effect by [Music For Robots](https://musicforrobots.com) for **late-'90s art-punk guitar**, with both halves of the sound: a glassy **solid-state clean** for the quiet parts and a **cranked British 50 W head** for the loud ones. Both go through a close mic, a room mic and 2-inch tape, and DOUBLE spreads the guitar into a hard-panned double-track.

(The plugin and files are named `MFR-Tannhauser`, without the umlaut, because hosts and installers don't all handle non-ASCII names.)

## The rig

- **Gate**, with an open/closed light.
- **CLEAN channel:** a solid-state combo. Its op-amp stage stays linear until it hits the rails. It has flat-ish Bass/Mid/Treble controls, a **Bright** switch, spring reverb and a 1x12.
- **DIRTY channel:** a British 50 W head. A bright first preamp stage feeds a cold-biased second one, then the classic treble/middle/bass tone stack (modelled on the circuit, in double precision), Presence, Master and EL34-style power clipping, into a 4x12. **BOOST** puts a mid-hump boost pedal in front of it.
- **Studio:** a dynamic close mic, a room mic (**Room**), 2-inch **Tape** saturation, and **Double**, which goes from one guitar in the middle (0) to two takes hard left and right (10).

The channel switch crossfades, so it doesn't click. Each channel keeps its own settings.

Both cabs were fitted to the measured tonal balance of a reference recording, taken from the side channel, which isolates the hard-panned guitars.
- **DIRTY** at the defaults is within about **1 dB** of the distorted guitars (100 Hz to 10 kHz).
- **CLEAN** is within about **1.5 dB** of the clean verse guitar (250 Hz to 6.3 kHz). For that measurement, only the steady (harmonic) parts of the spectrum were kept.

`TannTest --fit` searches each channel's tone controls against those curves.

## Controls

| Section | Controls |
| --- | --- |
| **Channel** | CLEAN / DIRTY |
| **Clean** | Volume, Bass, Mid, Treble, Spring, BRIGHT |
| **Dirty** | Preamp, Bass, Middle, Treble, Presence, Master, BOOST |
| **In / Out** | Gate (all the way down = off), Output |
| **Studio** | Room, Tape, Double |

INIT resets everything. Knobs: double-click or Alt-click resets a knob, Ctrl/Shift-drag gives fine control, and you can click a value to type one in.

Formats: VST3 and Standalone (Windows 64-bit). Stereo output, mono or stereo input.

## Installing

Download the latest release from the [Releases](../../releases) page. Use the installer, or copy `MFR-Tannhauser.vst3` into `C:\Program Files\Common Files\VST3\` yourself, then rescan plugins in your DAW.

## Building from source

Requirements: CMake 3.22+ and Visual Studio 2022 with the C++ desktop workload. JUCE 8 is included as a git submodule.

```
git clone --recursive https://github.com/music-for-robots/MFR-Tannhauser.git
cd MFR-Tannhauser
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The built plugin ends up in `build/Tannhauser_artefacts/Release/VST3/`. There are two offline tools:

- **`TannTest`** checks:
  - both voicings against the reference curves;
  - that CLEAN stays clean and DIRTY distorts;
  - that the channel switch doesn't jump;
  - the gate;
  - Double's width and mono fold-down;
  - held-chord stability;
  - output levels.

  It fails on regressions and writes a short demo WAV.
- **`TannSnapshot`** renders the UI to a PNG.

To package it locally, run `scripts/package.ps1`. It needs [Inno Setup 6](https://jrsoftware.org/isinfo.php).

## Releasing

1. Bump `VERSION` in `project(...)` in `CMakeLists.txt`.
2. Add a section for the new version to `CHANGELOG.md`.
3. Commit, then tag and push, e.g. `git tag v1.0.1 && git push origin v1.0.1`.

GitHub Actions then builds the plugin, runs `TannTest` and [pluginval](https://github.com/Tracktion/pluginval) (strictness 10), and publishes a release with the zip and the installer.

## Source layout

| Path | What's in it |
| --- | --- |
| `Source/TannDSP.h` | Gate, both channels, mics, doubler, room and tape (plain C++) |
| `Source/PluginProcessor.*` | Parameters, 4× oversampling, stereo output |
| `Source/PluginEditor.*` | User interface |
| `tools/` | `TannTest` and `TannSnapshot` |
| `installer/`, `scripts/package.ps1` | Installer script and packaging |
| `.github/workflows/build.yml` | CI build, validation and releases |

## License

MFR-Tannhäuser is licensed under the [GNU AGPLv3](LICENSE) (or any later version). It is built with [JUCE](https://juce.com), which is used under the AGPLv3, and with the Steinberg VST3 SDK. VST is a trademark of Steinberg Media Technologies GmbH. MFR-Tannhäuser is an independent project.
