#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * LFO Monitor Component - Overlay ORANGE
 * Affiche deux sinusoïdes fines (POS/PITCH) en temps réel
 */
class LfoMonitorComponent : public juce::Component, public juce::Timer
{
public:
    LfoMonitorComponent(EchoGrainSynthAudioProcessor& processor);
    ~LfoMonitorComponent() override;

    void paint(juce::Graphics& g) override;
    void timerCallback() override;
    void resized() override;
    
    void startMonitoring();
    void stopMonitoring();

private:
    EchoGrainSynthAudioProcessor& audioProcessor;
    
    // Animation phase for LFO visualization
    float posLfoPhase = 0.0f;
    float pitchLfoPhase = 0.0f;
    
    // Cached parameters for smooth animation
    float posLfoFreq = 1.0f;
    float posLfoDepth = 0.0f;
    float pitchLfoFreq = 1.0f;
    float pitchLfoDepth = 0.0f;
    
    // Visual bounds for each LFO
    juce::Rectangle<float> posLfoBounds;
    juce::Rectangle<float> pitchLfoBounds;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LfoMonitorComponent)
};
