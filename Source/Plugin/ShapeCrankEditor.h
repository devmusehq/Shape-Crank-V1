#pragma once

#include <JuceHeader.h>
#include "ShapeCrankProcessor.h"

namespace shapecrank
{
class ShapeCrankAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       private juce::Timer
{
public:
    explicit ShapeCrankAudioProcessorEditor(ShapeCrankAudioProcessor& processor);
    ~ShapeCrankAudioProcessorEditor() override;

    void paint(juce::Graphics& graphics) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void configureKnob(juce::Slider& slider, const juce::String& name,
                       const juce::String& tooltip, double defaultValue, bool isTrim = false);
    void timerCallback() override;
    bool isPedalOn() const;

    ShapeCrankAudioProcessor& audioProcessor;
    std::unique_ptr<juce::LookAndFeel> pedalLookAndFeel;

    juce::Slider shapeSlider;
    juce::Slider driveSlider;
    juce::Slider levelSlider;
    juce::Slider bassSlider;
    juce::Slider trebleSlider;
    juce::ToggleButton bypassButton;
    juce::TooltipWindow tooltips;
    bool lastPedalOn = true;

    std::unique_ptr<SliderAttachment> shapeAttachment;
    std::unique_ptr<SliderAttachment> driveAttachment;
    std::unique_ptr<SliderAttachment> levelAttachment;
    std::unique_ptr<SliderAttachment> bassAttachment;
    std::unique_ptr<SliderAttachment> trebleAttachment;
    std::unique_ptr<ButtonAttachment> bypassAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ShapeCrankAudioProcessorEditor)
};
} // namespace shapecrank
