#include "ShapeCrankProcessor.h"
#include "ShapeCrankEditor.h"

namespace shapecrank
{
namespace
{
constexpr std::array<double, shapeFilterCount> shapeFrequencies { 135.0, 240.0, 2900.0, 7000.0 };
constexpr auto pi = 3.14159265358979323846;
constexpr size_t overdriveOversamplingStages = 4; // 2^4 = 16x

float readParameter(const std::atomic<float>* value)
{
    if (value != nullptr)
        return value->load(std::memory_order_relaxed);

    return 0.0f;
}
} // namespace

void ShapeCurveFilter::prepare(double newSampleRate, double newFrequency)
{
    sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;
    frequency = std::clamp(newFrequency, 20.0, sampleRate * 0.45);
    setCoefficients(0.0f, 0.7, FilterType::peak);
    reset();
}

void ShapeCurveFilter::reset()
{
    z1 = 0.0;
    z2 = 0.0;
}

void ShapeCurveFilter::setCoefficients(float gainDb, double qOrSlope, FilterType filterType)
{
    const auto clampedGain = std::clamp((double) gainDb, -12.0, 6.0);
    const auto safeShape = std::clamp(qOrSlope, 0.35, 1.4);
    const auto omega = 2.0 * pi * std::clamp(frequency, 20.0, sampleRate * 0.45) / sampleRate;
    const auto sine = std::sin(omega);
    const auto cosine = std::cos(omega);
    const auto amplitude = std::pow(10.0, clampedGain / 40.0);

    auto rawB0 = 1.0;
    auto rawB1 = 0.0;
    auto rawB2 = 0.0;
    auto rawA0 = 1.0;
    auto rawA1 = 0.0;
    auto rawA2 = 0.0;

    if (filterType == FilterType::highPass)
    {
        const auto amount = std::clamp((double) gainDb, 0.0, 1.3);
        if (amount < 0.0001)
        {
            b0 = 1.0;
            b1 = 0.0;
            b2 = 0.0;
            a1 = 0.0;
            a2 = 0.0;
            return;
        }

        const auto cutoff = 55.0 + (std::clamp(frequency, 80.0, 160.0) - 55.0) * amount;
        const auto highPassOmega = 2.0 * pi * cutoff / sampleRate;
        const auto highPassSine = std::sin(highPassOmega);
        const auto highPassCosine = std::cos(highPassOmega);
        const auto alpha = highPassSine / (2.0 * safeShape);

        rawB0 = (1.0 + highPassCosine) * 0.5;
        rawB1 = -(1.0 + highPassCosine);
        rawB2 = (1.0 + highPassCosine) * 0.5;
        rawA0 = 1.0 + alpha;
        rawA1 = -2.0 * highPassCosine;
        rawA2 = 1.0 - alpha;
    }
    else if (filterType == FilterType::peak)
    {
        const auto alpha = sine / (2.0 * safeShape);
        rawB0 = 1.0 + alpha * amplitude;
        rawB1 = -2.0 * cosine;
        rawB2 = 1.0 - alpha * amplitude;
        rawA0 = 1.0 + alpha / amplitude;
        rawA1 = -2.0 * cosine;
        rawA2 = 1.0 - alpha / amplitude;
    }
    else
    {
        const auto shelfSlope = safeShape;
        const auto shelfRoot = std::sqrt((amplitude + 1.0 / amplitude) * (1.0 / shelfSlope - 1.0) + 2.0);
        const auto alpha = sine * 0.5 * shelfRoot;
        const auto beta = 2.0 * std::sqrt(amplitude) * alpha;

        if (filterType == FilterType::highShelf)
        {
            rawB0 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine + beta);
            rawB1 = -2.0 * amplitude * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
            rawB2 = amplitude * ((amplitude + 1.0) + (amplitude - 1.0) * cosine - beta);
            rawA0 = (amplitude + 1.0) - (amplitude - 1.0) * cosine + beta;
            rawA1 = 2.0 * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
            rawA2 = (amplitude + 1.0) - (amplitude - 1.0) * cosine - beta;
        }
        else
        {
            rawB0 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine + beta);
            rawB1 = 2.0 * amplitude * ((amplitude - 1.0) - (amplitude + 1.0) * cosine);
            rawB2 = amplitude * ((amplitude + 1.0) - (amplitude - 1.0) * cosine - beta);
            rawA0 = (amplitude + 1.0) + (amplitude - 1.0) * cosine + beta;
            rawA1 = -2.0 * ((amplitude - 1.0) + (amplitude + 1.0) * cosine);
            rawA2 = (amplitude + 1.0) + (amplitude - 1.0) * cosine - beta;
        }
    }

    b0 = rawB0 / rawA0;
    b1 = rawB1 / rawA0;
    b2 = rawB2 / rawA0;
    a1 = rawA1 / rawA0;
    a2 = rawA2 / rawA0;
}

