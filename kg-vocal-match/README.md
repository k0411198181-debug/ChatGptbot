KG Vocal Match v0.3
===================
Windows x64 VST3 by KG Music Records.

Purpose: compare a reference vocal stem (for example a Suno vocal stem) with your own vocal at the same musical timing, detect both fundamental pitches and correct YOUR vocal toward the reference pitch.

REAPER routing:
1. Put KG Vocal Match on YOUR vocal track.
2. Set that track to 4 channels.
3. Main vocal stays on channels 1/2.
4. Send the REFERENCE vocal stem to this track on channels 3/4 (sidechain).
5. The plugin outputs only the corrected main vocal.

Controls:
MATCH - correction strength
SPEED - pitch-correction smoothing
RANGE - maximum correction range in semitones
VIBRATO - how closely fine reference pitch/vibrato is followed
NATURAL - confidence gating / protection of consonants and unvoiced material
TOLERANCE - cents dead-zone
MIX / OUTPUT
EXACT - fast, exact reference-vibrato following
BYPASS - comparison

Pitch analysis runs about 100 measurements per second and includes confidence filtering and octave protection.
This real-time version does not provide Melodyne-style offline blob editing, formant-preserving resynthesis, or time-alignment/DTW; reference and live vocal should be aligned in REAPER.
