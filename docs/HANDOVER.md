# Starting a fresh session

Paste the prompt below into a new Claude Code session on this repository
(branch `claude/vst3-sample-instrument-qnemym`, or `main` once it is merged).
It is kept here so it survives the session that wrote it; update it at the
end of each session.

---

I'm continuing work on **Instant Virtual Instrument**, my VST3 that makes a
whole virtual instrument from one sample - every key, dynamic layers from pp
to ff, round robins, and nine articulations - by spectral resynthesis. I'm a
musician, not a programmer, so please explain everything in musical terms
(what I'll hear, which keys, which layer), with numbers you've measured.

Where we are: **version 0.2.0**. 0.1.0 built the engine and the plugin; 0.2.0
brought the window into the house style of my other repositories (Good
Idea's colour scheme and its Apple HIG and writing rules, done in JUCE the way
Noterator does it). Read `CLAUDE.md` first, then `docs/decisions/` and the
latest `docs/sessions/` log.

Before changing anything: build and run both test suites (`CLAUDE.md`,
Tests). After a change to the sound, render the demo phrase
(`IVI_RENDER_DEMO=demo.wav`) and listen-check it with me; after a change to
the window, run `PluginTests` with `IVI_SNAPSHOT=shot.png` and look at the
100% and 200% screenshots. Prove new checks bite by breaking the code in a
copy of the tree. Keep the house rules: every colour from `Theme.h`, words
per decision 0004, a decision record for any real choice, and a session log
at the end.
