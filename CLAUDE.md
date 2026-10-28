# Instant Virtual Instrument

A VST3 / AU / standalone instrument that makes a whole virtual instrument from
one sample: every semitone of a key range, dynamic layers from pp to ff that
change the timbre as well as the volume, round robins on every key and layer,
and nine articulations made automatically - by analysing the sample into a
spectral model and resynthesising it. JUCE 8. **The user is a musician, not a
programmer - explain in those terms** (what you will hear, which keys, which
layer), with numbers measured, not guessed.

It follows the house style of the user's other repositories: **Good Idea**
(ReaImGui, the source of the colour scheme and the HIG rules) and
**Noterator** (JUCE, whose `Theme` this plugin's follows). When in doubt about
the window, do what Good Idea says and Noterator does.

## Shape of it

| | |
| --- | --- |
| `Source/Core/SpectralAnalyzer.*`, `PitchTracker.*` | The sample to a `SpectralModel`: pitch, partials (slope, body, fine structure), noise bands, attack, loop. |
| `Source/Core/Instrument.*` | The library: keys × layers × round robins, each a seeded `ZoneVariation`. |
| `Source/Core/ResynthVoice.*` | One note: oscillator bank + noise filter bank, the nine articulations. |
| `Source/Core/SynthEngine.*` | MIDI, keyswitches, round-robin counters, legato, pedal, voices, the lock-free instrument swap. |
| `Source/Core/MultisampleExporter.*` | Export SFZ…: every zone to WAV, plus the SFZ map. |
| `Source/Core/DemoSource.*` | The demo sound and the tests' sources. |
| `Source/Plugin/PluginProcessor.*`, `Parameters.*` | The plugin: parameters, state (the sample embedded as FLAC), analysis and export off the audio thread, the message and Undo. |
| `Source/Plugin/Theme.*` | The house colours and the look-and-feel. **Every colour comes from here.** |
| `Source/Plugin/PluginEditor.*`, `Views.*` | The window: `IVIContent` (laid out for a width, wrapping) inside a viewport the editor scales for Text size. |
| `Tests/CoreTests.cpp` | The engine, headless (91 checks). `IVI_RENDER_DEMO=x.wav` renders a demo phrase. |
| `Tests/PluginTests.cpp` | The real processor and editor: state, MIDI, and Apple's guidelines. Needs a display (`xvfb-run` on Linux). `IVI_SNAPSHOT=x.png` writes screenshots at 100% and 200%. |
| `docs/decisions/` | Why things are the way they are, one file per decision. |
| `docs/sessions/` | What happened in a session, written at the end of it. |
| `docs/COLOUR.md`, `docs/apple-hig-review.md` | The palette (copied from Good Idea, kept by hand) and the HIG review. |
| `docs/HANDOVER.md` | The prompt to start a fresh session with. |

**`Source/Core` never includes GUI code**; the tests build it alone.

## The one idea

**The sample becomes a model, and the model is played**
([0001](docs/decisions/0001-spectral-resynthesis-not-stretched-copies.md)).
Nothing is resampled. A key's pitch comes from its MIDI note (in tune whatever
the sample's own tuning), its timbre from the model at that pitch with Formant
deciding how much of the body stays put, a layer's character from tilt,
attack speed, noise and level around Source dynamic, an articulation from how
the model is played through time.

**A zone is a seeded performance**
([0002](docs/decisions/0002-a-zone-is-a-seeded-performance-played-live.md)):
the same seed and settings always give the same library. Draws are keyed by
seed, key, layer and round robin (`mixSeed`), so changing the number of round
robins does not change the first ones' takes, and a key's character is shared
by its takes.

## Rules for the engine

- **The audio thread never allocates, locks or frees.** A new instrument goes
  in through `SynthEngine::setInstrument` (a pending slot); the old one is
  faded out, handed to a trash slot, and freed by `collectGarbage` on the
  message thread (the processor's timer). Tests must call `collectGarbage`.
- **Brightness changes timbre only**: the tilt's energy is normalised, so
  loudness comes from Dynamic range alone. (Before, ff clipped a single note.)
- **The source is normalised to -18 dBFS**, so a four-note ff chord peaks
  near -2 dBFS.
- Parameter **ids never change**; names may (0004).
- A setting that rebuilds the library (`IVIProcessor::structuralParams`) is
  not automatable; Root note re-analyses.

## Words on screen

Apple's writing guidelines (0004, after Good Idea's 0030): buttons and menu
items in title-style capitals, a verb first where they act, "…" (one
character) where they ask for more; a control's name, hints, tooltips and the
message in sentence case, whole sentences with full stops; curly quotes and
apostrophes, an em dash between phrases; messages that say what happened,
then what to do. Text with such characters goes through `params::str`
(UTF-8). Exported file names stay plain ASCII.

## The window: Apple's HIG Foundations

0003, after Good Idea's 0031 and Noterator's 0048:

- Every line of text **7:1** on what it sits on (`text`, `label`, `textDim`,
  `warnText`); `warn` and `edge` are never words.
- The system sans-serif at **13 points** (`theme::font()`), nothing smaller,
  no bold.
- Controls **28 points tall, 12 apart** (`theme::controlHeight`,
  `theme::gap`); a button's width from `theme::buttonWidth` (never narrower
  than its label); a cell's from `labelFit`.
