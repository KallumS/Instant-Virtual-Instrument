# The colour scheme

Every colour the window uses. It is the house scheme, the same as Good Idea,
Midi Catalogue, Starting Blocks, Midi Suggester, Midi Variator, ScaleView and
Noterator: a dark cool-grey ground, a light grey for the controls raised off
it, and one yellow for whatever is switched on.

This file was copied from Good Idea's `docs/COLOUR.md` on 2026-10-25 and is
**not generated**: it is a reference kept by hand. The values live in
`Source/Plugin/Theme.h`; if you change one there, change it here too. The
sections after "Where Instant Virtual Instrument uses them" are Good Idea's,
unchanged (its ReaImGui `Col_` table is kept for the family's sake; this
plugin is JUCE, and `Theme.cpp` sets JUCE's colour ids from the same values).

## Where Instant Virtual Instrument uses them

| | colour | |
| --- | --- | --- |
| The window | `#23272E` | ground |
| The sample, the spectral model, the library | `#111419` | the views' ground, as Good Idea's piano roll; edged in `#3A404A` |
| Buttons, combo boxes, the knobs' faces, keyswitch cells | `#A9AFBA` | controls, lettered in the ink `#14171C`; hover `#C0C6CF`, held `#8F96A2` |
| A chosen articulation, a ticked box, a sounding zone, a pressed key, the current keyswitch | `#FFF200` | the accent: only what is on. A chosen button also carries an ink bar; a sounding zone an ink stripe for its round robin |
| A knob's value arc | `#A9AFBA`, `#FFF200` while dragged | as Good Idea's slider grab |
| A knob's track, value fields | `#1A1D23` edged `#8A919C` | sunken, 4.7:1 edge |
| Dynamic layers in the library | `#2A2F37` (softest) to `#8F96A2` (loudest) | told apart by lightness and by the layer names written beside them, never by hue |
| The waveform and the partials | `#A9AFBA` | content in the controls' grey, like Good Idea's chords and bass |
| The body curve | `#DDE1E7` | |
| The noise in the spectral model, the sustain loop | `#3A404A` | |
| The attack region | `#22262D` | |
| Region edges | `#6D7581` | |
| The playhead, the keyboard's focus ring | `#F2F4F7` | near-white: yellow means *on*, and a playing or focused thing need not be |
| Body text, layer names | `#DDE1E7` | 11.4:1 |
| Names beside controls, panel titles | `#BFC5CE` | 8.6:1 |
| The message, hints, axis labels | `#B0B6C0` | 7.4:1 |
| A warning, written | `#FF9E96` | 7.6:1, beside a `#D2483F` mark |
| Menus and tooltips | `#2A2F37` edged `#3A404A` | lighter than the window, as Good Idea's final sweep set |

## The four that carry it

Everything else is support. These four are the scheme.

| | value | what it is |
| --- | --- | --- |
| Accent | `#FFF200` | whatever is switched on |
| Controls | `#A9AFBA` | buttons and slider grabs, raised off the ground |
| Ground | `#23272E` | the window behind everything |
| Ink | `#14171C` | the text on any button |

The accent goes on one thing only: the state that is on. A chosen button, a
ticked box, the slider grab being dragged, and the tune in the preview roll.
Nothing decorative takes it, which is what makes it read at a glance.

## The grey ramp

Eighteen greys, darkest to lightest.

| | R | G | B | used for |
| --- | ---: | ---: | ---: | --- |
| `#111419` | 17 | 20 | 25 | piano-roll ground |
| `#14171C` | 20 | 23 | 28 | the text on every button |
| `#1A1D23` | 26 | 29 | 35 | sunken frames, scrollbar track |
| `#1B1F25` | 27 | 31 | 37 | title bar, menu bar |
| `#1E2228` | 30 | 34 | 40 | roll beat lines |
| `#22262D` | 34 | 38 | 45 | frame hover, a menu item pressed |
| `#23272E` | 35 | 39 | 46 | the window chrome, active title |
| `#2A2F37` | 42 | 47 | 55 | menus and tooltips (a step lighter than the window, 16 October 2026), frame active |
| `#3A404A` | 58 | 64 | 74 | separators, roll bar lines, the edge of a menu, a menu item under the pointer |
| `#585F6B` | 88 | 95 | 107 | (no longer used: 2.6:1 as a scrollbar thumb) |
| `#6D7581` | 109 | 117 | 129 | scrollbar thumb and resize corner (3.6:1 on the track, 3.3:1 on the ground) |
| `#8A919C` | 138 | 145 | 156 | the edge of a tick box, the idea number and the swing slider (4.7:1); never words |
| `#8F96A2` | 143 | 150 | 162 | button held |
| `#A9AFBA` | 169 | 175 | 186 | buttons, slider grabs, chords and bass in the roll |
| `#B0B6C0` | 176 | 182 | 192 | secondary text: hints, labels, the status line (7.4:1) |
| `#BFC5CE` | 191 | 197 | 206 | the step numbers; a menu item's keys (Cmd+=), 7.2:1 on the menu |
| `#C0C6CF` | 192 | 198 | 207 | button hover |
| `#DDE1E7` | 221 | 225 | 231 | body text |
| `#F2F4F7` | 242 | 244 | 247 | the audition playhead, the keyboard's focus ring |

## One colour off the ramp

`#D2483F` - red, for warnings in the status line. **It
marks; it does not letter.** At 3.4:1 on the ground it is too faint for
words, so a warning's text is `#FF9E96` (7.6:1), the same red lightened, and
`#D2483F` is kept for marks that are not text. Deliberately not a shade of
the accent, so a warning can never read as a selection. It and the yellow are
the only saturated colours in the scheme.

## Hover and held

The two halves of the palette get their states differently, which matters if
you copy either.

| | rest | hover | held | how |
| --- | --- | --- | --- | --- |
| Controls | `#A9AFBA` | `#C0C6CF` | `#8F96A2` | set by hand in `THEME` |
| Accent | `#FFF200` | `#FFF42E` | `#D1C600` | computed by `shade()`, ±18% toward white and black |

So the source holds twenty values, while the running window shows twenty-two -
the accent's two states are derived rather than stored.

## Two rules worth carrying

**Keep the greys blue.** R < G < B holds in all eighteen. It is the least
obvious property here and the easiest to lose, because a neutral grey at the
same lightness looks perfectly correct in a diff and only reads as flat once it
is on screen beside the yellow. Compare `#A9AFBA` with a neutral `#B0B0B0`. The
greys in this window were neutral for a long time, which is why it is an easy
mistake to make twice. Taking the scheme elsewhere means taking the bias, not
the hexes - any cool ramp will do, a neutral one will not.

**Light controls force dark text.** Both button fills sit far lighter than the
ground, so the body text colour `#DDE1E7` vanishes on them. Every button takes
the ink, including the unchosen ones. This fails quietly: style the chosen state
alone and it looks right while every other control goes unreadable. Most dark
themes raise their buttons a shade off the chrome and letter them in the body
text colour; this one inverts that, so the habit is wrong here.

## Starting a new project from this

1. **Pick the ground first.** Dark, and cool rather than neutral. Stop short of
   black - flat black under a saturated accent reads as a hole, not a surface.
2. **Build the ramp off it**, keeping R < G < B the whole way. Everything
   structural comes out of the ramp: borders, frames, separators, scrollbars.
3. **Choose one accent and spend it only on what is on.** If a second thing
   wants the accent, it probably wants the ramp instead.
4. **Decide the ink before the controls.** If the controls end up lighter than
   the ground, every control needs dark text, and that reaches every button.
5. **Keep semantic colours out of the accent's hue**, so state and selection
   never look alike.

## The values

```css
:root {
  --accent:        #FFF200;
  --accent-hover:  #FFF42E;
  --accent-held:   #D1C600;
  --control:       #A9AFBA;
  --control-hover: #C0C6CF;
  --control-held:  #8F96A2;
  --ground:        #23272E;
  --sunken:        #1A1D23;
  --rule:          #3A404A;
  --ink:           #14171C;
  --text:          #DDE1E7;
  --text-dim:      #B0B6C0;
  --text-disabled: #8A919C;
  --roll:          #111419;
  --warn:          #D2483F;
  --warn-text:     #FF9E96;
}
```

Every ReaImGui `Col_` the window sets, for a project that wants the same window
rather than only the same scheme:

| `Col_` | |
| --- | --- |
| `Col_Border` | `#14171C` |
| `Col_Button` | `#A9AFBA` |
| `Col_ButtonActive` | `#8F96A2` |
| `Col_ButtonHovered` | `#C0C6CF` |
| `Col_CheckMark` | `#FFF200` |
| `Col_FrameBg` | `#1A1D23` |
| `Col_FrameBgActive` | `#2A2F37` |
| `Col_FrameBgHovered` | `#22262D` |
| `Col_NavHighlight` | `#F2F4F7` |
| `Col_PopupBg` | `#1B1F25` |
| `Col_ScrollbarBg` | `#1A1D23` |
| `Col_ScrollbarGrab` | `#585F6B` |
| `Col_ScrollbarGrabActive` | `#A9AFBA` |
| `Col_ScrollbarGrabHovered` | `#6D7581` |
| `Col_Separator` | `#3A404A` |
| `Col_SliderGrab` | `#A9AFBA` |
| `Col_SliderGrabActive` | `#FFF200` |
| `Col_Text` | `#DDE1E7` |
| `Col_TextDisabled` | `#8A919C` |
| `Col_TitleBg` | `#1B1F25` |
| `Col_TitleBgActive` | `#23272E` |
| `Col_TitleBgCollapsed` | `#1B1F25` |
| `Col_WindowBg` | `#23272E` |

## Contrast, after Apple's guidelines

Apple's Dark Mode page asks for 7:1 between small custom text and its ground
(4.5:1 at the least). On `#23272E`: body text `#DDE1E7` 11.4:1, secondary
`#B0B6C0` 7.4:1, step numbers `#BFC5CE` 8.6:1, warnings `#FF9E96` 7.6:1, and
the ink `#14171C` 8.2:1 on a grey button and 15.4:1 on the yellow. The old
dim `#8A919C` (4.7:1) and the house red (3.4:1) are kept for disabled text
and for marks, never for words that must be read. The keyboard's focus ring is
`#F2F4F7`, not the accent: yellow means *on*, and a focused control need not
be.

## The final HIG sweep (16 October 2026)

Measured, every value from the ramp, none new:

| what | was | now | contrast |
| --- | --- | --- | --- |
| Menus and tooltips | `#1B1F25`, darker than the window | `#2A2F37`, edge `#3A404A` | words 10:1 |
| Menu bar, highlighted menu item | ImGui's neutral grey and blue (words 3:1) | `#1B1F25`, `#3A404A`, pressed `#22262D` | words 7:1 or more |
| A menu item's keys | `#8A919C`, 4.2:1 on the menu | `#BFC5CE` | 7.2:1 |
| Scrollbar thumb, resize corner | `#585F6B` (2.6:1), ImGui's blue | `#6D7581` | 3.6:1 on the track |
| Tick boxes, the idea number, the swing slider | no edge (`#1A1D23` on the ground, 1.1:1) | a 1-point `#8A919C` edge | 4.7:1 |
