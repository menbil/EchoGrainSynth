#pragma once

#include <JuceHeader.h>

//==============================================================================
class GranularSound : public juce::SynthesiserSound
{
public:
    GranularSound() {}
    
    bool appliesToNote(int midiNoteNumber) override { juce::ignoreUnused(midiNoteNumber); return true; }
    bool appliesToChannel(int midiChannel) override { juce::ignoreUnused(midiChannel); return true; }
    
    void setSampleData(const juce::AudioBuffer<float>& newSampleData)
    {
        sampleData.makeCopyOf(newSampleData);
    }
    
    const juce::AudioBuffer<float>& getSampleData() const { return sampleData; }
    bool hasSampleData() const { return sampleData.getNumSamples() > 0; }
    
private:
    juce::AudioBuffer<float> sampleData;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularSound)
};
