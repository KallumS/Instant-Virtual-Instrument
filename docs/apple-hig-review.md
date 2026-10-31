# Review against Apple's Human Interface Guidelines: Foundations

Instant Virtual Instrument's window checked against the Foundations pages,
the way Good Idea (`docs/apple-hig-review.md`, 1.18 and 1.19), Noterator
(0048) and Transient Shuffler were. The decision is
[0003](decisions/0003-the-house-scheme-and-apples-guidelines.md); the words
are [0004](decisions/0004-the-windows-words-follow-apples-writing-guidelines.md).

The window is a JUCE plugin editor inside a host (a DAW), so some guidance
cannot apply or is the host's to meet. Where a guideline is not met for that
reason, this says so rather than counting it as met.

## What changed (0.2.0)

| Guideline | Before (0.1.0) | Now |
| --- | --- | --- |
| **Colour: one meaning per colour** | A teal accent on knobs and waveform, amber keyswitches, a blue loop, layers from blue to red. | The house scheme: the yellow only for what is on (chosen articulation, ticked box, sounding zone, pressed key); everything else from the blue-grey ramp. Layers by lightness and their written names. |
| **Accessibility, Dark Mode: contrast** | 11-point dim text (`#8A95A3`, 4.9:1). | Every line of text 7:1 or more; warnings written in `#FF9E96`, the house red only marks. |
| **Typography** | 10 to 11 points; a 20-point bold title. | The system sans-serif at 13 points throughout; nothing smaller; no bold. |
| **Accessibility: control size and spacing** | Buttons 24 to 30 points, 4 apart. | 28 points tall, 12 apart; every button as wide as its label. |
| **Accessibility, Typography: larger text** | None. | **Text size** 100%, 125%, 150%, 200%, Cmd+= / Cmd+- / Cmd+0, remembered. The window's rows wrap to its width. |
| **Accessibility: the keyboard alone** | Mouse only. | Tab reaches every control, with a near-white ring; Space or Return presses; Esc lets go. The library: arrow keys and Space. |
| **Colour: more than colour alone** | Chosen articulation shown by colour only. | An ink bar under a chosen button's label; an ink stripe for the sounding round robin; a notch on keyswitch keys. |
| **Layout: nothing important at the bottom** | The message under the zone map. | At the top, under the toolbar, until Dismiss. |
| **Design principles: mistakes cheap to undo** | Loading a sample or new takes lost the old ones. | Undo in the message brings them back. |
| **Writing** | Programmer's words. | 0004. |
| **Branding gives way to content** | A large title and tagline. | Gone; the host names the window. |

## Already met

- **Feedback.** The library lights the zone that sounds and the take it
  used; a playhead shows where in the sample the newest note is; analysis
  shows its progress; export shows its percentage and becomes Stop Export.
- **Playback you control.** Nothing plays by itself: only notes from the
  host, the keyboard, or a held click on the library.
- **Motion.** Only the playhead moves, with the music.
- **Dark Mode.** Always dark: the house scheme, and a media tool (Apple
  allows this).
- **Privacy.** Nothing is collected or sent. The sample is kept in the
  project; Text size in the plugin's own settings file.

## Not met, or not done

- **The ink on a button being pressed is 6.0:1**, not 7:1: the house's held
  grey `#8F96A2` is darker than the rest (`#A9AFBA`, 8.2:1) and hover
  (`#C0C6CF`, 10.5:1) greys. It is above Apple's 4.5:1 minimum and shows only
  while the button is held. It is the family's value (Good Idea's
  `COLOUR.md`), so it is measured and left for the user to decide, not
  changed here; the test holds it to 4.5:1.
- **Increase Contrast and Reduce Motion** are not read from the system.
  Noterator reads them through the Objective-C runtime; it could be done here
  the same way. Nothing moves but the playhead.
- **VoiceOver** can read every control's name (JUCE accessibility,
  `setTitle`), but not the pictures: the waveform, the spectral model and
  the library are not described in words.
- **Keyboard shortcuts** reach the window only when the host passes keys to
  plugins; Text size's menu works either way.
- **Right to left.** English only.

## Not relevant

Materials, app icons, SF Symbols, images, spatial and immersive pages: the
window has none of these.

## Unverified in a host

- How each DAW sizes a resizable plugin window, and whether it passes
  Cmd+= to it.
- Which face "sans-serif" resolves to on each system.
- Colours on a real screen (only measured here).

## Checked by

`Tests/PluginTests.cpp`, run headless with the real editor: the palette's
greys blue-shifted, text 7:1 on the ground, the views and menus, the ink 7:1
on every button state, edges 3:1, the ring near-white and not the yellow,
warnings in the lighter red; and the window at 100%, 200% and its smallest
width: nothing off its panel, controls 28 points tall and 12 apart, buttons
as wide as their labels and in title style with "…", labels 13 points and
7:1, every control reachable by Tab and not taking the keyboard on a click,
every tooltip a sentence in lines of 64 characters, the chosen articulation's
ink bar, the message at the top with Undo and Dismiss, a warning staying
until Dismiss, Cmd+= and Cmd+0. Each rule was broken on purpose and the test
failed (session log, 2026-10-25).
