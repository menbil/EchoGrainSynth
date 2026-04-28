/*
  ==============================================================================

    XYPadMappingPanel.h
    XY Pad mapping configuration panel with 4 slots

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class XYPadMappingPanel : public juce::Component
{
public:
    XYPadMappingPanel(juce::AudioProcessorValueTreeState& apvts);
    ~XYPadMappingPanel() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    using ComboBoxAttachment = juce::AudioProcessorValueTreeState::ComboBoxAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    struct MappingSlot
    {
        juce::ComboBox targetCombo;
        juce::ComboBox axisCombo;
        juce::Slider minSlider, maxSlider;
      juce::TextButton invertButton;
        
        std::unique_ptr<ComboBoxAttachment> targetAttachment;
        std::unique_ptr<ComboBoxAttachment> axisAttachment;
        std::unique_ptr<SliderAttachment> minAttachment;
        std::unique_ptr<SliderAttachment> maxAttachment;
        std::unique_ptr<ButtonAttachment> invertAttachment;
    };
    
    std::array<MappingSlot, 4> mappingSlots;
    
    void setupMappingSlot(MappingSlot& slot, int slotIndex);
    void setupComboBox(juce::ComboBox& combo, const juce::StringArray& items);
    void setupSlider(juce::Slider& slider, double min, double max, double defaultValue);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYPadMappingPanel)
};
