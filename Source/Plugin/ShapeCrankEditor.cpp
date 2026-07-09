#include "ShapeCrankEditor.h"
#include "BinaryData.h"

namespace shapecrank
{
namespace
{
constexpr auto sourceArtworkWidth = 1086.0f;
constexpr auto sourceArtworkHeight = 1448.0f;
constexpr auto knobZeroAngle = 270.0f;

juce::Colour ink() { return juce::Colour(0xff153f49); }
juce::Colour warmInk() { return juce::Colour(0xff49382b); }

const juce::Image& pedalBodyImage()
{
    static const auto image = juce::ImageCache::getFromMemory(BinaryData::pedal_body_clean_png, BinaryData::pedal_body_clean_pngSize);
    return image;
}

const juce::Image& knobImage()
{
    static const auto image = juce::ImageCache::getFromMemory(BinaryData::knob_png, BinaryData::knob_pngSize);
    return image;
}

const juce::Image& footswitchImage()
{
    static const auto image = juce::ImageCache::getFromMemory(BinaryData::footswitch_png, BinaryData::footswitch_pngSize);
    return image;
}

const juce::Image& ledOnImage()
{
    static const auto image = juce::ImageCache::getFromMemory(BinaryData::led_on_png, BinaryData::led_on_pngSize);
    return image;
}

const juce::Image& ledOffImage()
{
    static const auto image = juce::ImageCache::getFromMemory(BinaryData::led_off_png, BinaryData::led_off_pngSize);
    return image;
}

const juce::Image& textureOverlayImage()
{
    static const auto image = juce::ImageCache::getFromMemory(BinaryData::texture_overlay_clean_png, BinaryData::texture_overlay_clean_pngSize);
    return image;
}

juce::Rectangle<float> getPedalBounds(juce::Rectangle<int> localBounds)
{
    const auto bounds = localBounds.toFloat();
    const auto sourceAspect = sourceArtworkWidth / sourceArtworkHeight;

    auto width = bounds.getWidth();
    auto height = width / sourceAspect;

    if (height > bounds.getHeight())
    {
        height = bounds.getHeight();
        width = height * sourceAspect;
    }

    return juce::Rectangle<float>(width, height).withCentre(bounds.getCentre());
}

juce::Rectangle<float> mapArtworkRect(juce::Rectangle<float> pedalBounds, juce::Rectangle<float> artworkRect)
{
    const auto scaleX = pedalBounds.getWidth() / sourceArtworkWidth;
    const auto scaleY = pedalBounds.getHeight() / sourceArtworkHeight;

    return juce::Rectangle<float>(pedalBounds.getX() + artworkRect.getX() * scaleX,
                                  pedalBounds.getY() + artworkRect.getY() * scaleY,
                                  artworkRect.getWidth() * scaleX,
                                  artworkRect.getHeight() * scaleY);
}

juce::Rectangle<float> mapArtworkSquare(juce::Rectangle<float> pedalBounds, juce::Point<float> centre, float size)
{
    return mapArtworkRect(pedalBounds, { centre.x - size * 0.5f, centre.y - size * 0.5f, size, size });
}

void drawImage(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds)
{
    if (image.isValid())
        g.drawImage(image, bounds, juce::RectanglePlacement::stretchToFit, false);
}

void drawRotatedImage(juce::Graphics& g, const juce::Image& image, juce::Rectangle<float> bounds, float angleRadians)
{
    if (! image.isValid())
        return;

    juce::Graphics::ScopedSaveState saveState(g);
    g.addTransform(juce::AffineTransform::rotation(angleRadians - juce::degreesToRadians(knobZeroAngle),
                                                   bounds.getCentreX(),
                                                   bounds.getCentreY()));
    drawImage(g, image, bounds);
}

void drawCentredText(juce::Graphics& g, juce::Rectangle<float> bounds, const juce::String& text, float height, juce::Colour colour, bool bold = false)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain));
    g.drawFittedText(text, bounds.getSmallestIntegerContainer(), juce::Justification::centred, 1);
}

class PedalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override
    {
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        drawRotatedImage(g, knobImage(), bounds, angle);
    }

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override
    {
    }
};
} // namespace

