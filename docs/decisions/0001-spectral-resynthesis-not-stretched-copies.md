# 0001. Spectral resynthesis, not stretched copies of the sample

Taken 2026-10-25. Stands.

## Context

The user asked for a VST3 that makes a whole virtual instrument from one
sample: every semitone, round robins on every key, dynamic layers from
pianissimo to fortissimo that change the timbre as well as the volume, and
articulations made automatically.

The usual way to spread one sample across a keyboard is to resample it:
play it faster for higher notes, slower for lower ones. That moves the body
of the instrument with the pitch (the "chipmunk" sound an octave up), stretches
the attack and the vibrato, and gives every key the same timbre. It cannot
make a softer or louder take of the note either, nor a pizzicato from a
bowed note.

## Decision

The sample is **analysed into a model and resynthesised**
(`Source/Core/SpectralAnalyzer.cpp`, `ResynthVoice.cpp`):

- YIN finds the pitch, frame by frame, so vibrato and scoops are kept as
  movement around the root, and the sample is played in tune whatever its
  own tuning.
- A short-time Fourier transform follows every harmonic partial through the
  sample, including stretched (inharmonic) ones, as a piano's are.
- Each partial's level is split three ways: a **source slope** that moves
  with pitch, a **body resonance** fixed in frequency (the formants), and a
  **fine structure** tied to the harmonic number (a clarinet's weak even
  harmonics). Formant (0 to 100%) chooses how much of the body stays put.
- What is not a partial (bow, breath, hammer) is kept as noise in 24 bands.
- The attack is re-sharpened from the original, and the attack, the sustain
  loop and the release are found; a sound that dies away is marked as
  plucked or struck.

A dynamic layer is the same model with a different brightness tilt, attack
speed, noise balance and level; an articulation is a different way of
playing the model through time.

## Consequences

- Measured on a synthetic string tone (`Tests/CoreTests.cpp`): analysed
  harmonics within 0.03 dB, the root key resynthesised within 0.5 dB of the
  source (1/3-octave bands), every key within 4 cents.
- The engine is for **pitched** sounds. A drum or a noise loads, but has no
  partials to move and plays as C4.
- Very large moves from the root (several octaves) sound more synthetic than
  a sampled instrument would, because the model extrapolates partials the
  sample never had.
- First tries found, and the tests now hold: a noise band hidden under the
  fundamental falling back to the partial's own energy (20 dB too loud,
  fixed by interpolating from its neighbours); high keys losing their
  harmonics while the whole spectrum was tied to absolute frequency (fixed
  by separating the source slope); a stretched spectrum's pitch read 20
  cents sharp by YIN (fixed by taking the pitch from the first partial).
