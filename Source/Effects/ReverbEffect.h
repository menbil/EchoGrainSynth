#pragma once

#include <JuceHeader.h>

//==============================================================================
class ReverbEffect
{
public:
    ReverbEffect();
    ~ReverbEffect();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);
    
    void setRoomSize(float roomSize);
    void setDamping(float damping);
    void setWetLevel(float wetLevel);
    void setDryLevel(float dryLevel);
    void setWidth(float width);
    
private:
    juce::Reverb reverb;
    juce::Reverb::Parameters reverbParams;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbEffect)
};
