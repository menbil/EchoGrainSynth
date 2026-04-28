/*
  ==============================================================================

    AdsrComponent.cpp
    ADSR Envelope Component with visual envelope and 4 sliders

  ==============================================================================
*/

#include "AdsrComponent.h"

//==============================================================================
AdsrComponent::AdsrComponent(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts)
{
    // Setup ADSR sliders
    setupSlider(attackSlider, attackLabel, "ATTACK", "adsrAttack", attackAttachment);
    setupSlider(decaySlider, decayLabel, "DECAY", "adsrDecay", decayAttachment);
    setupSlider(sustainSlider, sustainLabel, "SUSTAIN", "adsrSustain", sustainAttachment);
    setupSlider(releaseSlider, releaseLabel, "RELEASE", "adsrRelease", releaseAttachment);
    
    // Compact slider style for ADSR
    for (auto* slider : {&attackSlider, &decaySlider, &sustainSlider, &releaseSlider})
    {
        slider->setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 56, 14);
        slider->setColour(juce::Slider::rotarySliderFillColourId, juce::Colour::fromString("#FF006E"));
        slider->setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour::fromString("#00FFFF"));
        slider->setColour(juce::Slider::thumbColourId, juce::Colours::white);
        slider->setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.95f));
        slider->setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff11182a).withAlpha(0.95f));
        slider->setColour(juce::Slider::textBoxOutlineColourId, juce::Colour::fromString("#00FFFF").withAlpha(0.28f));
        // Remove onDoubleClick - not available in this JUCE version
        slider->onValueChange = [this]
        {
            repaint();
        };
    }

    // Improve low-time precision while keeping full 0..600ms range available.
    attackSlider.setSkewFactorFromMidPoint(60.0);
    decaySlider.setSkewFactorFromMidPoint(90.0);
    releaseSlider.setSkewFactorFromMidPoint(120.0);

    auto msText = [](double value)
    {
        if (value < 1.0)
            return juce::String(value, 2) + " ms";
        if (value < 10.0)
            return juce::String(value, 1) + " ms";
        return juce::String(value, 0) + " ms";
    };

    attackSlider.textFromValueFunction = msText;
    decaySlider.textFromValueFunction = msText;
    releaseSlider.textFromValueFunction = msText;
    sustainSlider.textFromValueFunction = [](double value)
    {
        return juce::String(static_cast<int>(std::round(value * 100.0))) + "%";
    };
    
    // Special setup for sustain (0-1 range, different default)
    sustainSlider.setDoubleClickReturnValue(true, 0.7);
    sustainSlider.getProperties().set("defaultValue", 0.7);
    
    // Label styling
    for (auto* label : {&attackLabel, &decayLabel, &sustainLabel, &releaseLabel})
    {
        label->setFont(juce::Font(juce::FontOptions(8.0f, juce::Font::bold)));
        label->setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.95f));
        label->setJustificationType(juce::Justification::centred);
    }
}

AdsrComponent::~AdsrComponent() = default;

void AdsrComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    
    // Draw envelope area background
    envelopeArea = bounds.removeFromTop(bounds.getHeight() * 0.6f).reduced(5);
    
    g.setColour(juce::Colour::fromString("#1A0A2E").withAlpha(0.6f));
    g.fillRoundedRectangle(envelopeArea, 5.0f);
    
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.3f));
    g.drawRoundedRectangle(envelopeArea, 5.0f, 1.0f);
    
    // Draw ADSR envelope
    drawEnvelope(g, envelopeArea.reduced(5));
    
    // Label
    g.setColour(juce::Colour::fromString("#00FFFF"));
    g.setFont(juce::Font(juce::FontOptions(12.0f, juce::Font::bold)));
    const juce::Rectangle<int> adsrTextBounds(
        juce::roundToInt(envelopeArea.getX()),
        juce::roundToInt(envelopeArea.getY() + 2.0f),
        juce::roundToInt(envelopeArea.getWidth()),
        15);
    g.drawText("ADSR", adsrTextBounds, juce::Justification::centred);
}

