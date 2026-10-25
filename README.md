# Instant Virtual Instrument

A VST3 / AU / standalone instrument that turns **one audio sample into a complete
virtual instrument**. Import a single note and the plugin uses spectral resynthesis
to generate:

- **Every semitone** of a chosen key range (default C2 to C8), in tune.
- **Dynamic layers** (1 to 8, default 5: pp, p, mf, f, ff). Each layer changes loudness
  *and* timbre: brightness, attack speed and noise balance.
- **Round robins** (1 to 8 per key and layer, default 4). Each take is a different
  "performance": tuning, timbre, attack, timing, pitch scoop and noise all vary.
- **9 auto-generated articulations**, switched by keyswitches or from the UI:

| Keyswitch (default) | Articulation | How it is generated |
|---|---|---|
| C1  | Sustain    | Attack, then a seamless ping-pong loop through the sample's stable region |
| C#1 | Legato     | Monophonic; overlapping notes glide into each other without a new attack |
| D1  | Staccato   | Faster attack, short hold, quick decay |
| D#1 | Spiccato   | Even shorter, brighter, bouncier |
| E1  | Marcato    | Accented, bright attack that settles to a softer held note |
| F1  | Pizzicato  | Frozen spectrum plucked with a noise burst; upper partials die away first |
| F#1 | Tremolo    | Fast, slightly irregular re-articulation |
| G1  | Swell      | Crescendo that travels up through the dynamic layers |
| G#1 | Fortepiano | Loud attack dropping straight to a soft sustain |

The generated library can also be **exported to WAV + SFZ** (velocity layers, round
robins and keyswitches included), so it works in any SFZ sampler.

## How it works

1. **Analysis** (`Source/Core/SpectralAnalyzer.cpp`)
   - YIN pitch tracking finds the root note and the sample's pitch movement (vibrato).
   - A short-time Fourier transform tracks every harmonic partial frame by frame. It
     follows inharmonic (stretched) partials too, such as piano or bell spectra.
   - Each partial's level is split three ways:
     - a **source slope** that moves with pitch,
     - a **body resonance** (formants) fixed in frequency,
     - a **fine structure** tied to the harmonic number (for example a clarinet's weak
       even harmonics).
   - Whatever isn't a partial (bow, breath or hammer noise) is captured as
     band-limited noise.
   - Onsets are re-sharpened, then the attack, sustain loop and release regions are
     detected. Plucked or struck sources are flagged as decaying.
2. **Instrument build** (`Source/Core/Instrument.cpp`). Every key × layer × round
   robin gets its own deterministic set of variations from a seed. That makes the
   library reproducible, and "New takes" regenerates it.
3. **Real-time resynthesis** (`Source/Core/ResynthVoice.cpp`). Each voice runs an
   oscillator bank of up to 256 partials plus a noise filter bank.
   - Pitch is moved with adjustable formant preservation.
   - Dynamic layers are interpolated (or switched) by velocity or the mod wheel.

Measured by the test suite (`Tests/CoreTests.cpp`) on a synthetic string tone:

- Analysed harmonic levels are within 0.03 dB of the truth.
- Resynthesis at the root key is within 0.5 dB of the source (1/3-octave bands).
- Every mapped key is within 4 cents.
- Layers get louder and brighter at every step.
- Round robins are clearly different (waveform correlation about 0.35) but within
  0.7 dB of each other in level.
- 32 voices use about 30% of one CPU core.

## Controls

| Group | Controls |
|---|---|
| Mapping  | Root (auto or manual), Low/High key, Layers, Round Robins, Variation, Seed, *New takes* |
| Timbre   | Formant preservation, Dynamic brightness, Source dynamic (where your recording sits from ppp to fff), Noise, Max partials |
| Dynamics | Dynamic range (dB), Layer blend, Dynamics source (velocity or mod wheel), RR mode (cycle or random) |
| Shape    | Attack, Release, Legato glide, Tremolo rate, Pitch drift (how much of the original vibrato is kept), Sustain mode (auto, loop or natural) |
| Output   | Gain, Stereo width, Polyphony, Bend range, Keyswitch base, Keyswitches on/off |

MIDI: velocity, CC1 (dynamics when "Mod Wheel" is selected), CC11 (expression),
CC64 (sustain pedal), pitch bend.

Click any cell of the zone map to audition that key and layer. Cells light up as
zones play, and the white stripe shows which round robin fired.

The sample is embedded in the plugin state (as FLAC), so projects reopen without
the original file.

### Getting good results

- Use a clean, single, sustained or plucked note, ideally 1 to 5 seconds long. The
  engine is built for **pitched** sounds. Unpitched sounds (drums, noise) load, but
  are treated as C4.
- If the detected root is an octave off, set **Root** manually.
- Set **Source Dyn** to the dynamic your recording was played at. A soft recording set
  to *p* gives more realistic loud layers.
- Large transpositions sound most natural with **Formant** between 60 and 90%.

## Building

Requirements: CMake 3.22+, a C++20 compiler (Visual Studio 2022, Xcode 14+, or
GCC 11+ / Clang 14+). JUCE 8.0.15 is downloaded automatically; pass
`-DIVI_JUCE_PATH=/path/to/JUCE` to use a local copy instead.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target InstantVirtualInstrument_VST3
ctest --test-dir build -C Release          # core engine tests
cmake --build build --config Release --target IVIPluginTests && ./build/IVIPluginTests   # plugin smoke test
```

The core tests can also render a demo phrase that shows off the layers, round robins
and articulations: `IVI_RENDER_DEMO=demo.wav ./build/IVICoreTests_artefacts/Release/IVICoreTests`.

The VST3 ends up in `build/InstantVirtualInstrument_artefacts/Release/VST3/`. Copy it to:

- **Windows:** `C:\Program Files\Common Files\VST3`
- **macOS:** `~/Library/Audio/Plug-Ins/VST3`. The AU target is also built on macOS.
- **Linux:** `~/.vst3`

On Linux, install the JUCE dependencies first:

```bash
sudo apt install libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev libgl-dev
```

GitHub Actions (`.github/workflows/build.yml`) builds Windows, macOS (universal) and
Linux binaries on every push and uploads them as artifacts. The macOS builds are
unsigned. If macOS blocks them, run `xattr -cr <plugin>`.

## Project layout

```
Source/Core/     analysis, instrument builder, voice, engine, exporter (no GUI code)
Source/Plugin/   JUCE processor, parameters, editor and views
Tests/           headless test suite for the core engine
```

## Licence

GPLv3 (see `LICENSE`). JUCE 8 is used under its AGPLv3 option, which is compatible
with distributing this project under the GPL. Closed-source distribution would need
a commercial JUCE licence.
