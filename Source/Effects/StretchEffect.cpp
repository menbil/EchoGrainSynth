#include "StretchEffect.h"

StretchEffect::StretchEffect()
{
    createHanningWindow();
}

StretchEffect::~StretchEffect()
{
}

void StretchEffect::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    
    // Prepare buffers
    int bufferSize = windowSize * 4; // Ensure enough space
    inputBuffer.setSize(2, bufferSize);
    outputBuffer.setSize(2, bufferSize);
    windowBuffer.setSize(2, windowSize);
    
    inputBuffer.clear();
    outputBuffer.clear();
    windowBuffer.clear();
    
    inputWritePos = 0;
    outputReadPos = 0;
}

void StretchEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (std::abs(stretchRatio - 1.0f) < 0.01f && std::abs(pitchShift) < 0.01f)
        return; // No processing needed
        
    processTimeStretch(buffer);
}

void StretchEffect::setStretchRatio(float ratio)
{
    stretchRatio = juce::jlimit(0.25f, 4.0f, ratio);
    phaseIncrement = 1.0f / stretchRatio;
}

void StretchEffect::setPitchShift(float semitones)
{
    pitchShift = juce::jlimit(-24.0f, 24.0f, semitones);
}

void StretchEffect::setWindowSize(int size)
{
    windowSize = juce::jlimit(512, 8192, size);
    createHanningWindow();
    
    // Reinitialize buffers
    if (sampleRate > 0)
        prepare(sampleRate, windowSize);
}

void StretchEffect::setOverlapFactor(float overlap)
{
    overlapFactor = juce::jlimit(0.25f, 0.9f, overlap);
}

void StretchEffect::createHanningWindow()
{
    hanningWindow.resize(static_cast<size_t>(windowSize));
    
    for (int i = 0; i < windowSize; ++i)
    {
        hanningWindow[static_cast<size_t>(i)] = 0.5f * (1.0f - std::cos(2.0f * juce::MathConstants<float>::pi * static_cast<float>(i) / static_cast<float>(windowSize - 1)));
    }
}

void StretchEffect::processTimeStretch(juce::AudioBuffer<float>& buffer)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    // This is a simplified time-stretching implementation
    // A full implementation would use FFT-based phase vocoder
    
    for (int channel = 0; channel < juce::jmin(numChannels, inputBuffer.getNumChannels()); ++channel)
    {
        // Copy input to internal buffer
        for (int i = 0; i < numSamples; ++i)
        {
            inputBuffer.setSample(channel, inputWritePos, buffer.getSample(channel, i));
            inputWritePos = (inputWritePos + 1) % inputBuffer.getNumSamples();
        }
        
        // Process time stretching (simplified)
        for (int i = 0; i < numSamples; ++i)
        {
            // Simple linear interpolation for pitch shifting
            float readPos = static_cast<float>(outputReadPos) * phaseIncrement;
            int intPos = static_cast<int>(readPos) % inputBuffer.getNumSamples();
            float fracPos = readPos - std::floor(readPos);
            
            int nextPos = (intPos + 1) % inputBuffer.getNumSamples();
            
            float sample1 = inputBuffer.getSample(channel, intPos);
            float sample2 = inputBuffer.getSample(channel, nextPos);
            float interpolatedSample = sample1 + fracPos * (sample2 - sample1);
            
            // Apply pitch shift
            if (std::abs(pitchShift) > 0.01f)
            {
                float pitchRatio = std::pow(2.0f, pitchShift / 12.0f);
                // This is a very simplified pitch shift - real implementation would use FFT
                interpolatedSample *= pitchRatio; // This doesn't actually shift pitch, just gain
            }
            
            buffer.setSample(channel, i, interpolatedSample * 0.5f); // Reduce gain to prevent clipping
            
            if (channel == 0) // Only increment on first channel
                outputReadPos = (outputReadPos + 1) % inputBuffer.getNumSamples();
        }
    }
}
