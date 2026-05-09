#pragma once

#include <JuceHeader.h>
#include <array>
#include <vector>

//==============================================================================
/**
 * 8-line Feedback Delay Network (FDN) reverb.
 *
 * Replaces the legacy juce::Reverb (Schroeder 1962) with a modern FDN:
 *   - 8 delay lines with prime-spaced lengths for low modal density.
 *   - Normalized 8×8 Walsh-Hadamard mixing matrix (unitary → stable).
 *   - One-pole LPF per line for air/wall HF absorption (damping).
 *   - Gentle per-line sine modulation to break metallic coloration.
 *   - Decorrelated stereo output (lines 0-3 → L, 4-7 → R).
 *
 * Same public API as the previous juce::Reverb wrapper.
 */
class ReverbEffect
{
public:
    ReverbEffect();
    ~ReverbEffect() = default;

    void prepare(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer);

    void setRoomSize(float roomSize);  // 0..1  → T60 0.3..5 s
    void setDamping  (float damping);  // 0..1  → HF absorption
    void setWetLevel (float wetLevel); // 0..1
    void setDryLevel (float dryLevel); // 0..1  (default 1.0)
    void setWidth    (float width);    // 0..1  stereo width

private:
    static constexpr int kLines = 8;

    struct Line
    {
        std::vector<float> buf;
        int   wp      = 0;    // write position
        int   len     = 0;    // current delay length (samples)
        float lpfSt   = 0.f;  // one-pole state
        float modPh   = 0.f;  // modulation phase [0,1)
    };

    std::array<Line, kLines> lines;

    double sr        = 44100.0;
    float  roomSize  = 0.5f;
    float  damping   = 0.5f;
    float  wetLevel  = 0.f;
    float  dryLevel  = 1.f;
    float  width     = 1.f;

    // Derived coefficients (recomputed on parameter change or prepare)
    float lpfAlpha   = 0.5f;   // one-pole α: y += α*(x-y)
    float fbGain     = 0.8f;   // loop feedback gain

    void updateCoeffs()        noexcept;
    void updateDelayLengths()  noexcept;

    // 8-point in-place normalized Walsh-Hadamard transform
    static void hadamard8(std::array<float, kLines>& v) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ReverbEffect)
};
