# Instant Virtual Instrument

One sample in, a whole instrument out. Drop a single recorded note on the
window - a cello, a flute, a voice, a synth - and Instant Virtual Instrument
turns it into a playable sampled instrument: **every key** of the range you
choose, **dynamic layers** from pianissimo to fortissimo, **round robins** so
repeated notes never sound the same, and **nine articulations**, all made from
that one note. It runs as a VST3 (and an AU on the Mac) in your DAW, or on its
own.

## What you get from one note

- **Every semitone, in tune.** Each key is played at its own pitch from a
  model of your sample, not by speeding the recording up or down - so the
  attack and the vibrato keep their speed, and the instrument's body doesn't
  turn into a chipmunk an octave up. **Formant** decides how much of the body
  stays put (75% is natural; 0% is the sped-up-tape sound).
- **Dynamic layers** (1 to 8; 5 by default: pp, p, mf, f, ff). A louder layer
  isn't only louder: it's brighter, it speaks faster, and its bow or breath
  noise sits differently, as a real player's does. Velocity chooses the layer,
  or the mod wheel for long notes, as orchestral libraries work. **Layer
  blend** moves smoothly between neighbouring layers.
- **Round robins** (1 to 8 for every key and layer; 4 by default). Each take
  is a slightly different performance - a few cents of tuning, the tone, the
  attack, the timing, a little scoop into the note, the noise - and every key
  has a character of its own. **Variation** sets how loose the playing is;
  **New Takes** plays a fresh set, and Undo brings the old ones back.
- **Nine articulations**, on keyswitches from C1 or the buttons:

| Key | Articulation | What you hear |
|---|---|---|
| C1  | Sustain    | The note holds while the key is down, on a seamless loop of the sample's steady part |
| C#1 | Legato     | Overlapping notes glide into each other with no new attack |
| D1  | Staccato   | Short and detached |
| D#1 | Spiccato   | Shorter, brighter, bouncier |
| E1  | Marcato    | An accented, bright attack settling into a softer held note |
| F1  | Pizzicato  | Plucked: it rings and dies away, the upper partials first |
| F#1 | Tremolo    | A fast, slightly uneven tremolo |
| G1  | Swell      | A crescendo from soft, growing brighter through the layers |
| G#1 | Fortepiano | A loud attack that drops straight to soft |

The library lights up as you play: the cell for the key and layer turns yellow,
and a stripe shows which round robin played. Click a cell to hear it.

**Export SFZ…** writes the whole library out as WAV files with an SFZ map -
velocity layers, round robins and keyswitches included - so it plays in any
SFZ sampler too.

## How to use it

1. **Load Sample…** (or drop a file on the window): one note, ideally 1 to 5
   seconds, cleanly recorded. WAV, AIFF, FLAC, Ogg and MP3 all work.
   **Load Demo Sound** tries it out with a built-in string-like tone.
2. It finds the note it plays (shown at the top). If it picks the wrong
   octave, set **Root note**.
3. Set **Source dynamic** to how loud your recording was played - a gentle
   note set to *p* gives more convincing loud layers than one left at *mf*.
4. Choose the range (**Lowest key**, **Highest key**), how many **Dynamic
   layers** and **Round robins**, and play.

The sample is kept in your project, so it opens without the original file.

**Text size** (top right; Cmd+= and Cmd+- on the Mac, Ctrl on Windows) makes
the whole window larger, up to 200%. Everything can be reached from the
keyboard with Tab; in the library the arrow keys move and Space plays.

## Good to know

- It is made for **pitched** sounds. A drum or a noise will load, but there's
  no pitch to move, so it plays as C4.
- Moving several octaves from the original note sounds more synthetic than a
  multisampled library would.
- MIDI: velocity, the mod wheel (CC1, for dynamics when chosen), expression
  (CC11), the sustain pedal (CC64) and pitch bend.

## How it works

The sample is analysed into a model of its partials (the harmonics and their
levels over time, split into the tone that moves with pitch, the body
resonance that doesn't, and each harmonic's own colour) plus the noise between
them, then resynthesised live for each note. Measured on a synthetic string
tone: the root key comes back within 0.5 dB of the source, every key is within
4 cents, each layer is louder and brighter than the last, round robins are
clearly different but within 0.7 dB of each other, and 32 notes use about 30%
of one CPU core. The reasons for the design are in `docs/decisions/`.

## Building

Needs CMake 3.22 or later and a C++20 compiler (Visual Studio 2022, Xcode 14+,
or GCC 11+ / Clang 14+). JUCE 8.0.15 is downloaded automatically; add
`-DIVI_JUCE_PATH=/path/to/JUCE` to use your own copy.

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target InstantVirtualInstrument_VST3
```

The VST3 is in `build/InstantVirtualInstrument_artefacts/Release/VST3/`.
Copy it to `C:\Program Files\Common Files\VST3` (Windows),
`~/Library/Audio/Plug-Ins/VST3` (Mac; the AU builds there too) or `~/.vst3`
(Linux). On Linux, first install
`libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxrandr-dev
libxinerama-dev libxcursor-dev libxext-dev libxcomposite-dev libgl-dev`.

Every push to GitHub builds Windows, Mac (Intel and Apple silicon) and Linux
versions and runs the tests; the plug-ins are under the run's Artifacts. The
Mac build is unsigned: if macOS refuses it, run `xattr -cr` on the plug-in.

Tests:

```bash
cmake --build build --target IVICoreTests IVIPluginTests
build/IVICoreTests_artefacts/Release/IVICoreTests   # the engine
build/IVIPluginTests                                # the plug-in and its window (xvfb-run on Linux)
```

The engine tests can also render a demo phrase that walks through the layers,
round robins and articulations: `IVI_RENDER_DEMO=demo.wav` before the command.

## Licence

GPLv3 (see `LICENSE`). JUCE 8 is used under its AGPLv3 option, which works
with distributing this project under the GPL; selling it closed-source would
need a commercial JUCE licence.
