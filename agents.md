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

- `Crank`
- `Drive`
- `Level`

Optional only if already present and genuinely useful:

- `Shape`

Avoid adding more controls unless explicitly requested.

Do **not** add:

- Bass / Mid / Treble
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

### Crank

`Crank` is the main identity control.

It applies a broad, musical, pre-drive voicing curve and controlled pre-drive push.

As Crank increases:

- low end tightens
- flub is reduced before distortion
- useful mids come forward
- pick attack becomes clearer
- harsh top/fizz is controlled
- the shaped signal is pushed harder into the drive stage

Crank is **not** a hidden 7-band graphic EQ.

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
→ Crank-controlled broad pre-drive voicing curve
→ Pre-drive tightening / conditioning if needed
→ Drive / clipping stage
→ Final Level / output boost
→ Gentle safety limiter only
```
