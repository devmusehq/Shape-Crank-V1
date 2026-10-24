#include "ShapeCrankProcessor.h"

#include <chrono>
#include <complex>
#include <functional>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace
{
using Processor = shapecrank::ShapeCrankAudioProcessor;
namespace IDs = shapecrank::ParamIDs;
constexpr double sampleRate = 48000.0;
constexpr double twoPi = juce::MathConstants<double>::twoPi;

void require(bool condition, const std::string& message)
{
    if (! condition)
        throw std::runtime_error(message);
}

void set(Processor& processor, const char* id, float value)
{
    auto* parameter = processor.parameters.getParameter(id);
    require(parameter != nullptr, std::string("Missing parameter: ") + id);
    parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
}

float get(Processor& processor, const char* id)
{
    return processor.parameters.getRawParameterValue(id)->load();
}

using Settings = std::initializer_list<std::pair<const char*, float>>;

std::unique_ptr<Processor> prepared(Settings settings = {}, double rate = sampleRate,
                                    int channels = 2, int blockSize = 256)
{
    auto processor = std::make_unique<Processor>();
    juce::AudioProcessor::BusesLayout layout;
    const auto channelSet = channels == 1 ? juce::AudioChannelSet::mono() : juce::AudioChannelSet::stereo();
    layout.inputBuses.add(channelSet);
    layout.outputBuses.add(channelSet);
    require(processor->setBusesLayout(layout), "Requested layout rejected");
    for (const auto& [id, value] : settings)
        set(*processor, id, value);
    processor->prepareToPlay(rate, blockSize);
    return processor;
}

juce::AudioBuffer<float> sine(double hz, double amplitude = 0.1, int samples = 96000,
                              int channels = 2, double rate = sampleRate)
{
    juce::AudioBuffer<float> buffer(channels, samples);
    for (int channel = 0; channel < channels; ++channel)
        for (int sample = 0; sample < samples; ++sample)
            buffer.setSample(channel, sample, (float) (amplitude * std::sin(twoPi * hz * sample / rate)));
    return buffer;
}

void process(Processor& processor, juce::AudioBuffer<float>& buffer, int blockSize = 256, bool hostBypass = false)
{
    juce::MidiBuffer midi;
    for (int offset = 0; offset < buffer.getNumSamples(); offset += blockSize)
    {
        const auto samples = std::min(blockSize, buffer.getNumSamples() - offset);
        std::array<float*, 2> pointers {};
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            pointers[(size_t) channel] = buffer.getWritePointer(channel, offset);
        juce::AudioBuffer<float> block(pointers.data(), buffer.getNumChannels(), samples);
        if (hostBypass)
            processor.processBlockBypassed(block, midi);
        else
            processor.processBlock(block, midi);
    }
}

double rms(const juce::AudioBuffer<float>& buffer, int start = 48000)
{
    double sum = 0.0;
    for (int sample = start; sample < buffer.getNumSamples(); ++sample)
    {
        const double value = buffer.getSample(0, sample);
        sum += value * value;
    }
    return std::sqrt(sum / (buffer.getNumSamples() - start));
}

double component(const juce::AudioBuffer<float>& buffer, double hz, int start = 48000,
                 double rate = sampleRate)
{
    std::complex<double> sum {};
    for (int sample = start; sample < buffer.getNumSamples(); ++sample)
        sum += (double) buffer.getSample(0, sample) * std::polar(1.0, -twoPi * hz * sample / rate);
    return 2.0 * std::abs(sum) / (buffer.getNumSamples() - start);
}

double response(double hz, Settings settings)
{
    auto processor = prepared(settings);
    auto buffer = sine(hz);
    process(*processor, buffer);
    return 20.0 * std::log10(rms(buffer) / (0.1 / std::sqrt(2.0)));
}

void checkTransparentAndLevel()
{
    for (const auto level : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        auto processor = prepared({ { IDs::level, level } });
        const auto latency = processor->getLatencySamples();
        require(latency > 0 && latency < 256, "Oversampling latency must be fixed and small");
        if (level == 0.5f)
            std::cout << "  Fixed latency: " << latency << " samples at 48 kHz\n";
        auto input = sine(997.0, 0.75, 8192);
        juce::AudioBuffer<float> output(input);
        process(*processor, output, 511);
        const auto db = level <= 0.5f ? -18.0f + 36.0f * level : (level - 0.5f) * 24.0f;
        const auto gain = std::pow(10.0f, db / 20.0f);
        double error = 0;
        for (int sample = latency; sample < output.getNumSamples(); ++sample)
            error = std::max(error, std::abs((double) output.getSample(0, sample)
                                 - input.getSample(0, sample - latency) * gain));
        require(error < 0.000002, "Clean endpoint/Level changed, max error " + std::to_string(error));
        if (level == 1.0f)
            require(output.getMagnitude(0, output.getNumSamples()) > 2.9f, "Level must remain clean above 0 dBFS");
    }
}

void checkOriginalShape()
{
    // Frozen measurements from the unmodified ec1aa9d processor, 48 kHz, OD=0.
    // The baseline was generated from that commit, not from the current filter.
    constexpr std::array<double, 7> frequencies { 60, 100, 240, 1000, 2900, 7000, 12000 };
    constexpr std::array<std::array<double, 7>, 5> reference {{
        { 0, 0, 0, 0, 0, 0, 0 },
        { -4.25011569, -1.24986991, -0.34795753, -0.08473136, -0.16005872, -0.18684067, -0.23924026 },
        { -9.23347847, -3.70308302, -1.38821397, -0.39134841, -0.76800458, -0.89804461, -1.14979670 },
        { -14.89674542, -7.85629200, -2.97915306, -0.79599256, -1.55891558, -1.82021207, -2.32783120 },
        { -17.31984954, -9.99680451, -3.92003889, -1.00583023, -1.96337729, -2.28967632, -2.92563460 }
    }};
    for (size_t shape = 0; shape < reference.size(); ++shape)
        for (size_t frequency = 0; frequency < frequencies.size(); ++frequency)
        {
            const auto measured = response(frequencies[frequency], { { IDs::shape, (float) shape * 0.25f } });
            require(std::abs(measured - reference[shape][frequency]) < 0.003,
                    "Shape baseline changed at " + std::to_string(frequencies[frequency]) + " Hz");
        }
}

void checkTrims()
{
    for (const auto sign : { -1.0f, 1.0f })
    {
        const auto bassLow = response(40, { { IDs::bass, 6.0f * sign } });
        const auto bassHigh = response(12000, { { IDs::bass, 6.0f * sign } });
        const auto trebleHigh = response(12000, { { IDs::treble, 6.0f * sign } });
        const auto trebleLow = response(100, { { IDs::treble, 6.0f * sign } });
        require(bassLow * sign > 5.0 && bassLow * sign < 6.1, "Bass trim must reach a useful broad shelf");
        require(std::abs(bassHigh) < 0.02, "Bass trim changes the top end");
        require(trebleHigh * sign > 5.0 && trebleHigh * sign < 6.1, "Treble trim must reach a useful broad shelf");
        require(std::abs(trebleLow) < 0.02, "Treble trim changes the low end");
    }
    require(std::abs(response(160, { { IDs::bass, 6 } }) - 3.0) < 0.01, "Bass shelf centre must be 160 Hz");
    require(std::abs(response(3200, { { IDs::treble, 6 } }) - 3.0) < 0.01, "Treble shelf centre must be 3.2 kHz");
}

void checkDrive()
{
    double previousTHD = -1;
    double previousLevel = 0;
    for (const auto drive : { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 1.0f })
    {
        auto processor = prepared({ { IDs::drive, drive } });
        auto buffer = sine(997, 0.1);
        process(*processor, buffer);
        const auto fundamental = component(buffer, 997);
        double harmonics = 0;
        for (int harmonic = 2; harmonic <= 10; ++harmonic)
            harmonics += std::pow(component(buffer, 997 * harmonic), 2.0);
        const auto thd = std::sqrt(harmonics) / fundamental;
        const auto db = 20 * std::log10(rms(buffer) / (0.1 / std::sqrt(2.0)));
        std::cout << "  OD " << drive * 100 << "%: THD " << thd * 100 << "%, RMS change " << db << " dB\n";
        require(thd + 0.00001 >= previousTHD, "Drive distortion must rise across the dial");
        require(drive < 0.1f || thd > 0.002, "The lower drive range must add audible harmonics");
        require(drive < 1.0f || thd > 0.20, "Full drive must provide substantial saturation");
        require(db < 7.0 && (drive == 0.0f || db - previousLevel < 5.0), "Drive has an excessive nominal level jump");
        double mean = 0;
        for (int sample = 48000; sample < 96000; ++sample)
            mean += buffer.getSample(0, sample) / 48000.0;
        require(std::abs(mean) < 0.00002, "OD leaves residual DC on a steady tone");
        previousTHD = thd;
        previousLevel = db;
    }

    auto processor = prepared({ { IDs::drive, 1.0f } });
    auto highTone = sine(7331, 0.2);
    process(*processor, highTone);
    const auto fundamental = component(highTone, 7331);
    // Folded fifth and seventh harmonics are both in band at 48 kHz.
    const auto alias = std::hypot(component(highTone, 11345), component(highTone, 3317));
    const auto aliasDb = 20 * std::log10(alias / fundamental);
    std::cout << "  Folded 5th/7th harmonics at full OD: " << aliasDb << " dBc\n";
    require(aliasDb < -60, "Oversampled OD must suppress folded high harmonics");
    double mean = 0;
    for (int sample = 48000; sample < 96000; ++sample)
        mean += highTone.getSample(0, sample) / 48000.0;
    double legitimatePower = mean * mean;
    for (int harmonic = 1; harmonic <= 3; ++harmonic)
        legitimatePower += 0.5 * std::pow(component(highTone, 7331 * harmonic), 2.0);
    // Every other bin is unwanted for this coherent steady sine. Subtracting
    // legitimate harmonic power catches folds created inside the oversampler,
    // including high-order folds missed by the two selected probes above.
    const auto residualPower = std::max(0.0, std::pow(rms(highTone), 2.0) - legitimatePower);
    const auto residualDb = 10 * std::log10(std::max(1.0e-30, residualPower) / (0.5 * fundamental * fundamental));
    std::cout << "  All non-harmonic residual at full OD: " << residualDb << " dBc\n";
    std::cout << "  Internal-rate folds at 8725/5937 Hz: "
              << 20 * std::log10(component(highTone, 8725) / fundamental) << "/"
              << 20 * std::log10(component(highTone, 5937) / fundamental) << " dBc\n";
    require(residualDb < -65, "High-drive internal-rate folding must stay below -65 dBc in the stress test");
}

void checkState()
{
    auto processor = prepared();
    const std::array<juce::String, 6> order { "shape", "drive", "level", "bypass", "bass", "treble" };
    const auto& parameters = processor->getParameters();
    require(parameters.size() == 6, "Unexpected parameter count");
    for (int index = 0; index < parameters.size(); ++index)
    {
        const auto* parameter = dynamic_cast<juce::AudioProcessorParameterWithID*>(parameters[index]);
        require(parameter != nullptr && parameter->paramID == order[(size_t) index], "Existing parameter order changed");
        require(index < 4 || parameter->getVersionHint() > 0, "New AU params need a later version hint");
    }
    for (const auto& [id, value] : Settings { { IDs::shape, 0.63f }, { IDs::drive, 0.42f },
                                             { IDs::level, 0.71f }, { IDs::bass, -3.2f }, { IDs::treble, 4.1f } })
        set(*processor, id, value);
    juce::MemoryBlock state;
    processor->getStateInformation(state);
    auto restored = prepared();
    restored->setStateInformation(state.getData(), (int) state.getSize());
    for (const auto* id : { IDs::shape, IDs::drive, IDs::level, IDs::bypass, IDs::bass, IDs::treble })
        require(std::abs(get(*processor, id) - get(*restored, id)) < 0.00001f, "State round-trip failed");

    juce::ValueTree legacy("Parameters");
    for (const auto& [id, value] : Settings { { IDs::shape, 0.637f }, { IDs::drive, 0.289f },
                                             { IDs::level, 0.731f }, { IDs::bypass, 1.0f } })
    {
        juce::ValueTree parameter("PARAM");
        parameter.setProperty("id", id, nullptr);
        parameter.setProperty("value", value, nullptr);
        legacy.appendChild(parameter, nullptr);
    }
    juce::MemoryBlock legacyState;
    juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), legacyState);
    restored->setStateInformation(legacyState.getData(), (int) legacyState.getSize());
    require(std::abs(get(*restored, IDs::bass)) < 0.00001f && std::abs(get(*restored, IDs::treble)) < 0.00001f,
            "Old sessions must neutralise previously changed trims");
    require(std::abs(get(*restored, IDs::shape) - 0.637f) < 0.00001f
         && std::abs(get(*restored, IDs::drive) - 0.289f) < 0.00001f
         && std::abs(get(*restored, IDs::level) - 0.731f) < 0.00001f
         && get(*restored, IDs::bypass) > 0.5f, "Old session settings changed");
}

