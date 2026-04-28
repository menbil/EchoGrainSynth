/*
  ==============================================================================

    SampleManager.h
    Enhanced sample management with embedded save support
    For Ableton Live and external effects compatibility

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

class SampleManager
{
public:
    SampleManager();
    ~SampleManager();

    // Sample loading
    bool loadSample(const juce::File& file);
    bool loadSample(const juce::AudioBuffer<float>& buffer, double sampleRate);
    void clearSample();

    // Sample access
    const juce::AudioBuffer<float>& getSampleBuffer() const { return sampleBuffer; }
    double getSampleRate() const { return currentSampleRate; }
    juce::String getSampleName() const { return sampleName; }
    bool hasSample() const { return sampleBuffer.getNumSamples() > 0; }

    // Sample persistence for project saves
    juce::ValueTree getState() const;
    void setState(const juce::ValueTree& state);

    // Sample encoding for embedded storage
    juce::String encodeToBase64() const;
    bool decodeFromBase64(const juce::String& base64Data, const juce::String& name);

private:
    juce::AudioBuffer<float> sampleBuffer;
    double currentSampleRate;
    juce::String sampleName;
    
    juce::AudioFormatManager formatManager;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SampleManager)
};
