#pragma once

#include <JuceHeader.h>
#include <vector>

//==============================================================================
/**
 * Stereo delay with feedback, BPM-sync subdivision support,
 * and smooth wet-level transitions (click-free).
 */
class DelayEffect
{
public:
    DelayEffect();
    ~DelayEffect() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);

    // Core parameters
    void setDelayTimeMs(float ms);
    void setFeedback(float feedback);   // 0..0.95
    void setWetLevel(float wet);        // 0..1

    // BPM sync: pass current BPM and subdivision (0=1/4, 1=1/8, 2=1/16, 3=3/8, 4=1/2)
    void setBpmSync(bool enabled, double bpm, int subdivisionIndex);

private:
    static constexpr int maxDelayMs = 2000;

    double sampleRate = 44100.0;

    std::vector<float> delayBufL, delayBufR;
    int   delayBufSize = 0;
    int   writePos     = 0;

    float delayTimeMs = 375.0f;
    float feedback    = 0.30f;
    float wetLevel    = 0.0f;

    juce::LinearSmoothedValue<float> smoothedWet;

    int getDelaySamples() const;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DelayEffect)
};
