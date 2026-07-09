#include "ShapeCrankProcessor.h"
#include "ShapeCrankEditor.h"

namespace shapecrank
{
namespace
{
constexpr std::array<double, shapeFilterCount> shapeFrequencies { 135.0, 240.0, 2900.0, 7000.0 };
constexpr auto pi = 3.14159265358979323846;

float readParameter(juce::AudioProcessorValueTreeState& parameters, const char* parameterID)
{
    if (const auto* value = parameters.getRawParameterValue(parameterID))
        return value->load();

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
}

void ShapeCrankAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused(samplesPerBlock);

    currentSampleRate = sampleRate > 0.0 ? sampleRate : 48000.0;
    prepareShapeCurve(currentSampleRate);

    shapeValue.reset(currentSampleRate, 0.04);
    shapeValue.setCurrentAndTargetValue(readParameter(parameters, ParamIDs::shape));
    driveValue.reset(currentSampleRate, 0.035);
    driveValue.setCurrentAndTargetValue(readParameter(parameters, ParamIDs::drive));
    levelValue.reset(currentSampleRate, 0.025);
    levelValue.setCurrentAndTargetValue(readParameter(parameters, ParamIDs::level));

    odHighPassState.fill(0.0);
    odLowPassState.fill(0.0);
}

void ShapeCrankAudioProcessor::releaseResources()
{
    for (auto& channelFilters : shapeCurve)
        for (auto& filter : channelFilters)
            filter.reset();

    odHighPassState.fill(0.0);
    odLowPassState.fill(0.0);
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
    juce::ScopedNoDenormals noDenormals;

    const auto totalInputChannels = getTotalNumInputChannels();
    const auto totalOutputChannels = getTotalNumOutputChannels();
    for (auto channel = totalInputChannels; channel < totalOutputChannels; ++channel)
        buffer.clear(channel, 0, buffer.getNumSamples());

    shapeValue.setTargetValue(readParameter(parameters, ParamIDs::shape));
    driveValue.setTargetValue(readParameter(parameters, ParamIDs::drive));
    levelValue.setTargetValue(readParameter(parameters, ParamIDs::level));

    const auto bypassed = readParameter(parameters, ParamIDs::bypass) > 0.5f;
    if (bypassed || totalOutputChannels == 0)
        return;

    processShapeCurve(buffer);
    processOverdrive(buffer);
    processOutputLevel(buffer);
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

float ShapeCrankAudioProcessor::softClip(float input, float driveAmount)
{
    const auto asymmetry = 0.055f * driveAmount;
    const auto softInput = input * (0.9f + 0.1f * driveAmount) + asymmetry;
    const auto tanhPart = std::tanh(softInput);
    const auto atanPart = (float) ((2.0 / pi) * std::atan(1.45 * (double) softInput));
    const auto referenceTanh = std::tanh(asymmetry);
    const auto referenceAtan = (float) ((2.0 / pi) * std::atan(1.45 * (double) asymmetry));
    const auto clipped = 0.62f * (tanhPart - referenceTanh) + 0.38f * (atanPart - referenceAtan);

    return clipped * (0.98f + 0.02f * driveAmount);
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

void ShapeCrankAudioProcessor::processOverdrive(juce::AudioBuffer<float>& buffer)
{
    const auto channelCount = std::min(buffer.getNumChannels(), (int) odHighPassState.size());
    const auto sampleCount = buffer.getNumSamples();

    if (sampleCount <= 0)
        return;

    const auto startDrive = driveValue.getCurrentValue();
    const auto endDrive = driveValue.skip(sampleCount);
    const auto driveKnob = std::clamp((startDrive + endDrive) * 0.5f, 0.0f, 1.0f);

    if (driveKnob < 0.0001f)
        return;

    const auto driveShape = driveKnob * driveKnob * (3.0f - 2.0f * driveKnob);
    const auto wet = std::clamp(0.08f * driveKnob + 0.92f * driveShape, 0.0f, 1.0f);
    const auto inputGainDb = 2.0f + 22.0f * driveShape + 10.0f * driveShape * driveShape;
    const auto inputGain = juce::Decibels::decibelsToGain(inputGainDb);
    const auto clipMakeup = juce::Decibels::decibelsToGain(-0.24f * inputGainDb);

    const auto highPassCutoff = 62.0 + 45.0 * driveShape;
    const auto lowPassCutoff = 11200.0 - 2800.0 * driveShape;
    const auto highPassCoefficient = 1.0 - std::exp(-2.0 * pi * highPassCutoff / currentSampleRate);
    const auto lowPassCoefficient = 1.0 - std::exp(-2.0 * pi * lowPassCutoff / currentSampleRate);

    for (int sample = 0; sample < sampleCount; ++sample)
    {
        for (int channel = 0; channel < channelCount; ++channel)
        {
            const auto dry = buffer.getSample(channel, sample);

            odHighPassState[(size_t) channel] += highPassCoefficient * ((double) dry - odHighPassState[(size_t) channel]);
            const auto tightened = (float) ((double) dry - odHighPassState[(size_t) channel]);

            auto driven = softClip(tightened * inputGain, driveShape) * clipMakeup;

            odLowPassState[(size_t) channel] += lowPassCoefficient * ((double) driven - odLowPassState[(size_t) channel]);
            driven = (float) odLowPassState[(size_t) channel];

            buffer.setSample(channel, sample, dry + wet * (driven - dry));
        }
    }
}

void ShapeCrankAudioProcessor::processOutputLevel(juce::AudioBuffer<float>& buffer)
{
    const auto channelCount = buffer.getNumChannels();
    const auto sampleCount = buffer.getNumSamples();

    if (sampleCount <= 0 || channelCount <= 0)
        return;

    const auto startLevel = levelValue.getCurrentValue();
    const auto endLevel = levelValue.skip(sampleCount);
    const auto gain = juce::Decibels::decibelsToGain(levelGainDb((startLevel + endLevel) * 0.5f));

    buffer.applyGain(gain);
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
        if (state->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*state));
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

    return { params.begin(), params.end() };
}
} // namespace shapecrank

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new shapecrank::ShapeCrankAudioProcessor();
}
