/*
  ==============================================================================

    SampleManager.cpp
    Enhanced sample management with embedded save support

  ==============================================================================
*/

#include "SampleManager.h"

SampleManager::SampleManager()
    : currentSampleRate(44100.0)
{
    formatManager.registerBasicFormats();
}

SampleManager::~SampleManager()
{
}

bool SampleManager::loadSample(const juce::File& file)
{
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader != nullptr)
    {
        auto numChannels = static_cast<int>(reader->numChannels);
        auto numSamples = static_cast<int>(reader->lengthInSamples);
        
        sampleBuffer.setSize(numChannels, numSamples);
        reader->read(&sampleBuffer, 0, numSamples, 0, true, true);
        
        currentSampleRate = reader->sampleRate;
        sampleName = file.getFileNameWithoutExtension();
        
        return true;
    }
    
    return false;
}

bool SampleManager::loadSample(const juce::AudioBuffer<float>& buffer, double sampleRate)
{
    sampleBuffer.makeCopyOf(buffer);
    currentSampleRate = sampleRate;
    sampleName = "Recorded Sample";
    return true;
}

void SampleManager::clearSample()
{
    sampleBuffer.clear();
    sampleName.clear();
}

juce::ValueTree SampleManager::getState() const
{
    juce::ValueTree state("SampleManager");
    
    state.setProperty("sampleName", sampleName, nullptr);
    state.setProperty("sampleRate", currentSampleRate, nullptr);
    
    if (hasSample())
    {
        // Embed sample data as base64 for complete project portability
        state.setProperty("sampleData", encodeToBase64(), nullptr);
        state.setProperty("numChannels", sampleBuffer.getNumChannels(), nullptr);
        state.setProperty("numSamples", sampleBuffer.getNumSamples(), nullptr);
    }
    
    return state;
}

void SampleManager::setState(const juce::ValueTree& state)
{
    sampleName = state.getProperty("sampleName", "");
    currentSampleRate = state.getProperty("sampleRate", 44100.0);
    
    if (state.hasProperty("sampleData"))
    {
        juce::String base64Data = state.getProperty("sampleData");
        int numSamples = state.getProperty("numSamples", 0);
        
        if (numSamples > 0)
        {
            decodeFromBase64(base64Data, sampleName);
        }
    }
}

juce::String SampleManager::encodeToBase64() const
{
    if (!hasSample())
        return {};
    
    juce::MemoryBlock block;
    juce::MemoryOutputStream stream(block, false);
    
    // Write sample data in a simple format
    stream.writeInt(sampleBuffer.getNumChannels());
    stream.writeInt(sampleBuffer.getNumSamples());
    stream.writeDouble(currentSampleRate);
    
    for (int ch = 0; ch < sampleBuffer.getNumChannels(); ++ch)
    {
        const float* channelData = sampleBuffer.getReadPointer(ch);
        for (int sample = 0; sample < sampleBuffer.getNumSamples(); ++sample)
        {
            stream.writeFloat(channelData[sample]);
        }
    }
    
    return block.toBase64Encoding();
}

bool SampleManager::decodeFromBase64(const juce::String& base64Data, const juce::String& name)
{
    juce::MemoryBlock block;
    if (!block.fromBase64Encoding(base64Data))
        return false;
    
    juce::MemoryInputStream stream(block, false);
    
    int numChannels = stream.readInt();
    int numSamples = stream.readInt();
    double sampleRate = stream.readDouble();
    
    if (numChannels <= 0 || numSamples <= 0 || sampleRate <= 0)
        return false;
    
    sampleBuffer.setSize(numChannels, numSamples);
    
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = sampleBuffer.getWritePointer(ch);
        for (int sample = 0; sample < numSamples; ++sample)
        {
            channelData[sample] = stream.readFloat();
        }
    }
    
    currentSampleRate = sampleRate;
    sampleName = name;
    
    return true;
}