float ShapeCurveFilter::process(float input)
{
    const auto x = (double) input;
    const auto y = b0 * x + z1;
    z1 = b1 * x - a1 * y + z2;
    z2 = b2 * x - a2 * y;
    return (float) y;
}

ShapeCrankAudioProcessor::ShapeCrankAudioProcessor()
    : AudioProcessor(BusesProperties()
          .withInput("Input", juce::AudioChannelSet::stereo(), true)
          .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "Parameters", createParameterLayout())
{
    // Cache the atomics once: no identifier lookup or string construction in the audio callback.
    shapeParameter = parameters.getRawParameterValue(ParamIDs::shape);
    driveParameter = parameters.getRawParameterValue(ParamIDs::drive);
    levelParameter = parameters.getRawParameterValue(ParamIDs::level);
    bypassParameter = parameters.getRawParameterValue(ParamIDs::bypass);
    bassParameter = parameters.getRawParameterValue(ParamIDs::bass);
    trebleParameter = parameters.getRawParameterValue(ParamIDs::treble);
}

void ShapeCrankAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    maximumBlockSize = std::max(1, samplesPerBlock);
    prepareShapeCurve(currentSampleRate);

    shapeValue.reset(currentSampleRate, 0.04);
    shapeValue.setCurrentAndTargetValue(readParameter(shapeParameter));
    driveValue.reset(currentSampleRate, 0.035);
    driveValue.setCurrentAndTargetValue(readParameter(driveParameter));
    levelValue.reset(currentSampleRate, 0.025);
    levelValue.setCurrentAndTargetValue(readParameter(levelParameter));
    bassValue.reset(currentSampleRate, 0.04);
    bassValue.setCurrentAndTargetValue(readParameter(bassParameter));
    trebleValue.reset(currentSampleRate, 0.04);
    trebleValue.setCurrentAndTargetValue(readParameter(trebleParameter));
    effectMix.reset(currentSampleRate, 0.01);
    effectMix.setCurrentAndTargetValue(readParameter(bypassParameter) > 0.5f ? 0.0f : 1.0f);

    for (auto& channel : toneTrims)
    {
        channel[0].prepare(currentSampleRate, 160.0);
        channel[1].prepare(currentSampleRate, 3200.0);
    }
    lastBassDb = 100.0f;
    lastTrebleDb = 100.0f;
    trimUpdateCountdown = 0;

    // FIR half-band stages keep the wet/dry paths phase-aligned. The transparent
    // dry and bypass paths receive only this reported, fixed integer delay.
    const auto channels = (size_t) std::clamp(getTotalNumInputChannels(), 1, 2);
    oversampling = std::make_unique<juce::dsp::Oversampling<float>>(
        channels, overdriveOversamplingStages,
        juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
    oversampling->initProcessing((size_t) maximumBlockSize);
    oversampling->reset();
    const auto latency = (int) std::lround(oversampling->getLatencyInSamples());
    setLatencySamples(latency);

    const juce::dsp::ProcessSpec spec { currentSampleRate, (juce::uint32) maximumBlockSize, 2 };
    for (auto* delay : { &shapedDryDelay, &bypassDelay })
    {
        delay->setMaximumDelayInSamples(latency);
        delay->prepare(spec);
        delay->setDelay((float) latency);
    }
    shapedDryBuffer.setSize(2, maximumBlockSize);
    bypassBuffer.setSize(2, maximumBlockSize);
    odControls.setSize(7, maximumBlockSize);

    odHighPassState.fill(0.0);
    odInterstageState.fill(0.0);
    odLowPassState = {};
    odDCState.fill(0.0);
}

