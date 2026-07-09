#pragma once

#include <JuceHeader.h>

#include <array>

namespace shapecrank
{
namespace ParamIDs
{
inline constexpr const char* shape = "shape";
inline constexpr const char* drive = "drive";
inline constexpr const char* level = "level";
inline constexpr const char* bypass = "bypass";
} // namespace ParamIDs

inline constexpr int shapeFilterCount = 4;

class ShapeCurveFilter
{
public:
    enum class FilterType
    {
        peak,
        highPass,
        lowShelf,
        highShelf
    };

    void prepare(double newSampleRate, double newFrequency);
    void reset();
    void setCoefficients(float gainDb, double qOrSlope, FilterType filterType);
    float process(float input);

private:
    double sampleRate = 48000.0;
    double frequency = 1000.0;
    double b0 = 1.0;
    double b1 = 0.0;
    double b2 = 0.0;
    double a1 = 0.0;
    double a2 = 0.0;
    double z1 = 0.0;
    double z2 = 0.0;
};

class ShapeCrankAudioProcessor : public juce::AudioProcessor
{
public:
    ShapeCrankAudioProcessor();
    ~ShapeCrankAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState parameters;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    static float responseCurve(float knob);
    static float softClip(float input, float driveAmount);
    static float levelGainDb(float knob);

    void prepareShapeCurve(double sampleRate);
    void processShapeCurve(juce::AudioBuffer<float>& buffer);
    void processOverdrive(juce::AudioBuffer<float>& buffer);
    void processOutputLevel(juce::AudioBuffer<float>& buffer);

    juce::SmoothedValue<float> shapeValue;
    juce::SmoothedValue<float> driveValue;
    juce::SmoothedValue<float> levelValue;
    std::array<std::array<ShapeCurveFilter, shapeFilterCount>, 2> shapeCurve;
    std::array<double, 2> odHighPassState {};
    std::array<double, 2> odLowPassState {};
    double currentSampleRate = 48000.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShapeCrankAudioProcessor)
};
} // namespace shapecrank
