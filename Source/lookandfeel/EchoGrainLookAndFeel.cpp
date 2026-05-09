#include "EchoGrainLookAndFeel.h"

EchoGrainLookAndFeel::EchoGrainLookAndFeel()
{
    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff08111a));
    setColour(juce::PopupMenu::textColourId, juce::Colours::white.withAlpha(0.92f));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff123247));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

juce::Colour EchoGrainLookAndFeel::findAccentForSlider(const juce::Slider& slider) const
{
    const auto name = slider.getName().toLowerCase();

    if (name.contains("lfo"))
        return juce::Colour::fromString("#D9B48D");
    if (name.contains("reverb") || name.contains("formant") || name.contains("glitch"))
        return juce::Colour::fromString("#A99BE8");
    if (name.contains("root") || name.contains("fine") || name.contains("pb ") || name.contains("pb"))
        return juce::Colour::fromString("#8FD7BC");

    return juce::Colour::fromString("#8ED9EA");
}

void EchoGrainLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                            float sliderPosProportional, float rotaryStartAngle,
                                            float rotaryEndAngle, juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(static_cast<float>(x), static_cast<float>(y),
                                         static_cast<float>(width), static_cast<float>(height)).reduced(6.0f, 6.0f);
    const auto diameter = juce::jmin(bounds.getWidth(), bounds.getHeight());
    bounds = juce::Rectangle<float>(diameter, diameter).withCentre(bounds.getCentre());
    const auto radius = diameter * 0.5f;
    const auto centre = bounds.getCentre();
    const auto accent = findAccentForSlider(slider);
    const auto angle = rotaryStartAngle + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // ── Outer ambient glow ──
    juce::ColourGradient outerGlow(accent.withAlpha(0.18f), centre.x, centre.y,
                                    juce::Colours::transparentBlack, bounds.getX(), centre.y, true);
    g.setGradientFill(outerGlow);
    g.fillEllipse(bounds.expanded(radius * 0.22f));

    // ── Background track ──
    juce::Path trackArc;
    trackArc.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                            rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colours::white.withAlpha(0.07f));
    g.strokePath(trackArc, juce::PathStrokeType(2.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ── Value arc glow (wide, soft) ──
    if (sliderPosProportional > 0.001f)
    {
        juce::Path glowArc;
        glowArc.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                               rotaryStartAngle, angle, true);
        g.setColour(accent.withAlpha(0.22f));
        g.strokePath(glowArc, juce::PathStrokeType(6.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // ── Value arc (crisp) ──
    juce::Path valueArc;
    valueArc.addCentredArc(centre.x, centre.y, radius - 2.5f, radius - 2.5f, 0.0f,
                           rotaryStartAngle, angle, true);
    g.setColour(accent.withAlpha(0.92f));
    g.strokePath(valueArc, juce::PathStrokeType(2.7f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    // ── Knob body ──
    const auto knobArea = bounds.reduced(radius * 0.24f);
    juce::ColourGradient shell(juce::Colour::fromString("#1E2D40"), knobArea.getTopLeft(),
                               juce::Colour::fromString("#0D121B"), knobArea.getBottomRight(), false);
    g.setGradientFill(shell);
    g.fillEllipse(knobArea);

    // ── Rim ──
    g.setColour(accent.withAlpha(0.20f));
    g.drawEllipse(knobArea, 1.1f);

    // ── Specular highlight ──
    const float specW = radius * 0.30f;
    const float specH = radius * 0.16f;
    juce::ColourGradient spec(juce::Colours::white.withAlpha(0.26f),
                               centre.x - radius * 0.10f, knobArea.getY() + radius * 0.24f,
                               juce::Colours::transparentBlack,
                               centre.x - radius * 0.10f, knobArea.getY() + radius * 0.24f + specH * 2.0f, false);
    g.setGradientFill(spec);
    g.fillEllipse(centre.x - specW * 0.5f - radius * 0.10f, knobArea.getY() + radius * 0.24f, specW, specH);

    // ── Pointer ──
    juce::Path pointer;
    pointer.addRoundedRectangle(-1.5f, -radius + 10.0f, 3.0f, radius * 0.42f, 1.1f);
    g.setColour(juce::Colours::white.withAlpha(0.88f));
    g.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));

    // ── Centre dot ──
    g.setColour(accent.withAlpha(0.75f));
    g.fillEllipse(centre.x - 2.0f, centre.y - 2.0f, 4.0f, 4.0f);
}

void EchoGrainLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                                const juce::Colour& backgroundColour,
                                                bool shouldDrawButtonAsHighlighted,
                                                bool shouldDrawButtonAsDown)
{
    auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    const bool isResetButton = button.getButtonText().equalsIgnoreCase("RESET");
    const bool isInvButton = button.getButtonText().equalsIgnoreCase("INV") && bounds.getWidth() <= 28.0f;

    if (isInvButton)
    {
        auto base = juce::Colour(0xff101928);
        if (shouldDrawButtonAsDown)
            base = base.brighter(0.2f);

        const bool isOn = button.getToggleState();
        const auto accent = juce::Colour::fromString("#00E5FF");

        if (isOn)
        {
            juce::ColourGradient glow(accent.withAlpha(0.34f), bounds.getCentre(),
                                      juce::Colours::transparentBlack, bounds.getTopLeft(), true);
            g.setGradientFill(glow);
            g.fillRoundedRectangle(bounds.expanded(2.0f), 6.0f);
            base = juce::Colour(0xff0f3143);
        }

        g.setColour(base);
        g.fillRoundedRectangle(bounds, 4.0f);
        g.setColour((isOn ? accent : juce::Colour(0xff6f8598)).withAlpha(0.95f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.1f);
        return;
    }

    auto base = isResetButton ? juce::Colour(0xff1a2434) : backgroundColour;

    if (shouldDrawButtonAsDown || button.getToggleState())
        base = base.brighter(0.18f);
    else if (shouldDrawButtonAsHighlighted)
        base = base.brighter(0.08f);

    const float fillAlpha = isResetButton ? 1.0f : 0.86f;
    const float borderAlpha = isResetButton ? 1.0f : 0.6f;
    const auto endColour = isResetButton ? juce::Colour(0xff111a27) : juce::Colour::fromString("#111A27");

    if (isResetButton)
    {
        // Force a fully opaque base layer first, avoiding any perceived transparency.
        g.setColour(juce::Colour(0xff141f2f));
        g.fillRect(button.getLocalBounds());
    }

    juce::ColourGradient fill(base.withAlpha(fillAlpha), bounds.getTopLeft(),
                              endColour.withAlpha(fillAlpha), bounds.getBottomRight(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, isResetButton ? 6.0f : 8.0f);

    g.setColour((isResetButton ? juce::Colour(0xff8ed9ea) : base).withAlpha(borderAlpha));
    g.drawRoundedRectangle(bounds, isResetButton ? 6.0f : 8.0f, 1.0f);
}

void EchoGrainLookAndFeel::drawButtonText(juce::Graphics& g, juce::TextButton& button,
                                          bool, bool)
{
    const bool isResetButton = button.getButtonText().equalsIgnoreCase("RESET");
    const bool isInvButton = button.getButtonText().equalsIgnoreCase("INV")
                             && button.getWidth() <= 28
                             && button.getHeight() <= 24;

    g.setColour(juce::Colours::white.withAlpha(button.isEnabled() ? (isResetButton ? 1.0f : 0.92f) : 0.45f));
    if (isInvButton && button.getToggleState())
        g.setColour(juce::Colour(0xff041320).withAlpha(button.isEnabled() ? 0.98f : 0.45f));

    g.setFont(juce::Font(juce::FontOptions(isInvButton ? 8.0f : 11.0f, juce::Font::bold)));
    g.drawFittedText(button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, 1);
}

void EchoGrainLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                        int, int, int, int, juce::ComboBox& box)
{
    juce::ignoreUnused(box);

    auto bounds = juce::Rectangle<float>(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)).reduced(0.5f);
    auto accent = juce::Colour::fromString("#8ED9EA");

    juce::ColourGradient fill(juce::Colour::fromString("#162233"), bounds.getTopLeft(),
                              juce::Colour::fromString("#0E1622"), bounds.getBottomRight(), false);
    g.setGradientFill(fill);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(accent.withAlpha(0.55f));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    juce::Path arrow;
    const float arrowX = static_cast<float>(width) - 16.0f;
    const float arrowY = static_cast<float>(height) * 0.5f;
    arrow.startNewSubPath(arrowX - 4.0f, arrowY - 2.0f);
    arrow.lineTo(arrowX, arrowY + 3.0f);
    arrow.lineTo(arrowX + 4.0f, arrowY - 2.0f);
    g.setColour(accent);
    g.strokePath(arrow, juce::PathStrokeType(1.6f));
}

void EchoGrainLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto bounds = label.getLocalBounds().toFloat();

    if (label.isBeingEdited())
    {
        LookAndFeel_V4::drawLabel(g, label);
        return;
    }

    const bool hasBackground = label.findColour(juce::Label::backgroundColourId).getAlpha() > 0;
    if (hasBackground)
    {
        juce::ColourGradient fill(juce::Colour::fromString("#111C2A").withAlpha(0.92f), bounds.getTopLeft(),
                                  juce::Colour::fromString("#0A111A").withAlpha(0.92f), bounds.getBottomRight(), false);
        g.setGradientFill(fill);
        g.fillRoundedRectangle(bounds.reduced(0.5f), 4.0f);
        g.setColour(juce::Colour::fromString("#8ED9EA").withAlpha(0.2f));
        g.drawRoundedRectangle(bounds.reduced(0.5f), 4.0f, 1.0f);
    }

    const auto labelFont = label.getFont();

    g.setColour(label.findColour(juce::Label::textColourId));
    g.setFont(labelFont);
    g.drawFittedText(label.getText(), label.getLocalBounds().reduced(2), label.getJustificationType(), 1);
}

juce::Font EchoGrainLookAndFeel::getLabelFont(juce::Label& label)
{
    return label.getFont();
}