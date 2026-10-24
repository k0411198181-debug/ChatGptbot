KG Vocal Match v0.4
===================
Windows x64 VST3 by KG Music Records.

PURPOSE
Use a reference vocal stem (for example a Suno vocal stem) as the melodic guide and correct YOUR vocal toward it.

TWO SIMPLE MODES
1. OCTAVE SAFE
   Same melody as the reference, but keeps your natural register.
   Example: reference A4, you sing around A3 -> your voice is corrected toward A3, not forced to A4.
   This is the recommended musical mode when your voice sits an octave lower or higher.

2. EXACT UNISON
   Your vocal follows the absolute reference pitch.
   Use this when you already sing in roughly the same octave and want close unison.
   The mode allows up to one octave of correction, but very large shifts can sound synthetic.

REAPER ROUTING
1. Put KG Vocal Match on YOUR vocal track.
2. Set that track to 4 channels.
3. Your vocal stays on channels 1/2.
4. Send the REFERENCE vocal stem to channels 3/4 of your vocal track.
5. The plugin outputs only your corrected vocal.

VISIBLE CONTROLS
MATCH   - correction amount
NATURAL - protects consonants/unvoiced material by confidence gating
MIX     - processed/dry balance
OUTPUT  - final level
OCTAVE SAFE / EXACT UNISON - pitch mode
BYPASS - compare

ARTIFACT GUIDE
0-30 cents: usually transparent.
30-80 cents: normally natural with MATCH 70-100 and NATURAL 70-90.
80-150 cents: audible correction becomes possible; use NATURAL 80-95 and slower phrasing.
150-300 cents: artifact risk increases, especially on breathy or distorted vocals.
Over 300 cents: use only when necessary; rerecording closer to pitch is usually cleaner.
One full octave (1200 cents): mathematically possible in EXACT UNISON, but not expected to remain fully natural in this real-time granular engine.

The interface shows NATURAL CORRECTION ZONE / LARGE SHIFT / VERY LARGE SHIFT warnings based on current correction size.

Pitch analysis uses a YIN-style period detector with confidence filtering and octave protection.
This real-time version is not full offline Melodyne blob editing and does not yet provide formant-preserving resynthesis or automatic time alignment.
