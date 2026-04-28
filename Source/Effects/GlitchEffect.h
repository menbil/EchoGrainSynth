#pragma once

#include <JuceHeader.h>
#include <random>

//==============================================================================
class GlitchEffect
{
public:
    GlitchEffect();
    ~GlitchEffect();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);
    
    void setIntensity(float intensity);
    void setRate(float rate);
    void setProbability(float probability);  // NEW: Glitch probability
    void setBitCrush(float bitCrush);
    void setStutter(float stutter);
    
    // NEW: Check if glitch is currently active (for scope visualization)
    bool isGlitchCurrentlyActive() const { return glitchActive; }
    float getCurrentIntensity() const { return glitchActive ? intensity : 0.0f; }
    
private:
    double sampleRate = 44100.0;
    
    // Glitch parameters
    float intensity = 0.0f;
    float rate = 0.1f;
    float probability = 0.5f;        // NEW: Probability of glitch triggers
    float bitCrushAmount = 0.0f;
    float stutterAmount = 0.0f;
    
    // Internal state
    std::mt19937 randomGenerator;
    std::uniform_real_distribution<float> distribution;
    
    juce::AudioBuffer<float> stutterBuffer;
    int stutterLength = 0;
    int stutterPosition = 0;
    bool isStuttering = false;
    
    float glitchTimer = 0.0f;
    bool glitchActive = false;
    
    // Bit crushing
    float crushFactor = 1.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GlitchEffect)
};
