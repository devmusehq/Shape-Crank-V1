#include "ShapeCrankEditor.h"

namespace shapecrank
{
namespace
{
constexpr float designWidth = 480.0f;
constexpr float designHeight = 744.0f;
constexpr float startAngle = juce::MathConstants<float>::pi * 1.25f;
constexpr float endAngle = juce::MathConstants<float>::pi * 2.75f;

juce::Colour cream() { return juce::Colour(0xfff3e8cf); }
juce::Colour muted() { return juce::Colour(0xffa8beb5); }
juce::Colour amber() { return juce::Colour(0xffefaf50); }
juce::Colour petrol() { return juce::Colour(0xff184c4c); }

juce::Point<float> onCircle(juce::Point<float> centre, float radius, float angle)
{
    return centre + juce::Point<float>(std::sin(angle) * radius, -std::cos(angle) * radius);
}

void text(juce::Graphics& g, const juce::String& string, juce::Rectangle<float> bounds,
          float height, juce::Colour colour, bool bold = false,
          juce::Justification justification = juce::Justification::centred, float tracking = 0.05f)
{
    g.setColour(colour);
    g.setFont(juce::FontOptions(height, bold ? juce::Font::bold : juce::Font::plain).withKerningFactor(tracking));
    g.drawFittedText(string, bounds.toNearestInt(), justification, 1);
}

juce::String decibels(double value)
{
    if (std::abs(value) < 0.05)
        return "0.0 dB";
    return (value > 0.0 ? "+" : "") + juce::String(value, 1) + " dB";
}

double levelDb(double value)
{
    return value <= 0.5 ? -18.0 + value * 36.0 : (value - 0.5) * 24.0;
}

void screw(juce::Graphics& g, float x, float y, float rotation)
{
    const juce::Rectangle<float> bounds(x - 5.0f, y - 5.0f, 10.0f, 10.0f);
    g.setColour(juce::Colours::black.withAlpha(0.45f));
    g.fillEllipse(bounds.expanded(1.5f).translated(0.0f, 1.0f));
    g.setGradientFill(juce::ColourGradient(juce::Colour(0xff8b9690), x - 3.0f, y - 4.0f,
                                          juce::Colour(0xff303f3e), x + 3.0f, y + 4.0f, false));
    g.fillEllipse(bounds);
    g.setColour(juce::Colour(0xff152424));
    g.drawLine({ onCircle({ x, y }, 3.1f, rotation),
                 onCircle({ x, y }, 3.1f, rotation + juce::MathConstants<float>::pi) }, 1.4f);
}

class PedalLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    PedalLookAndFeel()
    {
        setColour(juce::TooltipWindow::backgroundColourId, cream());
        setColour(juce::TooltipWindow::textColourId, juce::Colour(0xff183f3f));
        setColour(juce::TooltipWindow::outlineColourId, juce::Colour(0xff668782));
        setColour(juce::BubbleComponent::backgroundColourId, cream());
        setColour(juce::BubbleComponent::outlineColourId, petrol());
    }

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<float>((float) x, (float) y, (float) width, (float) height);
        const auto centre = bounds.getCentre();
        const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const auto isTrim = (bool) slider.getProperties()["trim"];
        const auto focused = slider.hasKeyboardFocus(true);
        const auto hovered = slider.isMouseOverOrDragging();