void AdsrComponent::resized()
{
    auto bounds = getLocalBounds();
    
    // Reserve top area for envelope display
    bounds.removeFromTop(static_cast<int>(bounds.getHeight() * 0.50f));
    
    // Arrange labels + sliders in four dedicated slots for clarity.
    auto controlsArea = bounds.reduced(1);
    const int slotWidth = controlsArea.getWidth() / 4;
    const int slotHeight = controlsArea.getHeight();
    const int adsrKnobSize = juce::jlimit(62, 84, juce::jmin(slotWidth - 6, slotHeight - 18));

    auto layoutSlot = [&](juce::Slider& slider, juce::Label& label)
    {
        auto slot = controlsArea.removeFromLeft(slotWidth).reduced(2, 0);
        auto labelArea = slot.removeFromTop(16);
        label.setBounds(labelArea);
        slider.setBounds(slot.withSizeKeepingCentre(adsrKnobSize, adsrKnobSize));
    };

    layoutSlot(attackSlider, attackLabel);
    layoutSlot(decaySlider, decayLabel);
    layoutSlot(sustainSlider, sustainLabel);
    layoutSlot(releaseSlider, releaseLabel);
}

void AdsrComponent::mouseDown(const juce::MouseEvent& event)
{
    if (!envelopeArea.contains(event.position))
        return;

    isDraggingEnvelope = true;
    beginEnvelopeGesture();
    updateFromEnvelopeDrag(event.position, event.mods);
}

void AdsrComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!isDraggingEnvelope)
        return;

    updateFromEnvelopeDrag(event.position, event.mods);
}

void AdsrComponent::mouseUp(const juce::MouseEvent&)
{
    if (!isDraggingEnvelope)
        return;

    isDraggingEnvelope = false;
    endEnvelopeGesture();
}

void AdsrComponent::setupSlider(juce::Slider& slider, juce::Label& label, 
                               const juce::String& labelText, const juce::String& parameterID,
                               std::unique_ptr<SliderAttachment>& attachment)
{
    addAndMakeVisible(slider);
    addAndMakeVisible(label);
    
    label.setText(labelText, juce::dontSendNotification);
    
    attachment = std::make_unique<SliderAttachment>(valueTreeState, parameterID, slider);
}

void AdsrComponent::drawEnvelope(juce::Graphics& g, juce::Rectangle<float> bounds)
{
    if (bounds.isEmpty()) return;
    
    // Get current ADSR values
    const double attack = attackSlider.getValue();
    const double decay = decaySlider.getValue();
    const double sustain = sustainSlider.getValue();
    const double release = releaseSlider.getValue();
    
    // Perceptual time scaling keeps the graph reactive across wide millisecond ranges.
    const double attackWeight = std::log1p(attack);
    const double decayWeight = std::log1p(decay);
    const double releaseWeight = std::log1p(release);
    const double sustainWeight = 1.4;
    const double totalWeight = attackWeight + decayWeight + sustainWeight + releaseWeight;

    const double attackRatio = attackWeight / totalWeight;
    const double decayRatio = decayWeight / totalWeight;
    const double sustainRatio = sustainWeight / totalWeight;
    
    // Calculate envelope points
    juce::Path envelopePath;
    
    float x = bounds.getX();
    float y = bounds.getBottom();
    float width = bounds.getWidth();
    float height = bounds.getHeight();
    
    // Start point (silence)
    envelopePath.startNewSubPath(x, y);
    
    // Attack phase
    x += static_cast<float>(attackRatio * static_cast<double>(width));
    envelopePath.lineTo(x, bounds.getY()); // Peak
    
    // Decay phase  
    x += static_cast<float>(decayRatio * static_cast<double>(width));
    float sustainY = bounds.getY() + static_cast<float>((1.0 - sustain) * static_cast<double>(height));
    envelopePath.lineTo(x, sustainY);
    
    // Sustain phase
    x += static_cast<float>(sustainRatio * static_cast<double>(width));
    envelopePath.lineTo(x, sustainY);
    
    // Release phase
    x = bounds.getRight();
    envelopePath.lineTo(x, y);
    
    // Draw envelope with glow effect
    for (int i = 3; i > 0; --i)
    {
        g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.3f / i));
        g.strokePath(envelopePath, juce::PathStrokeType(2.0f + i, juce::PathStrokeType::curved));
    }
    
    g.setColour(juce::Colour::fromString("#FF006E"));
    g.strokePath(envelopePath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));

    // Subtle fill helps readability while the envelope evolves.
    juce::Path fillPath(envelopePath);
    fillPath.lineTo(bounds.getRight(), bounds.getBottom());
    fillPath.lineTo(bounds.getX(), bounds.getBottom());
    fillPath.closeSubPath();
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.12f));
    g.fillPath(fillPath);
    
    // Draw grid lines
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.2f));
    for (int i = 1; i < 4; ++i)
    {
        float gridY = bounds.getY() + (height / 4.0f) * i;
        g.drawLine(bounds.getX(), gridY, bounds.getRight(), gridY, 1.0f);
    }
}

