# 0004. The window's words follow Apple's writing guidelines

Taken 2026-10-25. Stands. Good Idea's
[0030](https://github.com/KallumS/Good-Idea/blob/main/docs/decisions/0030-the-windows-text-follows-apples-writing-guidelines.md),
applied here.

## Context

0.1.0's words were a programmer's: "Load Sample...", "Demo", "Export
SFZ...", "New takes", labels such as "Dyn Bright", "Source Dyn", "KS Base"
and "Trem Rate", parameters in title case ("Dynamic Layers"), values with a
space before the percent sign, tooltips that were fragments, and messages
that said what happened but not what to do.

## Decision

- **Buttons** in title-style capitals, a verb first where they act: Load
  Sample…, Load Demo Sound, Export SFZ…, Stop Export, Undo, Dismiss; the
  menu's Export This Articulation (Sustain)… and Export All Articulations…;
  "…" (one character) where a button asks for more before it acts. **New
  Takes** follows Good Idea's own New Idea. Articulations are names: Sustain,
  Legato, Staccato, Spiccato, Marcato, Pizzicato, Tremolo, Swell, Fortepiano.
- **A control's name is a label, in sentence case, in plain words**: Root
  note, Lowest key, Highest key, Dynamic layers, Round robins, Variation,
  Seed, Formant, Brightness, Source dynamic, Noise, Partials, Dynamic range,
  Layer blend, Dynamics from, Round robin order, Attack, Release, Legato
  glide, Tremolo rate, Pitch drift, Sustain, Gain, Stereo width, Polyphony,
  Bend range, Keyswitches, Lowest keyswitch, Text size. The host sees the
  same names.
- **Values** in title style where they have more than one word (Mod Wheel);
  "75%", not "75 %".
- **The parameter ids never change** (`Source/Plugin/Parameters.h`), only
  their names, so saved projects open as they were.
- **Tooltips and the message** in sentence case, whole sentences with full
  stops; curly quotes and apostrophes, an em dash between phrases, × for
  "by".
- **Messages say what happened, then what to do**: "Couldn’t read
  “x.wav”. Choose a WAV, AIFF, FLAC, Ogg or MP3 file."; "“Cello” is silent or
  too short to analyse. Choose a longer sample."; "Exported 1,460 samples and
  “Cello.sfz” to “…”. Open the .sfz in any SFZ player."
- **Empty places say what to do**: "Drop a sample here, or choose Load
  Sample…".

## Consequences

- Strings with curly quotes, dashes or "…" go through `params::str`
  (UTF-8); MSVC compiles with `/utf-8`.
- The test checks that every button is in title style with no "...", and
  that every tooltip is a sentence.
- Exported file names keep plain ASCII (`Cello_Sustain_060_C4_L3_RR2.wav`).