        if (isTrim)
        {
            // Recessed slotted metal trimmers remain distinct from the main pots.
            g.setColour(juce::Colour(0xff081e20));
            g.fillEllipse(bounds.reduced(radius * 0.12f));
            g.setColour(juce::Colour(0xff5c7870));
            g.drawEllipse(bounds.reduced(radius * 0.12f), 1.0f);
            const auto cap = bounds.reduced(radius * 0.33f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xffc0c2ad), cap.getX(), cap.getY(),
                                                  juce::Colour(0xff5c706a), cap.getRight(), cap.getBottom(), false));
            g.fillEllipse(cap);
            g.setColour(juce::Colour(0xffd6d9bf).withAlpha(0.5f));
            g.drawEllipse(cap.reduced(1.0f), 0.7f);
            g.setColour(juce::Colour(0xff243d39));
            g.drawLine({ onCircle(centre, radius * 0.42f, angle),
                         onCircle(centre, radius * 0.42f, angle + juce::MathConstants<float>::pi) }, radius * 0.13f);
            g.setColour(amber());
            g.fillEllipse(juce::Rectangle<float>(radius * 0.14f, radius * 0.14f)
                              .withCentre(onCircle(centre, radius * 0.78f, angle)));
        }
        else
        {
            for (int tick = 0; tick <= 10; ++tick)
            {
                const auto tickAngle = rotaryStartAngle + (float) tick * (rotaryEndAngle - rotaryStartAngle) / 10.0f;
                const auto major = tick == 0 || tick == 5 || tick == 10;
                g.setColour(major ? cream().withAlpha(0.85f) : muted().withAlpha(0.45f));
                g.drawLine({ onCircle(centre, radius * (major ? 0.89f : 0.93f), tickAngle),
                             onCircle(centre, radius * 0.99f, tickAngle) }, major ? 1.5f : 1.0f);
            }
            const auto cap = bounds.reduced(radius * 0.20f);
            g.setColour(juce::Colours::black.withAlpha(0.30f));
            g.fillEllipse(cap.expanded(2.5f).translated(0.0f, 4.0f));
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff555a55), centre.x, cap.getY(),
                                                  juce::Colour(0xff080f10), centre.x, cap.getBottom(), false));
            g.fillEllipse(cap);
            for (int flute = 0; flute < 44; ++flute)
            {
                const auto fluteAngle = (float) flute * juce::MathConstants<float>::twoPi / 44.0f;
                g.setColour(juce::Colours::black.withAlpha(0.45f));
                g.drawLine({ onCircle(centre, radius * 0.70f, fluteAngle),
                             onCircle(centre, radius * 0.79f, fluteAngle) }, 1.5f);
            }
            const auto face = bounds.reduced(radius * 0.35f);
            g.setGradientFill(juce::ColourGradient(juce::Colour(0xff333d39), face.getX(), face.getY(),
                                                  juce::Colour(0xff111b1b), face.getRight(), face.getBottom(), false));
            g.fillEllipse(face);
            g.setColour(juce::Colour(0xff777c6b).withAlpha(0.38f));
            g.drawEllipse(face, 0.8f);
            g.setColour(cream());
            g.drawLine({ onCircle(centre, radius * 0.36f, angle),
                         onCircle(centre, radius * 0.67f, angle) }, radius * 0.058f);
            g.setColour(amber());
            g.fillEllipse(juce::Rectangle<float>(radius * 0.055f, radius * 0.055f)
                              .withCentre(onCircle(centre, radius * 0.81f, angle)));
        }
        if (focused || hovered)
        {
            g.setColour(amber().withAlpha(focused ? 0.95f : 0.45f));
            g.drawEllipse(bounds.reduced(isTrim ? 1.0f : radius * 0.16f), focused ? 1.8f : 1.0f);
        }
    }

    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button, bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced(7.0f);
        const auto centre = bounds.getCentre();
        const auto radius = bounds.getWidth() * 0.5f;
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillEllipse(bounds.expanded(2.0f).translated(0.0f, 4.0f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffa1aaa6), centre.x, bounds.getY(),
                                              juce::Colour(0xff263938), centre.x, bounds.getBottom(), false));
        g.fillEllipse(bounds);
        g.setColour(juce::Colour(0xff142625));
        g.drawEllipse(bounds.reduced(1.5f), 1.3f);
        juce::Path nut;
        for (int corner = 0; corner < 6; ++corner)
        {
            const auto p = onCircle(centre, radius * 0.81f,
                                    (float) corner * juce::MathConstants<float>::twoPi / 6.0f);
            if (corner == 0) nut.startNewSubPath(p); else nut.lineTo(p);
        }
        nut.closeSubPath();
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xffe0e2d9), centre.x - radius, centre.y - radius,
                                              juce::Colour(0xff536561), centre.x + radius, centre.y + radius, false));
        g.fillPath(nut);
        g.setColour(juce::Colour(0xff263d38));
        g.strokePath(nut, juce::PathStrokeType(1.0f));
        const auto cap = bounds.reduced(radius * 0.39f).translated(0.0f, down ? 2.0f : 0.0f);
        g.setColour(juce::Colour(0xff283c36));
        g.fillEllipse(cap.expanded(1.7f).translated(0.0f, 1.0f));
        g.setGradientFill(juce::ColourGradient(juce::Colour(0xfff0eee3), centre.x, cap.getY(),
                                              juce::Colour(0xff899c93), centre.x, cap.getBottom(), false));
        g.fillEllipse(cap);
        g.setColour(juce::Colours::white.withAlpha(0.65f));
        g.drawEllipse(cap.reduced(1.5f), 0.8f);
        if (highlighted || button.hasKeyboardFocus(true))
        {
            g.setColour(amber().withAlpha(button.hasKeyboardFocus(true) ? 0.95f : 0.55f));
            g.drawEllipse(bounds.expanded(3.0f), 1.5f);
        }
    }
};
} // namespace

