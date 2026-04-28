#include "ReverbEffect.h"

ReverbEffect::ReverbEffect()
{
    // Initialize with default reverb parameters
    reverbParams.roomSize = 0.5f;
    reverbParams.damping = 0.5f;
    reverbParams.wetLevel = 0.3f;
    reverbParams.dryLevel = 0.7f;
    reverbParams.width = 1.0f;
    reverbParams.freezeMode = 0.0f;
    
    reverb.setParameters(reverbParams);
}

ReverbEffect::~ReverbEffect()
{
}

void ReverbEffect::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    // juce::Reverb doesn't have a prepare method with ProcessSpec
    // It automatically handles sample rate changes internally
    reverb.setSampleRate(sampleRate);
    reverb.reset();
}

void ReverbEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    // juce::Reverb works differently - it processes stereo samples
    if (buffer.getNumChannels() >= 2)
    {
        float* leftChannel = buffer.getWritePointer(0);
        float* rightChannel = buffer.getWritePointer(1);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            reverb.processStereo(leftChannel + sample, rightChannel + sample, 1);
        }
    }
    else if (buffer.getNumChannels() == 1)
    {
        float* monoChannel = buffer.getWritePointer(0);
        
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float left = monoChannel[sample];
            float right = left;
            reverb.processStereo(&left, &right, 1);
            monoChannel[sample] = (left + right) * 0.5f;
        }
    }
}

void ReverbEffect::setRoomSize(float roomSize)
{
    reverbParams.roomSize = juce::jlimit(0.0f, 1.0f, roomSize);
    reverb.setParameters(reverbParams);
}

void ReverbEffect::setDamping(float damping)
{
    reverbParams.damping = juce::jlimit(0.0f, 1.0f, damping);
    reverb.setParameters(reverbParams);
}

void ReverbEffect::setWetLevel(float wetLevel)
{
    reverbParams.wetLevel = juce::jlimit(0.0f, 1.0f, wetLevel);
    reverb.setParameters(reverbParams);
}

void ReverbEffect::setDryLevel(float dryLevel)
{
    reverbParams.dryLevel = juce::jlimit(0.0f, 1.0f, dryLevel);
    reverb.setParameters(reverbParams);
}

void ReverbEffect::setWidth(float width)
{
    reverbParams.width = juce::jlimit(0.0f, 1.0f, width);
    reverb.setParameters(reverbParams);
}
