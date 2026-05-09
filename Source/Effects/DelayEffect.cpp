#include "DelayEffect.h"

DelayEffect::DelayEffect()
{
    smoothedWet.setCurrentAndTargetValue(0.0f);
}

void DelayEffect::prepare(double sr, int /*samplesPerBlock*/)
{
    sampleRate    = sr;
    delayBufSize  = static_cast<int>(sr * maxDelayMs / 1000.0) + 2;

    delayBufL.assign(static_cast<size_t>(delayBufSize), 0.0f);
    delayBufR.assign(static_cast<size_t>(delayBufSize), 0.0f);
    writePos = 0;

    smoothedWet.reset(sr, 0.025);
    smoothedWet.setCurrentAndTargetValue(wetLevel);
}

//==============================================================================
void DelayEffect::setDelayTimeMs(float ms)
{
    delayTimeMs = juce::jlimit(1.0f, static_cast<float>(maxDelayMs), ms);
}

void DelayEffect::setFeedback(float fb)
{
    feedback = juce::jlimit(0.0f, 0.95f, fb);
}

void DelayEffect::setWetLevel(float wet)
{
    wetLevel = juce::jlimit(0.0f, 1.0f, wet);
    smoothedWet.setTargetValue(wetLevel);
}

void DelayEffect::setBpmSync(bool enabled, double bpm, int subdivisionIndex)
{
    if (!enabled || bpm <= 0.0)
        return;

    // Subdivision durations in beats: 1/16, 1/8, 1/4, 3/8, 1/2
    const float subdivisions[] = { 0.25f, 0.5f, 1.0f, 1.5f, 2.0f };
    const int numSubs = static_cast<int>(std::size(subdivisions));
    const int idx = juce::jlimit(0, numSubs - 1, subdivisionIndex);

    const float beatsPerMs  = static_cast<float>(bpm) / 60000.0f;
    const float newTimeMs   = subdivisions[idx] / beatsPerMs;
    setDelayTimeMs(newTimeMs);
}

int DelayEffect::getDelaySamples() const
{
    return juce::jlimit(1, delayBufSize - 1,
        static_cast<int>(delayTimeMs * 0.001 * sampleRate));
}

//==============================================================================
void DelayEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (delayBufSize <= 0)
        return;

    const int numSamples  = buffer.getNumSamples();
    const int numCh       = buffer.getNumChannels();
    const int delaySmp    = getDelaySamples();

    for (int i = 0; i < numSamples; ++i)
    {
        const float wet = smoothedWet.getNextValue();
        if (wet < 0.0001f && !smoothedWet.isSmoothing())
        {
            // Skip processing but keep write position advancing for seamless enable
            writePos = (writePos + 1) % delayBufSize;
            continue;
        }

        const int readPos = (writePos - delaySmp + delayBufSize) % delayBufSize;

        const float inL = numCh > 0 ? buffer.getReadPointer(0)[i] : 0.0f;
        const float inR = numCh > 1 ? buffer.getReadPointer(1)[i] : inL;

        const float delL = delayBufL[static_cast<size_t>(readPos)];
        const float delR = delayBufR[static_cast<size_t>(readPos)];

        // Write to delay buffer with feedback (soft-limit to prevent runaway)
        const float fbL = juce::jlimit(-1.0f, 1.0f, (inL + delL * feedback));
        const float fbR = juce::jlimit(-1.0f, 1.0f, (inR + delR * feedback));
        delayBufL[static_cast<size_t>(writePos)] = fbL;
        delayBufR[static_cast<size_t>(writePos)] = fbR;

        // Mix: dry stays at 1.0, wet blends in the delayed signal
        if (numCh > 0) buffer.getWritePointer(0)[i] = inL + delL * wet;
        if (numCh > 1) buffer.getWritePointer(1)[i] = inR + delR * wet;

        writePos = (writePos + 1) % delayBufSize;
    }
}