void ShapeCrankAudioProcessor::releaseResources()
{
    for (auto& channelFilters : shapeCurve)
        for (auto& filter : channelFilters)
            filter.reset();

    for (auto& channelFilters : toneTrims)
        for (auto& filter : channelFilters)
            filter.reset();

    if (oversampling != nullptr)
        oversampling->reset();
    shapedDryDelay.reset();
    bypassDelay.reset();
    odHighPassState.fill(0.0);
    odInterstageState.fill(0.0);
    odLowPassState = {};
    odDCState.fill(0.0);
}

bool ShapeCrankAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto mainInput = layouts.getMainInputChannelSet();
    const auto mainOutput = layouts.getMainOutputChannelSet();

    if (mainInput != mainOutput)
        return false;

    return mainOutput == juce::AudioChannelSet::mono()
        || mainOutput == juce::AudioChannelSet::stereo();
}

void ShapeCrankAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    processAudio(buffer, false);
}

void ShapeCrankAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ignoreUnused(midiMessages);
    processAudio(buffer, true);
}

void ShapeCrankAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer, bool hostBypassed)
{
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    shapeValue.setTargetValue(readParameter(shapeParameter));
    driveValue.setTargetValue(readParameter(driveParameter));
    levelValue.setTargetValue(readParameter(levelParameter));
    bassValue.setTargetValue(readParameter(bassParameter));
    trebleValue.setTargetValue(readParameter(trebleParameter));
    effectMix.setTargetValue(hostBypassed || readParameter(bypassParameter) > 0.5f ? 0.0f : 1.0f);

    const auto channels = std::min({ totalInputChannels, buffer.getNumChannels(), 2 });
    if (channels <= 0 || buffer.getNumSamples() <= 0 || maximumBlockSize <= 0 || oversampling == nullptr)
        return;

    // Hosts may send a larger block than promised. Non-owning views and bounded
    // chunks keep every scratch buffer allocation in prepareToPlay().
    for (int offset = 0; offset < buffer.getNumSamples(); offset += maximumBlockSize)
    {
        const auto samples = std::min(maximumBlockSize, buffer.getNumSamples() - offset);
        std::array<float*, 2> data {};
        for (int channel = 0; channel < channels; ++channel)
        {
            data[(size_t) channel] = buffer.getWritePointer(channel, offset);
            for (int sample = 0; sample < samples; ++sample)
            {
                bypassDelay.pushSample(channel, data[(size_t) channel][sample]);
                bypassBuffer.setSample(channel, sample, bypassDelay.popSample(channel));
            }
        }
        juce::AudioBuffer<float> block(data.data(), channels, samples);

        processShapeCurve(block);
        processToneTrims(block);
        processOverdrive(block);
        processOutputLevel(block);

        // Keep all paths warm even while bypassed, then crossfade aligned audio.
        for (int sample = 0; sample < samples; ++sample)
        {
            const auto mix = effectMix.getNextValue();
            for (int channel = 0; channel < channels; ++channel)
            {
                const auto dry = bypassBuffer.getSample(channel, sample);
                const auto wet = block.getSample(channel, sample);
                block.setSample(channel, sample, mix >= 1.0f ? wet
                                               : mix <= 0.0f ? dry : dry + mix * (wet - dry));
            }
        }
    }
}

void ShapeCrankAudioProcessor::prepareShapeCurve(double sampleRate)
{
    for (size_t channel = 0; channel < shapeCurve.size(); ++channel)
        for (int filter = 0; filter < shapeFilterCount; ++filter)
            shapeCurve[channel][(size_t) filter].prepare(sampleRate, shapeFrequencies[(size_t) filter]);
}

