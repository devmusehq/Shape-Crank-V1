# Shape Crank

Shape Crank is a DevMuse guitar pedal plugin for the front of an amp or amp sim. Shape tightens and smooths the guitar before gain, Overdrive adds grit through to full pedal saturation, and Level pushes the next stage with clean output gain.

## 0.2.0 development preview

This version keeps the original Shape curve and Level range, rebuilds the overdrive, adds two small curve trims and gives the editor a new pedal design.

- **Shape:** the original broad, subtractive curve. Zero is flat. The earlier full-strength curve still sits at about three-quarters.
- **Overdrive:** two soft saturation stages with 16x oversampling. A continuous gain taper gives the lower and middle parts of the dial more use. Zero leaves the shaped signal clean.
- **Level:** clean final gain, from -18 dB to +12 dB. Noon is unity. Its range and transfer function are unchanged.
- **Bass trim:** a broad 160 Hz low shelf, -6 to +6 dB.
- **Treble trim:** a broad 3.2 kHz high shelf, -6 to +6 dB.

Both trims default to 0 dB and sit after Shape, before Overdrive. They let you fine-tune the contour without changing the Shape control itself. Double-click a control to return it to its default. Shift-drag for fine adjustment; Tab and the arrow keys provide keyboard control. The amber light means the pedal is engaged.

The editor uses native vector drawing: petrol enamel, cream print, black main knobs, recessed metal trim pots and a metal footswitch. It scales from 384 x 595 to 720 x 1116, with control values below the knobs.

## Signal flow

```text
Input -> original Shape curve -> Bass / Treble trims -> Overdrive -> clean Level -> Output
```

The overdrive uses two cascaded soft saturation stages, a small asymmetric bias, low-end conditioning, interstage smoothing and DC removal. FIR oversampling filters reduce folded harmonics. The dry and bypass paths receive the same fixed integer delay, reported to the host, so changing Drive or bypass does not change latency. The delay is 67 samples, about 1.4 ms at 48 kHz. Pot changes ramp smoothly; bypass crossfades over 10 ms while the processing stays warm.

At zero Overdrive and neutral trims, the original Shape output passes through a pure delay. Level remains the final clean gain stage, including when its output exceeds 0 dBFS. Set it to suit the next amp or plugin.

## Sessions and automation

The existing plugin identity and first four parameter IDs, names, ranges and order are preserved: `shape`, `drive`, `level`, `bypass`. `bass` and `treble` are appended with a later AU version hint. Old sessions load with neutral trims, including when loaded after a preset that used the new trims.

Existing nonzero OD settings will sound different because the overdrive circuit and taper have changed. Keep the original 0.1.0 build available when you need an exact recall of an older driven sound.

## Build

Requires CMake 3.22+, a C++20 compiler and JUCE 8.0.12. For a reproducible dependency download:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSHAPECRANK_FETCH_JUCE=ON
cmake --build build --config Release --target ShapeCrankPlugin_VST3 ShapeCrankPlugin_Standalone --parallel
```

On macOS, `ShapeCrankPlugin_AU` builds the Audio Unit. To use an existing JUCE checkout, pass `-DJUCE_PATH=/path/to/JUCE` instead of the fetch option. `COPY_PLUGIN_AFTER_BUILD` is off; building does not replace an installed plugin.

## Verification

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSHAPECRANK_FETCH_JUCE=ON -DSHAPECRANK_BUILD_CHECKS=ON
cmake --build build --config Release --target ShapeCrankChecks --parallel
ctest --test-dir build -C Release --output-on-failure
```

The checks cover the clean endpoint and fixed latency; 35 frozen Shape responses from commit `ec1aa9d`; the original Level law; trim range and centres; drive progression, an aggregate aliasing stress test and folded harmonics; DC rejection; parameter order; old and new sessions; mono/stereo layouts; sample rates from 22.05 to 192 kHz; zero, one-sample and oversized blocks; automation; bypass; and editor parameter attachments.

The check executable can also render the actual native editor at three sizes and in bypass:

```sh
./build/ShapeCrankChecks --render /absolute/path/to/previews
```

## Downloads and license

[The published 0.1.0 prototype builds](https://github.com/devmusehq/Shape-Crank-V1/releases/tag/v0.1.0) remain available. This branch is a development preview of 0.2.0.

Shape Crank is free software under [GPLv3](LICENSE). The Shape Crank and DevMuse names, logos and official release branding do not grant permission to present a modified fork as an official DevMuse release.

The historical shape-only checkpoint is documented in [PROTOTYPE_CHECKPOINT.md](PROTOTYPE_CHECKPOINT.md).
