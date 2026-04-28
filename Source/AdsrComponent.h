/*
  ==============================================================================

    AdsrComponent.h
    ADSR Envelope Component with visual envelope and 4 sliders

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class AdsrComponent : public juce::Component
{
public:
    AdsrComponent(juce::AudioProcessorValueTreeState& apvts);
    ~AdsrComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
  void mouseDown(const juce::MouseEvent& event) override;
  void mouseDrag(const juce::MouseEvent& event) override;
  void mouseUp(const juce::MouseEvent& event) override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    
    juce::AudioProcessorValueTreeState& valueTreeState;
    
    // ADSR Sliders
    juce::Slider attackSlider, decaySlider, sustainSlider, releaseSlider;
    juce::Label attackLabel, decayLabel, sustainLabel, releaseLabel;
    
    // Attachments
    std::unique_ptr<SliderAttachment> attackAttachment;
    std::unique_ptr<SliderAttachment> decayAttachment;
    std::unique_ptr<SliderAttachment> sustainAttachment;
    std::unique_ptr<SliderAttachment> releaseAttachment;
    
    // Visual envelope display
    juce::Rectangle<float> envelopeArea;

    bool isDraggingEnvelope = false;

    void updateFromEnvelopeDrag(juce::Point<float> position, juce::ModifierKeys modifiers);
    void beginEnvelopeGesture();
    void endEnvelopeGesture();
    
    void setupSlider(juce::Slider& slider, juce::Label& label, 
                    const juce::String& labelText, const juce::String& parameterID,
                    std::unique_ptr<SliderAttachment>& attachment);
    
    void drawEnvelope(juce::Graphics& g, juce::Rectangle<float> bounds);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AdsrComponent)
};
