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

## Sound module controls

The editor is an 880 x 560 compact rack-style module, scalable from 792 x 504 to
1320 x 840 with a fixed aspect ratio. The LCD follows the selected MIDI part,
including incoming Program Change messages. Programs are displayed as 001-128;
MIDI and saved-state program values remain 0-127. Bank numbers are the SoundFont's
native bank numbers. The bank selector lists available banks, and the program menu
groups bank 0 by the sixteen GM families. Previous/next wraps through available
presets in the current bank. Channel 10 starts on percussion bank 128.

Each of the sixteen channels has independent, host-automatable controls:

| Control | Range / default | Implementation |
| --- | --- | --- |
| Volume | 0-127 / 100 | MIDI CC7 |
| Pan | 0-127 / 64 (center) | MIDI CC10 |
| Expression | 0-127 / 127 | MIDI CC11 |
| Reverb / chorus | 0-127 / 40 and 0 | Native FluidSynth sends, CC91 / CC93 |
| Attack / decay / release | 0.25x-4x / 1x | Native SoundFont generator offsets, -2400 to +2400 timecents |
| Sustain | -24 to +24 dB / 0 dB | Native sustain attenuation offset; positive raises sustain within the preset's valid limits |
| Bend range | 0-24 semitones / 2 | FluidSynth pitch-wheel sensitivity |
| Modulation | 0-127 / 0 | MIDI CC1 |
| Velocity sensitivity | 0-200% / 100% | Note-on velocity curve: output = 127 * (input / 127) ^ (sensitivity / 100) |
| Fine tune | -100 to +100 cents / 0 | Native fine-tuning generator offset |

Envelope controls modify the loaded preset's envelope rather than replacing it.
A piano sample still decays naturally, and increasing sustain cannot turn a
non-looping sample into an indefinitely sustained sound. Neutral values preserve
the instrument's original envelope. Reverb and chorus are channel sends to the
engine's shared effects; the audible result also depends on the SoundFont's own
modulators and sends.

Stereo output controls are shared by all channels:

| Control | Range / default | Implementation |
| --- | --- | --- |
| Filter switch | Off | Smoothed dry/filtered crossfade |
| Cutoff | 30-20,000 Hz / 20,000 Hz | Stereo TPT low-pass; capped below Nyquist at low sample rates |
| Resonance | Q 0.707-2 / 0.707 | Bounded filter resonance |
| Low / mid / high | +/-12 dB / 0 dB | 180 Hz shelf, 1 kHz bell, 5 kHz shelf |
| Master | -60 to +6 dB / 0 dB | Smoothed output gain |

Knobs support drag, mouse wheel, numeric entry, and double-click reset. Envelope
time values are displayed and entered as multipliers. Meter segments show
post-processing stereo peaks; CLIP holds briefly when the signal reaches full
scale before protection. A soft output guard is transparent below 0.95 amplitude
and bounds louder output to +/-1. MIDI IN flashes for notes, CC, program, bend,
and pressure messages. PANIC (ALL NOTES OFF) silences all parts and releases sustain.
RESET ALL restores the controls of all sixteen channels and the stereo output to
their defaults, including MIDI controller overrides and pitch bend. Instruments,
banks, the selected MIDI channel, and the loaded SoundFont are retained.

Incoming MIDI retains all sixteen channels. MIDI CC changes take precedence until
the corresponding knob or host parameter changes. Visible controller knobs follow
incoming CC without writing the MIDI performance into the host's automation lane.
The saved state includes both host parameter targets and effective MIDI controller
snapshots, so an in-progress smoothing ramp does not become a stuck intermediate
value after restoration. SoundFont path, all channel presets, and the selected
editor channel are also saved. Original JekaborGM states remain readable and use
neutral defaults for the new controls. SoundFont files themselves are external.

### Architecture and realtime behavior

`PluginProcessor` retains the FluidSynth engine and sample-position MIDI rendering.
JUCE AudioProcessorValueTreeState owns 215 stable parameters: thirteen for each of
sixteen channels, plus seven stereo-output parameters. Atomic parameter pointers
are cached before playback. Channel controls smooth over 25 ms; output controls
smooth over 35 ms. Coefficients use fixed arrays, with no per-sample allocation.
Filter/tone processing is stereo and follows the synth's native effects.

The audio callback owns synth access during rendering. Editor instrument changes
are queued atomically; editor meters, activity, and program data use atomic
snapshots. Preset names are cached outside the callback. SoundFont loading, state
restoration, and sample-rate recreation use a maintenance gate: only the
non-rendering caller waits, while the audio callback returns silence immediately.
There is no callback-side application mutex. FluidSynth's threadsafe API is disabled
because access is exclusive; its CPU core setting is one. SoundFont loading is a
lifecycle operation and briefly interrupts playback. A failed load leaves the
previous font installed.

### Regression checks and panel snapshots

A valid default SoundFont (or `JEKABORGM_SOUNDFONT`) is required by the runner.

```powershell
cmake --preset windows-x64 -DJEKABORGM_BUILD_TESTS=ON
cmake --build build/windows-x64 --config Release --target JekaborGMVerification --parallel 4
ctest --test-dir build/windows-x64 -C Release --output-on-failure
```

The optional runner checks parameter registration, MIDI event timing, audio output,
channel independence, Program Change and controller handling, complete parameter
state roundtrip, legacy state, SoundFont load failures, sample-rate changes, filter
attenuation, master gain, bounded output under extreme settings, and concurrent
maintenance/rendering. It also writes actual JUCE editor snapshots at three sizes
to `build/windows-x64/verification/`. Linux can enable the same option with the
`linux-x64` configure preset and run its generated test executable through CTest.

MIDI data entry and RPN/NRPN selection are forwarded to FluidSynth during playback.
Their command packets are not replayed as saved controller values; the module's
bend-range and tuning parameters are the persistent controls for those settings.