float ShapeCrankAudioProcessor::responseCurve(float knob)
{
    const auto clamped = std::clamp(knob, 0.0f, 1.0f);
    const auto smooth = clamped * clamped * (3.0f - 2.0f * clamped);
    constexpr auto currentFullStrengthAtKnob = 0.75f;
    const auto referenceSmooth = currentFullStrengthAtKnob * currentFullStrengthAtKnob * (3.0f - 2.0f * currentFullStrengthAtKnob);
    const auto referenceAmount = std::pow(referenceSmooth, 1.35f);
    return std::pow(smooth, 1.35f) / referenceAmount;
}

float ShapeCrankAudioProcessor::levelGainDb(float knob)
{
    const auto clamped = std::clamp(knob, 0.0f, 1.0f);

    if (clamped <= 0.5f)
        return juce::jmap(clamped, 0.0f, 0.5f, -18.0f, 0.0f);

    return juce::jmap(clamped, 0.5f, 1.0f, 0.0f, 12.0f);
}

void ShapeCrankAudioProcessor::processShapeCurve(juce::AudioBuffer<float>& buffer)
{
    const auto channelCount = std::min(buffer.getNumChannels(), (int) shapeCurve.size());
    const auto sampleCount = buffer.getNumSamples();

    if (sampleCount <= 0)
        return;

    const auto startKnob = shapeValue.getCurrentValue();
    const auto endKnob = shapeValue.skip(sampleCount);
    const auto amount = responseCurve((startKnob + endKnob) * 0.5f);

    // Broad subtractive front-end curve: trim flub, firm low mids, restrain edge, and soften fizz.
    const std::array<float, shapeFilterCount> gains {
        amount,
        -2.1f * amount,
        -1.25f * amount,
        -2.6f * amount
    };
    const std::array<double, shapeFilterCount> shapes { 0.65, 0.60, 0.55, 0.60 };
    const std::array<ShapeCurveFilter::FilterType, shapeFilterCount> types {
        ShapeCurveFilter::FilterType::highPass,
        ShapeCurveFilter::FilterType::peak,
        ShapeCurveFilter::FilterType::peak,
        ShapeCurveFilter::FilterType::highShelf
    };

    for (int filter = 0; filter < shapeFilterCount; ++filter)
        for (int channel = 0; channel < channelCount; ++channel)
            shapeCurve[(size_t) channel][(size_t) filter].setCoefficients(
                gains[(size_t) filter], shapes[(size_t) filter], types[(size_t) filter]);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        for (int channel = 0; channel < channelCount; ++channel)
        {
            auto value = buffer.getSample(channel, sample);

            for (int filter = 0; filter < shapeFilterCount; ++filter)
                value = shapeCurve[(size_t) channel][(size_t) filter].process(value);

            buffer.setSample(channel, sample, value);
        }
    }
}

void ShapeCrankAudioProcessor::processToneTrims(juce::AudioBuffer<float>& buffer)
{
    const auto channels = std::min(buffer.getNumChannels(), (int) toneTrims.size());
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto bass = bassValue.getNextValue();
        const auto treble = trebleValue.getNextValue();

        // A 16-sample control interval avoids per-sample trigonometry. The pot
        // values themselves ramp over 40 ms, with no callback-sized gain steps.
        if (trimUpdateCountdown == 0)
        {
            if (! juce::exactlyEqual(bass, lastBassDb))
            {
                for (auto& channel : toneTrims)
                    channel[0].setCoefficients(bass, 0.65, ShapeCurveFilter::FilterType::lowShelf);
                lastBassDb = bass;
            }
            if (! juce::exactlyEqual(treble, lastTrebleDb))
            {
                for (auto& channel : toneTrims)
                    channel[1].setCoefficients(treble, 0.65, ShapeCurveFilter::FilterType::highShelf);
                lastTrebleDb = treble;
            }
            trimUpdateCountdown = 16;
        }
        --trimUpdateCountdown;

        for (int channel = 0; channel < channels; ++channel)
        {
            auto value = toneTrims[(size_t) channel][0].process(buffer.getSample(channel, sample));
            value = toneTrims[(size_t) channel][1].process(value);
            buffer.setSample(channel, sample, value);
        }
    }
}

