# JekaborGM

General MIDI SoundFont synthesizer: C++20, JUCE 9.0.3, FluidSynth, VST3.

## Windows x64

Requirements: Visual Studio with **Desktop development with C++**, Windows SDK,
CMake 3.22 or newer, and Git. Run the commands from a Developer PowerShell with
CMake on PATH. Visual Studio 2026 requires a CMake version that supports its generator.

The included `.toolchain/fluidsynth-win` contains the Windows FluidSynth SDK.
To use another installation, pass `-DJEKABORGM_FLUIDSYNTH_ROOT=C:/path/to/fluidsynth`.
The SDK's `bin` directory must contain FluidSynth and all of its dependency DLLs.

```powershell
cmake --preset windows-x64
cmake --build --preset windows-x64 --clean-first --parallel 4
```

Output: `build/windows-x64/JekaborGM_artefacts/Release/VST3/JekaborGM.vst3`.
Copy the entire bundle, including its DLLs, to your VST3 plugin directory.
The Microsoft Visual C++ x64 runtime must be installed on the destination computer.

## Linux x64 (Ubuntu / Debian)

```sh
sudo apt-get update
sudo apt-get install -y build-essential cmake ninja-build git pkg-config \
    libfluidsynth-dev libasound2-dev libx11-dev libxext-dev libxinerama-dev \
    libxrandr-dev libxcursor-dev libxcomposite-dev libxrender-dev libxi-dev \
    libfreetype-dev libfontconfig1-dev libgl1-mesa-dev
cmake --preset linux-x64
cmake --build --preset linux-x64 --clean-first --parallel 4
```

Output: `build/linux-x64/JekaborGM_artefacts/Release/VST3/JekaborGM.vst3`.
Copy the bundle to `~/.vst3/`. FluidSynth and the system libraries used during
compilation must also be installed on the destination Linux system. Build on the
oldest Linux distribution you intend to support for compatible system libraries.

Windows and Linux require separate native binaries. The old files directly under
`build/` are historical Linux build outputs and are not used by these presets.
JUCE is downloaded on first configuration; an existing checkout can be used with
`-DFETCHCONTENT_SOURCE_DIR_JUCE=/absolute/path/to/JUCE`.

### Linux build from Windows using WSL

After installing WSL, restart Windows if requested. Then finish installing Ubuntu:

```powershell
wsl --install -d Ubuntu-24.04 --no-launch --web-download
wsl -d Ubuntu-24.04
```

Complete Ubuntu's first-run account setup, install the Linux dependencies above,
and build this checkout from Ubuntu:

```sh
cd /mnt/c/Users/Jekabor/JekaborGM
cmake --preset linux-x64 -DFETCHCONTENT_SOURCE_DIR_JUCE="$PWD/.toolchain/JUCE"
cmake --build --preset linux-x64 --clean-first --parallel 4
```

The explicit JUCE path reuses the checkout downloaded for the Windows build.

## SoundFont

Set `JEKABORGM_SOUNDFONT` to the absolute path of an `.sf2` file before starting
your DAW, or put `FluidR3_GM.sf2` in the per-user directory:

- Windows: `%APPDATA%/JekaborGM/FluidR3_GM.sf2`
- Linux: `${XDG_CONFIG_HOME:-$HOME/.config}/JekaborGM/FluidR3_GM.sf2`

The per-user directory can alternatively contain `GeneralUser-GS.sf2`
([GeneralUser GS by S. Christian Collins](https://github.com/mrbumpy409/GeneralUser-GS)).
The plugin window shows the loaded SoundFont or a loading error. Use **Select
SoundFont...** to load another `.sf2` or `.sf3` file; the selected path is saved
with the DAW project. Changing SoundFonts may briefly pause audio processing.

Use **MIDI channel** and **Instrument** to select a preset from the loaded
SoundFont for each of the 16 MIDI channels. The list shows preset names, program
numbers (1–128), and banks; channel 10 defaults to the percussion bank. Channel
selections are saved with the DAW project and retained when the audio sample rate
changes. Incoming MIDI Program Change messages update the selected instrument.

Linux also falls back to `/usr/share/sounds/sf2/FluidR3_GM.sf2`, available from
Ubuntu's `fluid-soundfont-gm` package. SoundFonts are not bundled. Without a valid
SoundFont the plugin loads but produces no sound.

The plugin identifier is now `Jkgm`; DAWs treat it as a different plugin from the
former OurGM (`Ougm`).