ShapeCrankAudioProcessorEditor::ShapeCrankAudioProcessorEditor(ShapeCrankAudioProcessor& owner)
    : AudioProcessorEditor(&owner), audioProcessor(owner), pedalLookAndFeel(std::make_unique<PedalLookAndFeel>())
{
    setSize(430, 590);

    configureKnob(shapeSlider);
    addAndMakeVisible(shapeSlider);
    configureKnob(driveSlider);
    addAndMakeVisible(driveSlider);
    configureKnob(levelSlider);
    addAndMakeVisible(levelSlider);

    bypassButton.setButtonText("");
    bypassButton.setLookAndFeel(pedalLookAndFeel.get());
    bypassButton.setClickingTogglesState(true);
    bypassButton.onClick = [this] { repaint(); };
    addAndMakeVisible(bypassButton);

    shapeAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::shape, shapeSlider);
    driveAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::drive, driveSlider);
    levelAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::level, levelSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, ParamIDs::bypass, bypassButton);
}

ShapeCrankAudioProcessorEditor::~ShapeCrankAudioProcessorEditor()
{
    bypassButton.setLookAndFeel(nullptr);
    shapeSlider.setLookAndFeel(nullptr);
    driveSlider.setLookAndFeel(nullptr);
    levelSlider.setLookAndFeel(nullptr);
}

void ShapeCrankAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xfff7f6f1));

    const auto pedalBounds = getPedalBounds(getLocalBounds());
    drawImage(graphics, pedalBodyImage(), pedalBounds);

    {
        juce::Graphics::ScopedSaveState textureState(graphics);
        graphics.setOpacity(0.22f);
        drawImage(graphics, textureOverlayImage(), pedalBounds);
    }

    drawCentredText(graphics, mapArtworkRect(pedalBounds, { 185.0f, 116.0f, 716.0f, 76.0f }), "Shape Crank", 40.0f, ink(), true);
    drawCentredText(graphics, mapArtworkRect(pedalBounds, { 248.0f, 180.0f, 590.0f, 42.0f }), "Shape + OD", 19.0f, warmInk());
    drawCentredText(graphics, mapArtworkRect(pedalBounds, { 110.0f, 430.0f, 250.0f, 45.0f }), "Shape", 23.0f, ink(), true);
    drawCentredText(graphics, mapArtworkRect(pedalBounds, { 418.0f, 430.0f, 250.0f, 45.0f }), "OD", 23.0f, ink(), true);
    drawCentredText(graphics, mapArtworkRect(pedalBounds, { 726.0f, 430.0f, 250.0f, 45.0f }), "Level", 23.0f, ink(), true);

    drawCentredText(graphics, mapArtworkRect(pedalBounds, { 372.0f, 810.0f, 342.0f, 52.0f }), "DevMuse", 23.0f, ink(), true);

    const auto ledBounds = mapArtworkSquare(pedalBounds, { 543.0f, 930.0f }, 112.0f);
    drawImage(graphics, isPedalOn() ? ledOnImage() : ledOffImage(), ledBounds);

    const auto footswitchBounds = mapArtworkSquare(pedalBounds, { 543.0f, 1118.0f }, 305.0f);
    drawImage(graphics, footswitchImage(), footswitchBounds);
}

void ShapeCrankAudioProcessorEditor::resized()
{
    const auto pedalBounds = getPedalBounds(getLocalBounds());
    shapeSlider.setBounds(mapArtworkSquare(pedalBounds, { 235.0f, 314.0f }, 220.0f).getSmallestIntegerContainer());
    driveSlider.setBounds(mapArtworkSquare(pedalBounds, { 543.0f, 314.0f }, 220.0f).getSmallestIntegerContainer());
    levelSlider.setBounds(mapArtworkSquare(pedalBounds, { 851.0f, 314.0f }, 220.0f).getSmallestIntegerContainer());
    bypassButton.setBounds(mapArtworkSquare(pedalBounds, { 543.0f, 1118.0f }, 335.0f).getSmallestIntegerContainer());
}

void ShapeCrankAudioProcessorEditor::configureKnob(juce::Slider& slider)
{
    slider.setLookAndFeel(pedalLookAndFeel.get());
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(juce::degreesToRadians(135.0f), juce::degreesToRadians(405.0f), true);
}

bool ShapeCrankAudioProcessorEditor::isPedalOn() const
{
    if (const auto* bypass = audioProcessor.parameters.getRawParameterValue(ParamIDs::bypass))
        return bypass->load() <= 0.5f;

    return true;
}
} // namespace shapecrank