ShapeCrankAudioProcessorEditor::ShapeCrankAudioProcessorEditor(ShapeCrankAudioProcessor& owner)
    : AudioProcessorEditor(&owner), audioProcessor(owner),
      pedalLookAndFeel(std::make_unique<PedalLookAndFeel>()), tooltips(this, 650)
{
    setOpaque(true);
    setResizable(true, true);
    setResizeLimits(384, 595, 720, 1116);
    getConstrainer()->setFixedAspectRatio((double) designWidth / (double) designHeight);
    setSize((int) designWidth, (int) designHeight);

    configureKnob(shapeSlider, "Shape", "The original Shape curve. Double-click to reset. Shift-drag for fine adjustment.", 0.0);
    configureKnob(driveSlider, "Overdrive", "From light grit to full pedal drive. Zero keeps the shaped signal clean.", 0.0);
    configureKnob(levelSlider, "Level", "Clean output boost. Noon is unity; full is +12 dB. Double-click for unity.", 0.5);
    configureKnob(bassSlider, "Bass trim", "Fine-tune the low end before overdrive. -6 to +6 dB at 160 Hz. Double-click for flat.", 0.0, true);
    configureKnob(trebleSlider, "Treble trim", "Fine-tune the top before overdrive. -6 to +6 dB at 3.2 kHz. Double-click for flat.", 0.0, true);

    int focusOrder = 1;
    for (auto* slider : { &shapeSlider, &driveSlider, &levelSlider, &bassSlider, &trebleSlider })
        slider->setExplicitFocusOrder(focusOrder++);

    bypassButton.setName("Pedal bypass");
    bypassButton.setTitle("Pedal bypass");
    bypassButton.setButtonText("Bypass");
    bypassButton.setTooltip("Click, or press Return with the switch focused, to bypass. Amber light means the pedal is on.");
    bypassButton.setLookAndFeel(pedalLookAndFeel.get());
    bypassButton.setClickingTogglesState(true);
    bypassButton.setWantsKeyboardFocus(true);
    bypassButton.setExplicitFocusOrder(focusOrder);
    bypassButton.onStateChange = [this] { repaint(); };
    addAndMakeVisible(bypassButton);
    tooltips.setLookAndFeel(pedalLookAndFeel.get());

    shapeAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::shape, shapeSlider);
    driveAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::drive, driveSlider);
    levelAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::level, levelSlider);
    bassAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::bass, bassSlider);
    trebleAttachment = std::make_unique<SliderAttachment>(audioProcessor.parameters, ParamIDs::treble, trebleSlider);
    bypassAttachment = std::make_unique<ButtonAttachment>(audioProcessor.parameters, ParamIDs::bypass, bypassButton);

    // APVTS installs parameter text functions when attaching. Apply the pedal's
    // units afterwards while preserving the host parameter text/identity.
    const auto percent = [](double value) { return juce::String(juce::roundToInt(value * 100.0)) + "%"; };
    const auto percentValue = [](const juce::String& value) { return juce::jlimit(0.0, 1.0, value.getDoubleValue() / 100.0); };
    shapeSlider.textFromValueFunction = percent;
    driveSlider.textFromValueFunction = percent;
    shapeSlider.valueFromTextFunction = percentValue;
    driveSlider.valueFromTextFunction = percentValue;
    levelSlider.textFromValueFunction = [](double value) { return decibels(levelDb(value)); };
    levelSlider.valueFromTextFunction = [](const juce::String& value)
    {
        const auto db = juce::jlimit(-18.0, 12.0, value.getDoubleValue());
        return db <= 0.0 ? (db + 18.0) / 36.0 : 0.5 + db / 24.0;
    };
    bassSlider.textFromValueFunction = decibels;
    trebleSlider.textFromValueFunction = decibels;
    const auto trimValue = [](const juce::String& value) { return juce::jlimit(-6.0, 6.0, value.getDoubleValue()); };
    bassSlider.valueFromTextFunction = trimValue;
    trebleSlider.valueFromTextFunction = trimValue;
    lastPedalOn = isPedalOn();
    startTimerHz(20);
}