- **Text size** 100-200% (Cmd+= / Cmd+- / Cmd+0), saved in the plugin's
  settings file under `textSize`. The whole content is scaled
  (`setTransform`) and laid out for `width / scale`, wrapping (`Flow`); a
  smaller window scrolls.
- **Keyboard**: every control `wantsKeyboardFocus`, and
  `theme::clicksKeepFocus` stops a click from taking the keys; custom-drawn
  focusable things draw `theme::focusRing` (`#F2F4F7`, never the yellow).
- **Not colour alone**: chosen buttons carry an ink bar (`drawButtonText`).
- **The message at the top** (`IVIProcessor::say (text, warning, canUndo)`),
  until Dismiss; a change that replaces the user's work offers Undo.
- Tooltips: a sentence, broken at 64 characters by the look-and-feel.

New code: every colour from `Theme.h`, every size through the constants
above, every new control given a tooltip sentence and a `setTitle` name.
`PluginTests` walks the whole window and fails on anything that breaks these
at 100%, 200% and the smallest width - add new states to it.

## Colour

The house scheme, unchanged: see `docs/COLOUR.md`. Ground `#23272E`, controls
`#A9AFBA` lettered in the ink `#14171C` (chosen or not), the yellow `#FFF200`
only for what is on. Every grey is blue-shifted, R < G < B. The views sit on
the roll's `#111419`; content (waveform, partials) takes the controls' grey;
dynamic layers are told apart by lightness and their written names.

## Tests

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DIVI_JUCE_PATH=<a JUCE 8.0.15 checkout>
cmake --build build --target IVICoreTests IVIPluginTests
build/IVICoreTests_artefacts/Release/IVICoreTests
xvfb-run -a build/IVIPluginTests
```

**Prove a test bites**: break the thing on purpose in a *copy* of the tree
(never in the working tree), build and run, watch it fail. 0.2.0's UI checks
were proved this way (session log, 2026-10-25).

## What has been learned

- **Measure, then claim.** The first engine looked right and was 10 dB out in
  the noise below the fundamental; high keys were 25 semitones out by the
  test's pitch reading (the harmonics had gone, not the pitch). Each was found
  only by a test measuring it.
- **An FFT test window must fit the region.** 0.17 s at 48 kHz is fewer than
  8,192 samples: the spectrum came back empty and two checks failed for the
  test's sake, not the engine's.
- **A loudness change can hide in a timbre control.** Brightness boosted
  energy at ff by about 10 dB until it was normalised.
- **Builds are slow** (LTO): the plugin takes about 4 minutes, the plugin
  test about 3. Run them in the background and wait for the notification.
- **The container** cannot run downloaded binaries (pluginval was refused):
  `PluginTests` stands in for it. JUCE's `runDispatchLoopUntil` needs modal
  loops, which are off: call change handlers directly in tests.
- `juce::String` from a plain `char*` must be ASCII: use `params::str`.
- **A check must not read the value it checks.** The 28-point and 12-point
  checks first compared against `theme::controlHeight` and `theme::gap`, so
  breaking the theme moved them too; they hold Apple's numbers themselves.
- **A test with a window runs as a JUCE app** (`JUCEApplicationBase::main`).
  A hand-made `runDispatchLoop` hung on macOS CI for six hours: on the Mac
  JUCE's `main` sets up NSApplication. Tests print unbuffered and the plugin
  test has a five-minute watchdog, so a hang shows where it stopped.
- The name `ui` for a helper clashed with the `ivi::ui` namespace under
  `using namespace ivi`.

**Left open** (none asked for yet): Increase Contrast and Reduce Motion not
read from the system; the views not described for VoiceOver; Windows and
macOS builds only built by CI, never run here; real recordings never heard
here - only synthetic tones were measured.
