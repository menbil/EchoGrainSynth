/*
  ==============================================================================

    XYPadMappingPanel.cpp
    XY Pad mapping configuration panel with 4 slots

  ==============================================================================
*/

#include "XYPadMappingPanel.h"

//==============================================================================
XYPadMappingPanel::XYPadMappingPanel(juce::AudioProcessorValueTreeState& apvts)
    : valueTreeState(apvts)
{
    // Setup all 4 mapping slots
    for (int i = 0; i < 4; ++i)
    {
        setupMappingSlot(mappingSlots[i], i);
    }
}

XYPadMappingPanel::~XYPadMappingPanel() = default;

void XYPadMappingPanel::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    g.setColour(juce::Colour(0xff0d1623));
    g.fillRoundedRectangle(bounds, 5.0f);

    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.35f));
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

    auto content = bounds.reduced(6.0f);
    const float titleHeight = 16.0f;
    const float headerHeight = 16.0f;

    auto titleArea = content.removeFromTop(titleHeight);
    g.setColour(juce::Colour::fromString("#00FFFF"));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    g.drawText("XY MAPPING", titleArea, juce::Justification::centredLeft);

    content.removeFromTop(2.0f);
    auto headerArea = content.removeFromTop(headerHeight);
    g.setColour(juce::Colour(0xff122236));
    g.fillRoundedRectangle(headerArea, 3.0f);

    const float w = headerArea.getWidth();
    auto colTarget = headerArea.removeFromLeft(w * 0.42f);
    auto colAxis = headerArea.removeFromLeft(w * 0.12f);
    auto colMin = headerArea.removeFromLeft(w * 0.17f);
    auto colMax = headerArea.removeFromLeft(w * 0.17f);
    auto colInv = headerArea;

    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.setFont(juce::Font(juce::FontOptions(9.0f, juce::Font::bold)));
    g.drawText("TARGET", colTarget, juce::Justification::centredLeft);
    g.drawText("AXIS", colAxis, juce::Justification::centred);
    g.drawText("MIN", colMin, juce::Justification::centred);
    g.drawText("MAX", colMax, juce::Justification::centred);
    g.drawText("INV", colInv, juce::Justification::centred);

    content.removeFromTop(2.0f);
    const float rowGap = 3.0f;
    const float rowHeight = (content.getHeight() - rowGap * 3.0f) / 4.0f;

    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.18f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = content.getY() + static_cast<float>(i) * (rowHeight + rowGap) - (rowGap * 0.5f);
        g.drawHorizontalLine(static_cast<int>(y), content.getX(), content.getRight());
    }
}

void XYPadMappingPanel::resized()
{
    auto bounds = getLocalBounds();
    bounds.reduce(6, 6);
    bounds.removeFromTop(16); // title
    bounds.removeFromTop(2);
    bounds.removeFromTop(16); // header
    bounds.removeFromTop(2);

    const int rowGap = 3;
    const int slotHeight = (bounds.getHeight() - rowGap * 3) / 4;
    
    for (int i = 0; i < 4; ++i)
    {
        auto slotBounds = bounds.removeFromTop(slotHeight);
        if (i < 3)
            bounds.removeFromTop(rowGap);

        auto& slot = mappingSlots[i];

        const int rowYPad = juce::jmax(0, (slotBounds.getHeight() - 18) / 2);
        slotBounds.reduce(0, rowYPad);

        const int totalW = slotBounds.getWidth();
        const int targetW = static_cast<int>(totalW * 0.42f);
        const int axisW = static_cast<int>(totalW * 0.12f);
        const int minW = static_cast<int>(totalW * 0.17f);
        const int maxW = static_cast<int>(totalW * 0.17f);

        auto targetArea = slotBounds.removeFromLeft(targetW).reduced(1, 0);
        auto axisArea = slotBounds.removeFromLeft(axisW).reduced(1, 0);
        auto minArea = slotBounds.removeFromLeft(minW).reduced(1, 2);
        auto maxArea = slotBounds.removeFromLeft(maxW).reduced(1, 2);
        auto invArea = slotBounds.reduced(1, 0);

        slot.targetCombo.setBounds(targetArea);
        slot.axisCombo.setBounds(axisArea);
        slot.minSlider.setBounds(minArea);
        slot.maxSlider.setBounds(maxArea);
        slot.invertButton.setBounds(juce::Rectangle<int>(20, 20).withCentre(invArea.getCentre()));
    }
}

