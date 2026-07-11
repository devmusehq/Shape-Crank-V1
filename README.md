# Shape Crank

## Plain-English Version

Shape Crank is a guitar tone-shaping plugin. Put it before an amp, amp sim, or other gain stage and use it like a simple pedal: turn the main shape control until the guitar feels tighter, clearer, and better prepared for distortion.

It is designed to remove the messy parts of a raw guitar signal before they hit the amp. That means less flubby low end, less harsh fizz, clearer pick attack, and a more focused sound without making you adjust a complicated EQ. The extra drive control can add some pedal-style grit, and the level control lets you push the next thing in the chain harder.

In short: Shape Crank helps a guitar hit an amp properly.

## Download

The current prototype VST3 builds are available from the GitHub Releases page:

- [Download Shape Crank v0.1.0 for macOS and Windows](https://github.com/devmusehq/Shape-Crank-V1/releases/tag/v0.1.0)

Install the VST3 by extracting the zip and copying `Shape Crank.vst3` into your normal VST3 plugin folder, then restart or rescan your DAW.

Shape Crank is currently a DevMuse JUCE/C++ shape-and-OD prototype.

The shape-only prototype was preserved before the OD stage was added back.

The shape-only checkpoint lives beside this project at:

```text
../Shape Crank - Shape Only Prototype
```

## Signal Flow

```text
Input
-> Shape curve
-> OD stage
-> clean Level
-> Output
```

There is no diode solver, amp simulation, cabinet simulation, compressor, noise gate, or visible EQ section in this prototype.

## Shape Control

`shape` controls one continuous curve with an eased/exponential pot response. At zero it is flat. As it rises, broad low tightening and broad high smoothing increase together. The knob is scaled so the earlier full-strength curve sits around three-quarters, leaving the final quarter for deeper amp-specific tightening.

The current curve is based on a recording front-end move: clean up the DI before it hits a gain stage, while preserving the core guitar midrange.

It uses:

- gentle variable high-pass tightening below the main guitar body
- broad low-mid control for the woolly 220-260 Hz mass
- broad upper-edge control around the harsh 2.5-4 kHz region
- gentle high shelf to smooth fizz around 6-8 kHz and above

The curve is subtractive. It does not boost mids, and it is not exposed as individual EQ bands. It should be tuned by ear as one musical contour.

## OD Control

`drive` / `OD` is a simple musical soft-clipping stage after Shape.

At zero, the OD path is bypassed and the signal remains Shape-only. As OD rises, a wet/dry blend brings in a lightly high-passed, asymmetric soft clip. The clipping stage uses a rounded tanh/atan hybrid for a softer shoulder, with enough range at the top of the control to push into stronger pedal drive.

There is no hard clipping, diode/op-amp solver, final saturation, or makeup gain after the output.

## Level Control

`level` / `Level` is clean final output gain after the OD stage.

No tone shaping, clipping, saturation, or limiting is attached to Level. Noon is unity gain, with trim below noon and clean amp-front-end push above noon.

## Build

From this folder:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target ShapeCrankPlugin_Standalone
```

By default, `CMakeLists.txt` expects JUCE at:

```text
../DoomEngine/VoidLoomJUCE/JUCE
```

You can override that path:

```sh
cmake -S . -B build -DJUCE_PATH=/path/to/JUCE
```

The project can also build VST3 and AU targets through CMake/JUCE.
