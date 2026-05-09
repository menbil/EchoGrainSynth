#include "FormantFilter.h"
#include <cmath>

//==============================================================================
FormantFilter::FormantFilter() = default;

void FormantFilter::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;

    // Reset all filter states
    for (auto& ch : filters)
        for (auto& bq : ch)
            bq.reset();

    updateCoefficients();
}

//==============================================================================
// setCoeffs: Audio EQ Cookbook BPF (constant 0 dB peak gain)
//   w0    = 2π*f/Fs
//   alpha = sin(w0) / (2*Q)
//   b0    =  alpha / (1+alpha)
//   b2    = -alpha / (1+alpha)
//   a1    = -2*cos(w0) / (1+alpha)
//   a2    = (1-alpha) / (1+alpha)
void FormantFilter::setCoeffs(Biquad& bq, float freqHz, float q) noexcept
{
    freqHz = juce::jlimit(20.f, (float)(sr * 0.49), freqHz);
    q      = juce::jmax(0.1f, q);

    const float w0    = juce::MathConstants<float>::twoPi * freqHz / (float)sr;
    const float cosW0 = std::cos(w0);
    const float sinW0 = std::sin(w0);
    const float alpha = sinW0 / (2.0f * q);
    const float a0inv = 1.0f / (1.0f + alpha);

    bq.b0 =  alpha * a0inv;
    bq.b2 = -alpha * a0inv;
    bq.a1 = -2.0f * cosW0 * a0inv;
    bq.a2 = (1.0f - alpha) * a0inv;
}

void FormantFilter::updateCoefficients() noexcept
{
    // Frequency scale: ratio of requested F1 to the vowel's default F1
    const float defaultF1  = kFreqs[vowel][0];
    const float freqScale  = (defaultF1 > 0.f) ? (f1Hz / defaultF1) : 1.0f;

    for (int f = 0; f < kFormants; ++f)
    {
        const float freq = juce::jlimit(20.f, 20000.f, kFreqs[vowel][f] * freqScale);
        const float q    = kQBase[f] * qScale;

        // Apply to both channels (same coefficients, independent state)
        for (int ch = 0; ch < kChannels; ++ch)
            setCoeffs(filters[ch][f], freq, q);
    }
}

//==============================================================================
void FormantFilter::setFormantFrequency(float f1)
{
    f1Hz = juce::jlimit(100.f, 5000.f, f1);
    updateCoefficients();
}

void FormantFilter::setBandwidth(float bandwidthHz)
{
    // Approximate Q from F1 bandwidth: Q ≈ F1/BW
    const float approxQ1 = juce::jmax(100.f, f1Hz) / juce::jmax(1.f, bandwidthHz);
    // Store as a scale relative to the default Q (kQBase[0]=10)
    qScale = juce::jlimit(0.2f, 10.f, approxQ1 / 10.f);
    updateCoefficients();
}

void FormantFilter::setGain(float g)
{
    gain = juce::jlimit(0.0f, 5.0f, g);
}

void FormantFilter::setDryWetMix(float mix)
{
    dryWetMix = juce::jlimit(0.0f, 1.0f, mix);
}

void FormantFilter::setVowel(int vowelType)
{
    if (vowelType >= 0 && vowelType < 5)
    {
        vowel = vowelType;
        updateCoefficients();
    }
}

//==============================================================================
void FormantFilter::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (dryWetMix < 0.001f) return;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin(buffer.getNumChannels(), kChannels);

    // Normalisation: 5 parallel filters, scale output so unity gain at each peak
    // sums to a reasonable level without boosting excessively
    const float wetScale = gain / (float)kFormants;

    for (int ch = 0; ch < numCh; ++ch)
    {
        float* data = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float x = data[i];

            // Sum 5 parallel BPFs
            float wet = 0.f;
            for (int f = 0; f < kFormants; ++f)
                wet += filters[ch][f].process(x);

            wet *= wetScale;

            data[i] = x * (1.f - dryWetMix) + wet * dryWetMix;
        }
    }
}