void AdsrComponent::updateFromEnvelopeDrag(juce::Point<float> position, juce::ModifierKeys modifiers)
{
    auto editableBounds = envelopeArea.reduced(5.0f);
    if (editableBounds.isEmpty())
        return;

    const float nx = juce::jlimit(0.0f, 1.0f, (position.x - editableBounds.getX()) / editableBounds.getWidth());
    const float ny = juce::jlimit(0.0f, 1.0f, 1.0f - ((position.y - editableBounds.getY()) / editableBounds.getHeight()));

    const auto toTimeMs = [](float t)
    {
        // Nonlinear mapping gives much finer control in the 0..120ms zone.
        return static_cast<double>(600.0f * std::pow(juce::jlimit(0.0f, 1.0f, t), 2.3f));
    };

    const bool fineAdjust = modifiers.isShiftDown();
    const double blend = fineAdjust ? 0.2 : 1.0;
    const auto applyBlended = [blend](juce::Slider& slider, double target)
    {
        const double current = slider.getValue();
        const double value = current + (target - current) * blend;
        slider.setValue(value, juce::sendNotificationSync);
    };

    // Drag zones: left=attack, middle=decay+sustain, right=release+sustain.
    if (nx <= 0.33f)
    {
        const float t = nx / 0.33f;
        applyBlended(attackSlider, toTimeMs(t));
    }
    else if (nx <= 0.66f)
    {
        const float t = (nx - 0.33f) / 0.33f;
        applyBlended(decaySlider, toTimeMs(t));
        applyBlended(sustainSlider, ny);
    }
    else
    {
        const float t = (nx - 0.66f) / 0.34f;
        applyBlended(releaseSlider, toTimeMs(t));
        applyBlended(sustainSlider, ny);
    }
}

void AdsrComponent::beginEnvelopeGesture()
{
    if (auto* p = valueTreeState.getParameter("adsrAttack")) p->beginChangeGesture();
    if (auto* p = valueTreeState.getParameter("adsrDecay")) p->beginChangeGesture();
    if (auto* p = valueTreeState.getParameter("adsrSustain")) p->beginChangeGesture();
    if (auto* p = valueTreeState.getParameter("adsrRelease")) p->beginChangeGesture();
}

void AdsrComponent::endEnvelopeGesture()
{
    if (auto* p = valueTreeState.getParameter("adsrAttack")) p->endChangeGesture();
    if (auto* p = valueTreeState.getParameter("adsrDecay")) p->endChangeGesture();
    if (auto* p = valueTreeState.getParameter("adsrSustain")) p->endChangeGesture();
    if (auto* p = valueTreeState.getParameter("adsrRelease")) p->endChangeGesture();
}
