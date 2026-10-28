# 0003. The house colour scheme and Apple's Human Interface Guidelines

Taken 2026-10-25. Stands. Follows Good Idea's
[0031](https://github.com/KallumS/Good-Idea/blob/main/docs/decisions/0031-apples-hig-foundations.md)
and Noterator's 0015 and 0048, which ported the same scheme to JUCE.

## Context

The user asked for the house style and GUI guidelines from the Good Idea
repository to be followed. The first window (0.1.0) had its own look: a teal
accent, amber keyswitches, a blue loop, layers coloured from blue to red,
11-point text, controls 24 points tall, a large title, and the message
under the zone map.

## Decision

Good Idea's rules, in JUCE the way Noterator does them (`Source/Plugin/Theme.*`):

- **The house scheme** (`docs/COLOUR.md`): ground `#23272E`, controls
  `#A9AFBA`, the ink `#14171C` on every button, chosen or not, and the yellow
  `#FFF200` only for what is on - the chosen articulation, a ticked box, a
  sounding zone, a pressed key. Every grey blue-shifted. The views (sample,
  spectral model, library) sit on the roll's `#111419`. Layers are told
  apart by lightness and their written names, not by hue.
- **Contrast**: every line of text 7:1 on what it sits on (text `#DDE1E7`,
  names `#BFC5CE`, secondary `#B0B6C0`, warnings `#FF9E96`); the house red
  only marks. Things you can see and grab are 3:1 or more (edges `#8A919C`,
  scrollbar thumb `#6D7581`).
- **Type and size**: the system sans-serif at 13 points, nothing smaller, no
  bold headings (the family's look); controls 28 points tall and 12 apart;
  every button at least as wide as its label.
- **Text Size** 100%, 125%, 150%, 200%, with Cmd+= / Cmd+- / Cmd+0 (Ctrl on
  Windows), remembered between sessions in the plugin's own settings file,
  not in the project. The whole window scales, and its rows wrap to the
  window's width, so nothing runs off; a window smaller than its content
  scrolls.
- **The keyboard**: Tab reaches every control in reading order, with a
  near-white ring (`#F2F4F7`); a click leaves the keyboard where it was, as
  on the Mac; Esc lets go of it. The library is reachable too: arrow keys
  move a cursor, Space plays the zone or chooses the keyswitch.
- **Not colour alone**: a chosen button carries an ink bar under its label;
  a sounding zone an ink stripe for its round robin; a keyswitch key a notch.
- **The message at the top**, until Dismiss or a newer one; a change that
  replaces your work (loading another sample, New Takes) leaves an **Undo**.
  Errors are written there, not in dialogs.
- **Menus and tooltips** `#2A2F37`, lighter than the window, edged
  `#3A404A`; tooltips broken into lines of 64 characters.
- **No space spent on a logo**: the large title went; the host's window
  title names the plugin.

## Consequences

- `Tests/PluginTests.cpp` checks, as Good Idea's `hig_mock.lua` does: blue
  greys, 7:1 text, 7:1 ink on every button state, 3:1 edges, the ring not
  the yellow; and for the window at 100%, 200% and the smallest width:
  nothing off its panel, controls 28 points and 12 apart, buttons as wide as
  their labels, title-style buttons, 13-point labels at 7:1, every control
  reachable by keyboard with a sentence for a tooltip of 64-character lines,
  the chosen button's ink bar, Undo and Dismiss, Cmd+= and Cmd+0. Each was
  broken on purpose and the test failed (session log, 2026-10-25).
- The window is taller than 0.1.0's; at 200% it scrolls on most screens.
- Not done, as `docs/apple-hig-review.md` says: Increase Contrast and
  Reduce Motion are not read from the system (Noterator reads them through
  the Objective-C runtime; nothing here moves but the playhead); VoiceOver
  reads the controls' names (JUCE's accessibility, `setTitle`), but not the
  views' pictures.