void ShapeCrankAudioProcessor::processOverdrive(juce::AudioBuffer<float>& buffer)
{
    const auto channels = std::min(buffer.getNumChannels(), (int) odHighPassState.size());
    const auto samples = buffer.getNumSamples();
    if (samples <= 0 || oversampling == nullptr)
        return;

    enum Control { inputGain, outputGain, secondStageGain, secondStageScale, bias, biasReference, wetMix };
    const auto startDrive = driveValue.getCurrentValue();
    for (int sample = 0; sample < samples; ++sample)
    {
        const auto drive = std::clamp(driveValue.getNextValue(), 0.0f, 1.0f);
        const auto blend = std::clamp(drive / 0.15f, 0.0f, 1.0f);
        const auto offset = 0.025 + 0.10 * (double) drive;
        // A continuous dB taper gives every part of the dial useful gain. The
        // short initial blend preserves an exact clean endpoint at zero.
        // Split the gain across two rounded stages: a single 42dB first stage
        // creates needlessly sharp edges even before the downsampling filters.
        odControls.setSample(inputGain, sample, juce::Decibels::decibelsToGain(6.0f + 24.0f * drive));
        const auto makeupDb = -6.0f - 8.0f * (1.0f - std::exp(-3.0f * drive));
        odControls.setSample(outputGain, sample, juce::Decibels::decibelsToGain(makeupDb));
        const auto stageScale = 1.0f + 0.75f * drive;
        odControls.setSample(secondStageGain, sample, stageScale * juce::Decibels::decibelsToGain(12.0f * drive));
        odControls.setSample(secondStageScale, sample, 1.0f / stageScale);
        odControls.setSample(bias, sample, offset);
        odControls.setSample(biasReference, sample, std::tanh(offset));
        odControls.setSample(wetMix, sample, blend * blend * (3.0f - 2.0f * blend));

        for (int channel = 0; channel < channels; ++channel)
        {
            shapedDryDelay.pushSample(channel, buffer.getSample(channel, sample));
            shapedDryBuffer.setSample(channel, sample, shapedDryDelay.popSample(channel));
        }
    }

    const auto drive = 0.5 * (double) (startDrive + driveValue.getCurrentValue());
    const auto oversamplingFactor = oversampling->getOversamplingFactor();
    const auto oversampledRate = currentSampleRate * (double) oversamplingFactor;
    const auto coefficient = [oversampledRate](double hz)
    {
        return 1.0 - std::exp(-2.0 * pi * std::min(hz, oversampledRate * 0.40) / oversampledRate);
    };
    const auto highPassCoefficient = coefficient(45.0 + 35.0 * drive);
    const auto interstageCoefficient = coefficient(16000.0 - 4500.0 * drive);
    const auto lowPassCoefficient = coefficient(12000.0 - 4500.0 * drive);
    const auto dcCoefficient = coefficient(12.0);

    juce::dsp::AudioBlock<float> block(buffer);
    auto upsampled = oversampling->processSamplesUp(block);
    for (int channel = 0; channel < channels; ++channel)
    {
        auto* audio = upsampled.getChannelPointer((size_t) channel);
        auto& highPass = odHighPassState[(size_t) channel];
        auto& interstage = odInterstageState[(size_t) channel];
        auto& lowPass = odLowPassState[(size_t) channel];
        auto& dc = odDCState[(size_t) channel];
        for (size_t sample = 0; sample < upsampled.getNumSamples(); ++sample)
        {
            const auto controlSample = (int) (sample / oversamplingFactor);
            const auto input = (double) audio[sample];
            highPass += highPassCoefficient * (input - highPass);
            const auto reference = (double) odControls.getSample(biasReference, controlSample);
            const auto gain = (double) odControls.getSample(inputGain, controlSample);
            const auto offset = (double) odControls.getSample(bias, controlSample);
            // Offset subtraction makes silence exactly silent. The small bias
            // adds even harmonics without an artificial constant DC output.
            auto value = (std::tanh((input - highPass) * gain + offset) - reference)
                       / (1.0 - reference * reference);
            interstage += interstageCoefficient * (value - interstage);
            const auto stageGain = (double) odControls.getSample(secondStageGain, controlSample);
            value = std::tanh(interstage * stageGain) * odControls.getSample(secondStageScale, controlSample);
            dc += dcCoefficient * (value - dc);
            value -= dc;
            lowPass[0] += lowPassCoefficient * (value - lowPass[0]);
            lowPass[1] += lowPassCoefficient * (lowPass[0] - lowPass[1]);
            audio[sample] = (float) (lowPass[1] * odControls.getSample(outputGain, controlSample));
        }
    }
    oversampling->processSamplesDown(block);

    for (int channel = 0; channel < channels; ++channel)
    {
        auto* audio = buffer.getWritePointer(channel);
        const auto* dry = shapedDryBuffer.getReadPointer(channel);
        const auto* mix = odControls.getReadPointer(wetMix);
        for (int sample = 0; sample < samples; ++sample)
            audio[sample] = mix[sample] >= 1.0 ? audio[sample]
                          : mix[sample] <= 0.0 ? dry[sample]
                          : (float) (dry[sample] + mix[sample] * (audio[sample] - dry[sample]));
    }
}