void checkLayoutsAndBlocks()
{
    for (const auto rate : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        for (const auto channels : { 1, 2 })
        {
            auto processor = prepared({ { IDs::shape, 0.8f }, { IDs::drive, 0.9f },
                                        { IDs::bass, 6 }, { IDs::treble, -6 }, { IDs::level, 1 } }, rate, channels, 64);
            auto buffer = sine(997, 0.5, 8193, channels, rate);
            process(*processor, buffer, 4097); // Host exceeds the promised 64 samples.
            require(std::isfinite(rms(buffer, 4096)) && buffer.getMagnitude(0, buffer.getNumSamples()) < 8.0f,
                    "Non-finite or unstable output for a supported layout/rate");
            if (channels == 2)
                for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
                    require(buffer.getSample(0, sample) == buffer.getSample(1, sample), "Stereo channels diverge");
        }
    auto processor = prepared({ { IDs::drive, 1 } }, sampleRate, 2, 1);
    auto oneSample = sine(997, 0.2, 1025);
    oneSample.clear(1, 0, oneSample.getNumSamples());
    process(*processor, oneSample, 1);
    require(oneSample.getMagnitude(1, 0, oneSample.getNumSamples()) == 0.0f, "Stereo crosstalk or silence offset");
    juce::AudioBuffer<float> zeroBlock(2, 0);
    juce::MidiBuffer midi;
    processor->processBlock(zeroBlock, midi);
}

void checkAutomationAndBypass()
{
    auto processor = prepared();
    auto constant = [](int samples)
    {
        juce::AudioBuffer<float> buffer(2, samples);
        for (int channel = 0; channel < 2; ++channel)
            for (int sample = 0; sample < samples; ++sample)
                buffer.setSample(channel, sample, 0.05f);
        return buffer;
    };
    auto warmup = constant(4096);
    process(*processor, warmup);
    set(*processor, IDs::level, 1);
    auto ramp = constant(4096);
    process(*processor, ramp, 512);
    double largestStep = 0;
    for (int sample = 1; sample < ramp.getNumSamples(); ++sample)
        largestStep = std::max(largestStep, std::abs((double) ramp.getSample(0, sample) - ramp.getSample(0, sample - 1)));
    require(largestStep < 0.0004, "Level automation steps at block edges");

    for (const bool hostBypass : { false, true })
    {
        if (! hostBypass) set(*processor, IDs::bypass, 1);
        auto bypass = constant(4096);
        process(*processor, bypass, 511, hostBypass);
        for (int sample = 2048; sample < bypass.getNumSamples(); ++sample)
            require(std::abs(bypass.getSample(0, sample) - 0.05f) < 0.000001, "Bypass must ignore Level and remain raw");
        for (int sample = 1; sample < bypass.getNumSamples(); ++sample)
            require(std::abs(bypass.getSample(0, sample) - bypass.getSample(0, sample - 1)) < 0.0005,
                    "Bypass crossfade has a discontinuity");
        set(*processor, IDs::bypass, 0);
        auto engaged = constant(4096);
        process(*processor, engaged);
    }
    for (const bool hostBypass : { false, true })
    {
        auto bypassProcessor = prepared({ { IDs::bypass, hostBypass ? 0.0f : 1.0f },
                                           { IDs::shape, 1 }, { IDs::drive, 1 }, { IDs::level, 1 } });
        auto inputTone = sine(997, 0.2, 8192);
        juce::AudioBuffer<float> outputTone(inputTone);
        process(*bypassProcessor, outputTone, 511, hostBypass);
        const auto delay = bypassProcessor->getLatencySamples();
        for (int sample = 2048; sample < outputTone.getNumSamples(); ++sample)
            require(outputTone.getSample(0, sample) == inputTone.getSample(0, sample - delay),
                    "Bypass must match the reported integer delay exactly");
    }
    // Re-enter a clean endpoint after saturation, automation and bypass.
    set(*processor, IDs::drive, 1);
    set(*processor, IDs::bass, 6);
    set(*processor, IDs::treble, -6);
    auto driven = sine(997, 0.2, 8192);
    process(*processor, driven, 31);
    set(*processor, IDs::drive, 0);
    set(*processor, IDs::bass, 0);
    set(*processor, IDs::treble, 0);
    set(*processor, IDs::level, 0.5f);
    auto input = sine(997, 0.2, 16384);
    juce::AudioBuffer<float> clean(input);
    process(*processor, clean, 31);
    const auto latency = processor->getLatencySamples();
    for (int sample = 10000; sample < clean.getNumSamples(); ++sample)
        require(std::abs(clean.getSample(0, sample) - input.getSample(0, sample - latency)) < 0.000002,
                "Returning pots to neutral leaves stale processing");
}

void checkEditor(const juce::File& destination)
{
    auto processor = prepared({ { IDs::shape, 0.67f }, { IDs::drive, 0.43f },
                                { IDs::level, 0.58f }, { IDs::bass, -1.5f }, { IDs::treble, 2.0f } });
    auto editor = std::unique_ptr<juce::AudioProcessorEditor>(processor->createEditor());
    const std::array<juce::String, 5> names { "Shape", "Overdrive", "Level", "Bass trim", "Treble trim" };
    const std::array<const char*, 5> ids { IDs::shape, IDs::drive, IDs::level, IDs::bass, IDs::treble };
    for (size_t i = 0; i < names.size(); ++i)
    {
        juce::Slider* found = nullptr;
        for (auto* child : editor->getChildren())
            if (child->getName() == names[i]) found = dynamic_cast<juce::Slider*>(child);
        require(found != nullptr && found->getWantsKeyboardFocus(), "Missing accessible editor control");
        require(found->getExplicitFocusOrder() == (int) i + 1, "Unexpected keyboard focus order");
        require(found->getTextFromValue(found->getValue()).contains(i < 2 ? "%" : "dB"),
                "Editor readout lost its percentage or dB units");
        for (const auto fraction : { 0.0, 0.25, 0.5, 0.75, 1.0 })
        {
            const auto value = found->getMinimum() + fraction * (found->getMaximum() - found->getMinimum());
            require(std::abs(found->getValueFromText(found->getTextFromValue(value)) - value) < 0.0001,
                    "Accessible text input does not invert the displayed value");
        }
        const auto old = found->getValue();
        const auto testValue = i < 3 ? 0.25 : 3.0;
        found->setValue(testValue, juce::sendNotificationSync);
        require(std::abs(get(*processor, ids[i]) - testValue) < 0.0001, "Editor control is not attached to audio parameter");
        const auto now = juce::Time::getCurrentTime();
        const auto point = found->getLocalBounds().getCentre().toFloat();
        found->mouseDoubleClick(juce::MouseEvent(juce::Desktop::getInstance().getMainMouseSource(), point,
            juce::ModifierKeys::leftButtonModifier, 1.0f, 0, 0, 0, 0, found, found, now, point, now, 2, false));
        require(std::abs(get(*processor, ids[i]) - (i == 2 ? 0.5 : 0.0)) < 0.0001,
                "Double-click must return the audio parameter to its default");
        found->setValue(old, juce::sendNotificationSync);
    }
    // Exercise the actual mouse handlers so a documented Shift modifier cannot
    // silently behave like an ordinary coarse drag.
    auto* shape = dynamic_cast<juce::Slider*>(editor->getChildComponent(0));
    for (auto* child : editor->getChildren())
        if (child->getName() == "Shape") shape = dynamic_cast<juce::Slider*>(child);
    require(shape != nullptr, "Shape slider missing");
    const auto previousShape = shape->getValue();
    shape->setPopupDisplayEnabled(false, false, nullptr);
    const auto drag = [&](bool fine)
    {
        shape->setValue(0.5, juce::sendNotificationSync);
        const auto source = juce::Desktop::getInstance().getMainMouseSource();
        const auto origin = shape->getLocalBounds().getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        const auto modifiers = juce::ModifierKeys(juce::ModifierKeys::leftButtonModifier
                                | (fine ? juce::ModifierKeys::shiftModifier : 0));
        const auto event = [&](juce::Point<float> position, bool moved)
        {
            return juce::MouseEvent(source, position, modifiers, 1.0f, 0, 0, 0, 0,
                                    shape, shape, now, origin, now, 1, moved);
        };
        shape->mouseDown(event(origin, false));
        shape->mouseDrag(event(origin.translated(0.0f, -20.0f), true));
        shape->mouseUp(event(origin.translated(0.0f, -20.0f), true));
        return shape->getValue() - 0.5;
    };
    const auto coarse = drag(false);
    const auto fine = drag(true);
    require(coarse > 0.05 && fine > 0 && fine < coarse * 0.25, "Shift-drag must provide fine adjustment");
    shape->setValue(previousShape, juce::sendNotificationSync);
    if (destination == juce::File()) return;
    require(destination.createDirectory().wasOk(), "Cannot create snapshot folder");
    const auto save = [&](const juce::String& name, int width, int height)
    {
        editor->setSize(width, height);
        const auto image = editor->createComponentSnapshot(editor->getLocalBounds(), true, 2.0f);
        auto stream = destination.getChildFile(name).createOutputStream();
        require(stream != nullptr && stream->setPosition(0) && stream->truncate().wasOk()
                 && juce::PNGImageFormat().writeImageToStream(image, *stream), "Cannot write snapshot");
    };
    save("Shape-Crank-pedal.png", 480, 744);
    save("Shape-Crank-small.png", 384, 595);
    save("Shape-Crank-large.png", 720, 1116);
    editor.reset();
    set(*processor, IDs::bypass, 1);
    editor.reset(processor->createEditor());
    save("Shape-Crank-bypassed.png", 480, 744);
}

void checkProcessingCost()
{
    auto processor = prepared({ { IDs::shape, 0.75f }, { IDs::drive, 1 },
                                { IDs::bass, 3 }, { IDs::treble, 2 } }, sampleRate, 2, 256);
    const auto input = sine(997, 0.3, 256);
    juce::AudioBuffer<float> working(2, 256);
    juce::MidiBuffer midi;
    constexpr int blocks = 1000;
    const auto start = std::chrono::steady_clock::now();
    for (int block = 0; block < blocks; ++block)
    {
        for (int channel = 0; channel < 2; ++channel)
            working.copyFrom(channel, 0, input, channel, 0, 256);
        processor->processBlock(working, midi);
    }
    const auto seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
    const auto audioSeconds = blocks * 256.0 / sampleRate;
    std::cout << "  Stereo/full OD CPU time: " << seconds * 100.0 / audioSeconds
              << "% of real time (256-sample blocks, this machine)\n";
    require(std::isfinite(rms(working, 0)), "Processing benchmark became unstable");
}
} // namespace

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    std::cout << std::fixed << std::setprecision(3);
    int failed = 0;
    const auto run = [&](const char* name, const std::function<void()>& check)
    {
        try { check(); std::cout << "PASS: " << name << '\n'; }
        catch (const std::exception& error) { ++failed; std::cerr << "FAIL: " << name << ": " << error.what() << '\n'; }
    };
    const auto started = std::chrono::steady_clock::now();
    run("Transparent clean endpoint, fixed latency and original Level law", checkTransparentAndLevel);
    run("Original Shape: 35 frozen frequency responses", checkOriginalShape);
    run("Bass and treble shelf range, centres and isolation", checkTrims);
    run("Drive progression, DC rejection and alias suppression", checkDrive);
    run("Parameter identity, session round-trip and legacy session loading", checkState);
    run("Mono/stereo, sample rates, silence and variable host block sizes", checkLayoutsAndBlocks);
    run("Level automation, pedal/host bypass and return to clean", checkAutomationAndBypass);
    juce::File snapshots;
    if (argc == 3 && std::string(argv[1]) == "--render") snapshots = juce::File(juce::String(argv[2]));
    run("Editor attachments, keyboard focus and snapshots", [&] { checkEditor(snapshots); });
    run("Sustained real-time processing probe", checkProcessingCost);
    const auto elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - started).count();
    std::cout << (failed == 0 ? "All checks passed" : "Checks failed") << " in " << elapsed << " seconds\n";
    return failed == 0 ? 0 : 1;
}