void XYPadMappingPanel::setupMappingSlot(MappingSlot& slot, int slotIndex)
{
    auto slotPrefix = juce::String("xySlot") + juce::String(slotIndex + 1);
    
    // Target parameter combo
    juce::StringArray targetOptions = {"none", "grainSize", "density", "position", "pitch", 
                                      "pan", "formantFreq", "reverbWet"};
    setupComboBox(slot.targetCombo, targetOptions);
    slot.targetAttachment = std::make_unique<ComboBoxAttachment>(valueTreeState, 
                                                                slotPrefix + "Target", 
                                                                slot.targetCombo);
    
    // Axis combo (X/Y)
    juce::StringArray axisOptions = {"X", "Y"};
    setupComboBox(slot.axisCombo, axisOptions);
    slot.axisAttachment = std::make_unique<ComboBoxAttachment>(valueTreeState, 
                                                              slotPrefix + "Axis", 
                                                              slot.axisCombo);
    
    // Min/Max sliders
    setupSlider(slot.minSlider, 0.0, 1.0, 0.0);
    slot.minAttachment = std::make_unique<SliderAttachment>(valueTreeState, 
                                                           slotPrefix + "Min", 
                                                           slot.minSlider);
    
    setupSlider(slot.maxSlider, 0.0, 1.0, 1.0);
    slot.maxAttachment = std::make_unique<SliderAttachment>(valueTreeState, 
                                                           slotPrefix + "Max", 
                                                           slot.maxSlider);
    
    // Invert toggle
    addAndMakeVisible(slot.invertButton);
    slot.invertButton.setButtonText("INV");
    slot.invertButton.setClickingTogglesState(true);
    slot.invertButton.setTriggeredOnMouseDown(true);
    slot.invertButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff101928));
    slot.invertButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour::fromString("#00E5FF").withAlpha(0.85f));
    slot.invertButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.92f));
    slot.invertButton.setColour(juce::TextButton::textColourOnId, juce::Colour(0xff041320));
    slot.invertAttachment = std::make_unique<ButtonAttachment>(valueTreeState, 
                                                              slotPrefix + "Invert", 
                                                              slot.invertButton);
    
    // Add all components
    addAndMakeVisible(slot.targetCombo);
    addAndMakeVisible(slot.axisCombo);
    addAndMakeVisible(slot.minSlider);
    addAndMakeVisible(slot.maxSlider);
}

void XYPadMappingPanel::setupComboBox(juce::ComboBox& combo, const juce::StringArray& items)
{
    combo.addItemList(items, 1);
    combo.setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff13253a));
    combo.setColour(juce::ComboBox::textColourId, juce::Colours::white.withAlpha(0.95f));
    combo.setColour(juce::ComboBox::outlineColourId, juce::Colour::fromString("#00FFFF").withAlpha(0.45f));
    combo.setColour(juce::ComboBox::arrowColourId, juce::Colour::fromString("#FF006E"));
    combo.setJustificationType(juce::Justification::centredLeft);
}

void XYPadMappingPanel::setupSlider(juce::Slider& slider, double min, double max, double defaultValue)
{
    slider.setRange(min, max, 0.01);
    slider.setValue(defaultValue);
    slider.setSliderStyle(juce::Slider::LinearHorizontal);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setColour(juce::Slider::backgroundColourId, juce::Colour(0xff1a2a3b));
    slider.setColour(juce::Slider::trackColourId, juce::Colour::fromString("#00FFFF").withAlpha(0.55f));
    slider.setColour(juce::Slider::thumbColourId, juce::Colour::fromString("#FF006E"));
}
