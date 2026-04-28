#include "GlitchEffect.h"

GlitchEffect::GlitchEffect()
    : randomGenerator(std::random_device{}())
    , distribution(0.0f, 1.0f)
{
}

GlitchEffect::~GlitchEffect()
{
}

void GlitchEffect::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    
    // Prepare stutter buffer
    stutterBuffer.setSize(2, static_cast<int>(sampleRate * 0.5)); // 500ms max stutter
}

void GlitchEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (intensity <= 0.0f)
        return;
        
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    
    // Update glitch state
    glitchTimer += static_cast<float>(numSamples) / static_cast<float>(sampleRate);
    
    if (glitchTimer >= (1.0f / rate))
    {
        glitchTimer = 0.0f;
        // Use probability parameter for glitch triggering
        glitchActive = (distribution(randomGenerator) < probability) && (intensity > 0.0f);
        
        // Maybe start stuttering (only if glitch is active)
        if (glitchActive && stutterAmount > 0.0f && distribution(randomGenerator) < stutterAmount)
        {
            isStuttering = true;
            stutterLength = static_cast<int>((distribution(randomGenerator) * 0.2f + 0.05f) * static_cast<float>(sampleRate)); // 50-250ms
            stutterPosition = 0;
            
            // Capture current audio for stuttering
            int captureLength = juce::jmin(stutterLength, numSamples);
            for (int ch = 0; ch < juce::jmin(numChannels, stutterBuffer.getNumChannels()); ++ch)
            {
                stutterBuffer.copyFrom(ch, 0, buffer, ch, 0, captureLength);
            }
        }
    }
    
    const float wetAmount = juce::jlimit(0.0f, 1.0f, intensity * 0.85f);

    // Apply effects
    for (int sample = 0; sample < numSamples; ++sample)
    {
        for (int channel = 0; channel < numChannels; ++channel)
        {
            float inputSample = buffer.getSample(channel, sample);
            float outputSample = inputSample;
            
            // Stutter effect
            if (isStuttering && stutterPosition < stutterLength)
            {
                if (channel < stutterBuffer.getNumChannels())
                {
                    outputSample = stutterBuffer.getSample(channel, stutterPosition % stutterLength);
                }
                
                if (channel == 0) // Only increment on first channel
                {
                    stutterPosition++;
                    if (stutterPosition >= stutterLength)
                        isStuttering = false;
                }
            }
            
            // Bit crushing
            if (bitCrushAmount > 0.0f && glitchActive)
            {
                crushFactor = 1.0f + bitCrushAmount * 31.0f; // 1 to 32 levels
                outputSample = std::floor(outputSample * crushFactor) / crushFactor;
            }
            
            // Random dropouts and gain flicker (musical range)
            if (glitchActive)
            {
                const float randomValue = distribution(randomGenerator);

                if (randomValue < (0.01f + intensity * 0.04f))
                {
                    // Partial dropout instead of hard mute to avoid abrupt clicks.
                    outputSample *= (0.1f + distribution(randomGenerator) * 0.25f);
                }
                else if (randomValue < (0.03f + intensity * 0.08f))
                {
                    outputSample *= juce::jmap(distribution(randomGenerator), 0.0f, 1.0f, 0.55f, 1.45f);
                }
            }

            // Blend back with dry signal so the effect stays playable.
            outputSample = inputSample + (outputSample - inputSample) * wetAmount;
            outputSample = juce::jlimit(-1.0f, 1.0f, outputSample);
            
            buffer.setSample(channel, sample, outputSample);
        }
    }
}

void GlitchEffect::setIntensity(float newIntensity)
{
    intensity = juce::jlimit(0.0f, 1.0f, newIntensity);
}

void GlitchEffect::setRate(float newRate)
{
    rate = juce::jlimit(0.1f, 50.0f, newRate); // Extended range for new parameter
}

void GlitchEffect::setProbability(float newProbability)
{
    probability = juce::jlimit(0.0f, 1.0f, newProbability);
}

void GlitchEffect::setBitCrush(float newBitCrush)
{
    bitCrushAmount = juce::jlimit(0.0f, 1.0f, newBitCrush);
}

void GlitchEffect::setStutter(float newStutter)
{
    stutterAmount = juce::jlimit(0.0f, 1.0f, newStutter);
}
