#pragma once

#include <JuceHeader.h>
#include <array>

//==============================================================================
/**
 * 5-band parallel biquad bandpass formant filter (vowels A/E/I/O/U).
 *
 * Replaces the previous 2-band IIR implementation. Uses 5 resonant BPF bands
 * (F1-F5) per channel, matching human vocal tract acoustics.
 * Coefficients computed via Audio EQ Cookbook (BPF peak = unity gain).
 *
 * Same public API as before.
 */
class FormantFilter
{
public:
    FormantFilter();
    ~FormantFilter() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);

    // setFormantFrequency: sets F1 in Hz; F2-F5 scale proportionally from vowel ratios.
    // Range: 150..3200 Hz (matches APVTS "formantFreq" parameter).
    void setFormantFrequency(float f1Hz);

    void setBandwidth(float bandwidthHz);  // approximate F1 bandwidth in Hz (→ Q)
    void setGain(float gain);              // output gain (default 1.0)
    void setDryWetMix(float mix);          // 0=dry, 1=wet
    void setVowel(int vowelType);          // 0=A, 1=E, 2=I, 3=O, 4=U

private:
    static constexpr int kFormants = 5;
    static constexpr int kChannels = 2;

    // [vowel][formant]: F1..F5 centre frequencies (Hz)
    // Source: Peterson & Barney (1952), averaged across speakers
    static constexpr float kFreqs[5][5] = {
        {  800.f, 1200.f, 2500.f, 3500.f, 4500.f },  // A  /a/
        {  400.f, 2000.f, 2600.f, 3500.f, 4500.f },  // E  /e/
        {  300.f, 2800.f, 3200.f, 3600.f, 4500.f },  // I  /i/
        {  450.f,  800.f, 2500.f, 3500.f, 4500.f },  // O  /o/
        {  350.f,  600.f, 2400.f, 3500.f, 4500.f },  // U  /u/
    };

    // [formant]: base Q factors (narrower at higher formants = perceptually appropriate)
    static constexpr float kQBase[5] = { 10.f, 14.f, 18.f, 22.f, 28.f };

    // 2nd-order BPF (Direct Form II transposed, b1=0 for bandpass)
    struct Biquad
    {
        float b0 = 0.f, b2 = 0.f;
        float a1 = 0.f, a2 = 0.f;
        float s1 = 0.f, s2 = 0.f;  // state

        float process(float x) noexcept
        {
            const float y = b0 * x + s1;
            s1 = -a1 * y + s2;   // b1 = 0 → no b1*x term
            s2 =  b2 * x - a2 * y;
            return y;
        }

        void reset() noexcept { s1 = s2 = 0.f; }
    };

    // [channel][formant]
    std::array<std::array<Biquad, kFormants>, kChannels> filters;

    double sr          = 44100.0;
    int    vowel       = 0;        // 0=A..4=U
    float  f1Hz        = 800.f;   // current F1 (set via setFormantFrequency)
    float  qScale      = 1.0f;    // Q multiplier (set via setBandwidth)
    float  gain        = 1.0f;
    float  dryWetMix   = 0.5f;

    void updateCoefficients() noexcept;
    void setCoeffs(Biquad& bq, float freqHz, float q) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FormantFilter)
};