void ShapeCrankAudioProcessor::processOutputLevel(juce::AudioBuffer<float>& buffer)
{
    // Keep the original transfer function and final clean gain, but apply its
    // existing smoothing per sample so automation cannot step at block edges.
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        const auto gain = juce::Decibels::decibelsToGain(levelGainDb(levelValue.getNextValue()));
        for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
            buffer.setSample(channel, sample, buffer.getSample(channel, sample) * gain);
    }
}

juce::AudioProcessorEditor* ShapeCrankAudioProcessor::createEditor()
{
    return new ShapeCrankAudioProcessorEditor(*this);
}

void ShapeCrankAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String ShapeCrankAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void ShapeCrankAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

void ShapeCrankAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto state = parameters.copyState().createXml())
        copyXmlToBinary(*state, destData);
}

void ShapeCrankAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto state = getXmlFromBinary(data, sizeInBytes))
    {
        if (! state->hasTagName(parameters.state.getType()))
            return;

        auto restored = juce::ValueTree::fromXml(*state);
        // APVTS retains a missing parameter's current value. Explicit neutral
        // nodes make a v0.1 session load correctly even after using the trims.
        for (const auto* trim : { ParamIDs::bass, ParamIDs::treble })
        {
            if (! restored.getChildWithProperty("id", trim).isValid())
            {
                juce::ValueTree neutral("PARAM");
                neutral.setProperty("id", trim, nullptr);
                neutral.setProperty("value", 0.0f, nullptr);
                restored.appendChild(neutral, nullptr);
            }
        }
        parameters.replaceState(restored);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout ShapeCrankAudioProcessor::createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::shape, "Shape", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::drive, "OD", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.0f));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        ParamIDs::level, "Level", juce::NormalisableRange<float>(0.0f, 1.0f, 0.001f), 0.5f));
    params.push_back(std::make_unique<juce::AudioParameterBool>(ParamIDs::bypass, "Bypass", false));

    // Append only: preserve the original parameter indices and AU ordering.
    const auto trimAttributes = juce::AudioParameterFloatAttributes().withLabel("dB");
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::bass, 1 }, "Bass trim",
        juce::NormalisableRange<float>(-6.0f, 6.0f, 0.1f), 0.0f, trimAttributes));
    params.push_back(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID { ParamIDs::treble, 1 }, "Treble trim",
        juce::NormalisableRange<float>(-6.0f, 6.0f, 0.1f), 0.0f, trimAttributes));

    return { params.begin(), params.end() };
}
} // namespace shapecrank

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new shapecrank::ShapeCrankAudioProcessor();
}