ShapeCrankAudioProcessorEditor::~ShapeCrankAudioProcessorEditor()
{
    stopTimer();
    tooltips.setLookAndFeel(nullptr);
    bypassButton.setLookAndFeel(nullptr);
    for (auto* slider : { &shapeSlider, &driveSlider, &levelSlider, &bassSlider, &trebleSlider })
        slider->setLookAndFeel(nullptr);
}

void ShapeCrankAudioProcessorEditor::paint(juce::Graphics& graphics)
{
    graphics.fillAll(juce::Colour(0xff141c1d));
    juce::Graphics::ScopedSaveState state(graphics);
    graphics.addTransform(juce::AffineTransform::scale((float) getWidth() / designWidth,
                                                      (float) getHeight() / designHeight));
    const juce::Rectangle<float> body(22.0f, 16.0f, 436.0f, 704.0f);
    graphics.setColour(juce::Colours::black.withAlpha(0.38f));
    graphics.fillRoundedRectangle(body.expanded(5.0f).translated(0.0f, 7.0f), 29.0f);
    graphics.setGradientFill(juce::ColourGradient(juce::Colour(0xff788680), 30.0f, 20.0f,
                                                 juce::Colour(0xff182928), 450.0f, 720.0f, false));
    graphics.fillRoundedRectangle(body, 25.0f);
    graphics.setGradientFill(juce::ColourGradient(juce::Colour(0xff235b59), 80.0f, 36.0f,
                                                 juce::Colour(0xff103b3d), 400.0f, 708.0f, false));
    const auto face = body.reduced(5.0f);
    graphics.fillRoundedRectangle(face, 20.0f);
    {
        juce::Graphics::ScopedSaveState grainState(graphics);
        juce::Path faceMask;
        faceMask.addRoundedRectangle(face, 20.0f);
        graphics.reduceClipRegion(faceMask);
        juce::Random grain(0x53484352);
        for (int dot = 0; dot < 4100; ++dot)
        {
            const auto x = face.getX() + grain.nextFloat() * face.getWidth();
            const auto y = face.getY() + grain.nextFloat() * face.getHeight();
            graphics.setColour((dot % 2 == 0 ? juce::Colours::white : juce::Colours::black).withAlpha(0.032f));
            graphics.fillRect(x, y, 0.65f, 0.65f);
        }
    }
    graphics.setColour(cream().withAlpha(0.19f));
    graphics.drawRoundedRectangle(body.reduced(1.0f), 24.0f, 0.8f);
    graphics.setColour(cream().withAlpha(0.28f));
    graphics.drawRoundedRectangle({ 39.0f, 33.0f, 402.0f, 670.0f }, 12.0f, 0.8f);
    screw(graphics, 43.0f, 38.0f, 0.6f);
    screw(graphics, 437.0f, 38.0f, -0.5f);
    screw(graphics, 43.0f, 698.0f, -0.8f);
    screw(graphics, 437.0f, 698.0f, 0.35f);

    text(graphics, "DEVMUSE", { 63.0f, 60.0f, 354.0f, 19.0f }, 13.0f, cream(), true,
         juce::Justification::centred, 0.25f);
    text(graphics, "SHAPE", { 65.0f, 94.0f, 343.0f, 63.0f }, 69.0f, cream(), true,
         juce::Justification::centredLeft, -0.035f);
    text(graphics, "CRANK", { 65.0f, 150.0f, 343.0f, 63.0f }, 69.0f, cream(), true,
         juce::Justification::centredLeft, -0.035f);
    juce::Path curveMark;
    curveMark.startNewSubPath(342.0f, 131.0f);
    curveMark.cubicTo(357.0f, 131.0f, 354.0f, 110.0f, 370.0f, 110.0f);
    curveMark.cubicTo(386.0f, 110.0f, 382.0f, 131.0f, 399.0f, 131.0f);
    graphics.setColour(amber());
    graphics.strokePath(curveMark, juce::PathStrokeType(2.2f, juce::PathStrokeType::curved));
    graphics.setColour(muted().withAlpha(0.5f));
    graphics.drawLine(343.0f, 141.0f, 399.0f, 141.0f, 0.8f);
    text(graphics, "SC / 02", { 338.0f, 164.0f, 67.0f, 16.0f }, 10.0f, muted(), false,
         juce::Justification::centred, 0.10f);
    text(graphics, "AMP SHAPING  /  OVERDRIVE", { 65.0f, 218.0f, 350.0f, 16.0f }, 10.0f, muted(), false,
         juce::Justification::centredLeft, 0.15f);
    graphics.setColour(cream().withAlpha(0.35f));
    graphics.drawLine(65.0f, 246.0f, 415.0f, 246.0f, 0.9f);

    const std::array<float, 3> centres { 118.0f, 240.0f, 362.0f };
    const std::array<juce::String, 3> names { "SHAPE", "OVERDRIVE", "LEVEL" };
    const std::array<juce::String, 3> values {
        shapeSlider.getTextFromValue(shapeSlider.getValue()),
        driveSlider.getTextFromValue(driveSlider.getValue()),
        levelSlider.getTextFromValue(levelSlider.getValue())
    };
    for (size_t i = 0; i < centres.size(); ++i)
    {
        text(graphics, names[i], { centres[i] - 58.0f, 263.0f, 116.0f, 18.0f }, 12.0f, cream(), true);
        text(graphics, values[i], { centres[i] - 53.0f, 395.0f, 106.0f, 18.0f }, 11.0f, muted());
    }
    graphics.setColour(juce::Colour(0xff0b3032).withAlpha(0.70f));
    graphics.fillRoundedRectangle({ 107.0f, 438.0f, 266.0f, 98.0f }, 12.0f);
    graphics.setColour(cream().withAlpha(0.17f));
    graphics.drawRoundedRectangle({ 107.0f, 438.0f, 266.0f, 98.0f }, 12.0f, 0.8f);
    text(graphics, "CURVE TRIM", { 171.0f, 422.0f, 138.0f, 13.0f }, 9.0f, muted(), true,
         juce::Justification::centred, 0.14f);
    for (auto centre : { 185.0f, 295.0f })
    {
        text(graphics, "-", { centre - 43.0f, 462.0f, 14.0f, 18.0f }, 13.0f, muted());
        text(graphics, "+", { centre + 29.0f, 462.0f, 14.0f, 18.0f }, 13.0f, muted());
        graphics.setColour(cream().withAlpha(0.65f));
        graphics.drawLine(centre, 441.0f, centre, 445.0f, 1.0f);
    }
    text(graphics, "BASS", { 142.0f, 502.0f, 86.0f, 13.0f }, 10.0f, cream(), true);
    text(graphics, "TREBLE", { 252.0f, 502.0f, 86.0f, 13.0f }, 10.0f, cream(), true);
    text(graphics, decibels(bassSlider.getValue()), { 142.0f, 517.0f, 86.0f, 12.0f }, 9.0f, muted());
    text(graphics, decibels(trebleSlider.getValue()), { 252.0f, 517.0f, 86.0f, 12.0f }, 9.0f, muted());

    const auto on = isPedalOn();
    const juce::Rectangle<float> led(234.5f, 561.5f, 11.0f, 11.0f);
    if (on)
    {
        graphics.setGradientFill(juce::ColourGradient(amber().withAlpha(0.22f), 240.0f, 567.0f,
                                                     amber().withAlpha(0.0f), 257.0f, 584.0f, true));
        graphics.fillEllipse(led.expanded(15.0f));
    }
    graphics.setColour(juce::Colour(0xff0a2020));
    graphics.fillEllipse(led.expanded(2.0f));
    graphics.setColour(juce::Colour(0xff62716a));
    graphics.drawEllipse(led.expanded(1.8f), 1.0f);
    graphics.setGradientFill(juce::ColourGradient(on ? juce::Colour(0xffffe7a7) : juce::Colour(0xff5a482e),
                                                 238.0f, 563.0f, on ? juce::Colour(0xffda8b27) : juce::Colour(0xff302b20),
                                                 242.0f, 572.0f, false));
    graphics.fillEllipse(led);
    text(graphics, on ? "ENGAGED" : "BYPASSED", { 164.0f, 582.0f, 152.0f, 15.0f }, 9.0f,
         on ? cream() : muted(), true, juce::Justification::centred, 0.16f);
    text(graphics, "SHAPE  /  DRIVE  /  BOOST", { 90.0f, 687.0f, 300.0f, 15.0f }, 8.5f, muted(), false,
         juce::Justification::centred, 0.16f);
}

