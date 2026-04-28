#pragma once

#include <JuceHeader.h>

//==============================================================================
class StretchEffect
{
public:
    StretchEffect();
    ~StretchEffect();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);
    
    void setStretchRatio(float ratio); // 0.5 = half speed, 2.0 = double speed
    void setPitchShift(float semitones);
    void setWindowSize(int size);
    void setOverlapFactor(float overlap);
    
private:
    double sampleRate = 44100.0;
    
    float stretchRatio = 1.0f;
    float pitchShift = 0.0f;
    int windowSize = 2048;
    float overlapFactor = 0.75f;
    
    // Time-stretching state
    juce::AudioBuffer<float> inputBuffer;
    juce::AudioBuffer<float> outputBuffer;
    juce::AudioBuffer<float> windowBuffer;
    
    int inputWritePos = 0;
    int outputReadPos = 0;
    float phaseIncrement = 1.0f;
    
    // Window function
    std::vector<float> hanningWindow;
    
    void createHanningWindow();
    void processTimeStretch(juce::AudioBuffer<float>& buffer);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(StretchEffect)
};
