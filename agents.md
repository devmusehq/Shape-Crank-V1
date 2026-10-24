# AGENTS.md — Shape Crank

## Project Identity

Shape Crank is a DevMuse JUCE/C++ guitar audio plugin.

It is a simple gain/boost/amp-shaping pedal designed to sit before a real amp, high-gain amp, or amp sim.

It is **not** a Tube Screamer clone.
It is **not** a visible graphic EQ pedal.
It is **not** a full amp sim.
It is **not** a rack-style studio processor.

Core idea:

> Turn one knob until the guitar hits the amp properly.

Shape Crank should feel like a musical pedal, not a lab tool.

---

## Main Controls

The intended main controls are:

- `Shape`
- `Overdrive` (existing `drive` host parameter)
- `Level`

Explicitly requested small trim pots:

- `Bass trim`: broad 160 Hz shelf, -6 to +6 dB, default 0 dB
- `Treble trim`: broad 3.2 kHz shelf, -6 to +6 dB, default 0 dB

Avoid adding more controls unless explicitly requested.

Do **not** add:

- additional tone controls beyond the two requested trim pots
- visible 7-band EQ sliders
- noise gate
- compressor
- cab sim
- amp sim
- tuner
- pre/post EQ switch
- separate EQ volume
- separate overdrive volume

---

## Control Meaning

### Shape

`Shape` is the main identity control. Preserve its existing filter and response code.

It applies the existing broad, subtractive pre-drive voicing curve.

As Shape increases:

- low end tightens
- flub is reduced before distortion
- the core midrange stays intact
- pick attack becomes clearer
- harsh top/fizz is controlled

Shape is **not** a hidden 7-band graphic EQ.

It should behave like one smooth engineered curve.

### Drive

`Drive` controls the saturation/clipping amount.

It should avoid:

- cheap fuzz character
- uncontrolled low-end distortion
- harsh fizzy clipping
- huge volume jumps

### Level

`Level` is the final output/boost control.

It must be after the drive stage.

Inside the plugin, Level should be mostly clean output gain.

Externally, when feeding an amp or amp sim, Level naturally pushes that amp harder. That is intended.

Do not add hard clipping or tone-defining saturation after Level.

---

## Intended Signal Flow

Preferred signal flow:

```text
Input
→ Original Shape-controlled broad pre-drive voicing curve
→ Neutral-centred Bass and Treble shelf trims
→ Oversampled Drive / clipping stage
→ Final clean Level / output boost
→ Output
```
