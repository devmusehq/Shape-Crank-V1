# Shape Crank Prototype Checkpoint

This folder is the current Shape Crank shape-only prototype checkpoint.

Date: 2026-05-25

## Prototype State

This checkpoint intentionally contains only the adjustable Shape curve.

Signal flow:

```text
Input
-> Shape curve
-> Output
```

There is no overdrive, boost, output Level stage, clipping, diode solver, amp simulation, cabinet simulation, compressor, noise gate, or visible EQ section in this prototype.

## Current Shape Curve

The Shape knob is a broad, subtractive guitar front-end curve:

- gentle variable high-pass tightening below the main guitar body
- broad low-mid wool control around 240 Hz
- broad upper-edge smoothing around 2.9 kHz
- gentle high shelf fizz control around 7 kHz

The knob range is scaled so the earlier full-strength curve sits around three-quarters, with the final quarter available for deeper amp-specific tightening.

Keep this checkpoint intact before adding the overdrive stage back.
