#pragma once

#include <JuceHeader.h>

//==============================================================================
class FormantFilter
{
public:
    FormantFilter();
    ~FormantFilter();
    
    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);
    
    void setFormantFrequency(float frequency);
    void setBandwidth(float bandwidth);
    void setGain(float gain);
    void setDryWetMix(float mix);
    
    // Preset formants
    void setVowel(int vowelType); // 0=A, 1=E, 2=I, 3=O, 4=U
    
private:
    juce::dsp::IIR::Filter<float> formantFilter1, formantFilter2;
    juce::dsp::IIR::Coefficients<float>::Ptr coefficients1, coefficients2;
    
    double sampleRate = 44100.0;
    float formantFreq = 800.0f;
    float bandwidth = 80.0f;
    float gain = 1.0f;
    float dryWetMix = 0.5f;
    
    // Formant frequencies for vowels (F1, F2)
    static const float vowelFormants[5][2];
    
    void updateFilter();
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormantFilter)
};