void ShapeCrankAudioProcessorEditor::resized()
{
    const auto sx = (float) getWidth() / designWidth;
    const auto sy = (float) getHeight() / designHeight;
    const auto place = [sx, sy](juce::Component& component, float cx, float cy, float size)
    {
        component.setBounds(juce::Rectangle<float>((cx - size * 0.5f) * sx, (cy - size * 0.5f) * sy,
                                                  size * sx, size * sy).toNearestInt());
    };
    place(shapeSlider, 118.0f, 336.0f, 112.0f);
    place(driveSlider, 240.0f, 336.0f, 112.0f);
    place(levelSlider, 362.0f, 336.0f, 112.0f);
    place(bassSlider, 185.0f, 471.0f, 54.0f);
    place(trebleSlider, 295.0f, 471.0f, 54.0f);
    place(bypassButton, 240.0f, 637.0f, 82.0f);
}

void ShapeCrankAudioProcessorEditor::configureKnob(juce::Slider& slider, const juce::String& name,
                                                  const juce::String& tooltip, double defaultValue, bool isTrim)
{
    slider.setName(name);
    slider.setTitle(name);
    slider.setDescription(tooltip);
    slider.setTooltip(tooltip);
    slider.setLookAndFeel(pedalLookAndFeel.get());
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters(startAngle, endAngle, true);
    slider.setDoubleClickReturnValue(true, defaultValue);
    slider.setMouseDragSensitivity(isTrim ? 280 : 220);
    slider.setVelocityModeParameters(0.25, 1, 0.0, true, juce::ModifierKeys::shiftModifier);
    slider.setPopupDisplayEnabled(true, true, this, 1200);
    slider.setWantsKeyboardFocus(true);
    slider.getProperties().set("trim", isTrim);
    slider.onValueChange = [this] { repaint(); };
    addAndMakeVisible(slider);
}

void ShapeCrankAudioProcessorEditor::timerCallback()
{
    const auto pedalOn = isPedalOn();
    if (pedalOn != lastPedalOn)
    {
        lastPedalOn = pedalOn;
        repaint();
    }
}

bool ShapeCrankAudioProcessorEditor::isPedalOn() const
{
    if (const auto* bypass = audioProcessor.parameters.getRawParameterValue(ParamIDs::bypass))
        return bypass->load() <= 0.5f;
    return true;
}
} // namespace shapecrank
