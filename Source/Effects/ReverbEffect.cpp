#include "ReverbEffect.h"
#include <cmath>

// Base delay times (ms) – loosely prime-spaced for low modal density
static constexpr float kBaseMs[8] = {
    29.3f, 37.1f, 41.2f, 53.5f, 59.7f, 67.3f, 71.9f, 83.1f
};

// Per-line modulation rates (Hz) and depths (samples) – break flutter/metallic coloration
static constexpr float kModRateHz[8]   = { 0.17f, 0.23f, 0.31f, 0.41f, 0.19f, 0.29f, 0.37f, 0.43f };
static constexpr float kModDepthSmp[8] = {  1.5f,  1.2f,  1.8f,  1.3f,  1.6f,  1.1f,  1.9f,  1.4f };

// Average base delay (ms) – used for T60 → feedback calculation
static constexpr float kAvgBaseMs = (29.3f + 37.1f + 41.2f + 53.5f + 59.7f + 67.3f + 71.9f + 83.1f) / 8.0f;

//==============================================================================
ReverbEffect::ReverbEffect()
{
    // Stagger initial modulation phases so lines start decorrelated
    for (int j = 0; j < kLines; ++j)
        lines[j].modPh = (float)j / (float)kLines;
}

void ReverbEffect::prepare(double sampleRate, int /*samplesPerBlock*/)
{
    sr = sampleRate;

    // Allocate delay buffers: 200 ms + headroom for room size scaling + modulation
    const int maxSamples = (int)(0.215 * sr) + 8;
    for (auto& line : lines)
    {
        line.buf.assign(maxSamples, 0.f);
        line.wp    = 0;
        line.lpfSt = 0.f;
    }

    updateDelayLengths();
    updateCoeffs();
}

//==============================================================================
void ReverbEffect::setRoomSize(float r) { roomSize = juce::jlimit(0.f, 1.f, r); updateDelayLengths(); updateCoeffs(); }
void ReverbEffect::setDamping  (float d) { damping  = juce::jlimit(0.f, 1.f, d); updateCoeffs(); }
void ReverbEffect::setWetLevel (float w) { wetLevel = juce::jlimit(0.f, 1.f, w); }
void ReverbEffect::setDryLevel (float d) { dryLevel = juce::jlimit(0.f, 1.f, d); }
void ReverbEffect::setWidth    (float w) { width    = juce::jlimit(0.f, 1.f, w); }

//==============================================================================
void ReverbEffect::updateDelayLengths() noexcept
{
    // Room size scales delay: 0→×0.5 (small), 0.5→×1.0 (medium), 1→×1.5 (hall)
    const float scale = 0.5f + roomSize;
    for (int j = 0; j < kLines; ++j)
    {
        const int desired = (int)(kBaseMs[j] * 0.001f * scale * (float)sr);
        lines[j].len = juce::jlimit(1, (int)lines[j].buf.size() - 4, desired);
    }
}

void ReverbEffect::updateCoeffs() noexcept
{
    // T60: 0.3 s (roomSize=0, dead space) → 5.0 s (roomSize=1, large hall)
    const float t60 = 0.3f + roomSize * 4.7f;

    // Mean physical delay scales with room size
    const float avgDelayS = kAvgBaseMs * 0.001f * (0.5f + roomSize);

    // feedback = 10^(-3 * avgDelay / T60)   [standard sabine-derived formula]
    fbGain = std::pow(10.0f, -3.0f * avgDelayS / t60);
    fbGain = juce::jlimit(0.0f, 0.975f, fbGain);

    // LPF cutoff: damping=0 → 18 kHz (bright/transparent), damping=1 → 800 Hz (very dark)
    const float cutHz = 800.0f + (1.0f - damping) * 17200.0f;
    const float w0    = juce::MathConstants<float>::twoPi * cutHz / (float)sr;
    lpfAlpha = 1.0f - std::exp(-w0);
    lpfAlpha = juce::jlimit(0.01f, 1.0f, lpfAlpha);
}

//==============================================================================
// 8-point normalized Walsh-Hadamard transform (in-place butterfly, O(N log N))
void ReverbEffect::hadamard8(std::array<float, kLines>& v) noexcept
{
    // Butterfly stages
    for (int stride = 1; stride < kLines; stride <<= 1)
    {
        for (int i = 0; i < kLines; i += stride << 1)
        {
            for (int j = 0; j < stride; ++j)
            {
                const float a = v[i + j];
                const float b = v[i + j + stride];
                v[i + j]         = a + b;
                v[i + j + stride] = a - b;
            }
        }
    }
    // Normalize: 1/sqrt(8) preserves energy (unitary → stable feedback)
    const float norm = 1.0f / std::sqrt((float)kLines);
    for (auto& x : v) x *= norm;
}

//==============================================================================
void ReverbEffect::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (wetLevel < 0.0001f) return;

    const int numSamples = buffer.getNumSamples();
    const int numCh      = juce::jmin(buffer.getNumChannels(), 2);

    // Input injection scale: equal-energy injection across all lines
    const float injScale = 1.0f / std::sqrt((float)kLines);

    for (int i = 0; i < numSamples; ++i)
    {
        const float inL = (numCh > 0) ? buffer.getSample(0, i) : 0.f;
        const float inR = (numCh > 1) ? buffer.getSample(1, i) : inL;
        const float monoIn = (inL + inR) * 0.5f;

        // --- Read from each delay line with fractional (linear-interp) modulation ---
        std::array<float, kLines> v;
        for (int j = 0; j < kLines; ++j)
        {
            auto& line = lines[j];
            const int size = (int)line.buf.size();

            // Sine modulation: ±kModDepthSmp[j] samples
            const float mod       = kModDepthSmp[j] * std::sin(line.modPh * juce::MathConstants<float>::twoPi);
            const float floatRead = (float)(line.wp - line.len) - mod;

            // Linear interpolation for sub-sample accuracy
            int r0 = (int)floatRead;
            const float frac = floatRead - (float)r0;
            r0 = ((r0 % size) + size) % size;
            const int r1 = (r0 + 1) % size;
            v[j] = line.buf[r0] * (1.f - frac) + line.buf[r1] * frac;

            // Advance modulation phase
            line.modPh += kModRateHz[j] / (float)sr;
            if (line.modPh >= 1.0f) line.modPh -= 1.0f;
        }

        // --- Hadamard mix: decorrelates the 8 delay-line signals ---
        hadamard8(v);

        // --- Damping LPF + feedback write ---
        for (int j = 0; j < kLines; ++j)
        {
            auto& line = lines[j];
            // One-pole LPF: y += α*(x−y)
            line.lpfSt += lpfAlpha * (v[j] - line.lpfSt);
            line.buf[line.wp] = monoIn * injScale + fbGain * line.lpfSt;
            if (++line.wp >= (int)line.buf.size()) line.wp = 0;
        }

        // --- Stereo decorrelated output: lines 0-3 → L, 4-7 → R ---
        float outL = 0.f, outR = 0.f;
        for (int j = 0;            j < kLines / 2; ++j) outL += v[j];
        for (int j = kLines / 2;   j < kLines;     ++j) outR += v[j];

        // Normalize and apply stereo width
        const float norm = 2.0f / (float)kLines;
        outL *= norm;
        outR *= norm;

        const float mid  = (outL + outR) * 0.5f;
        const float side = (outL - outR) * 0.5f * width;
        outL = mid + side;
        outR = mid - side;

        // --- Dry/wet mix ---
        if (numCh > 0) buffer.setSample(0, i, dryLevel * inL + wetLevel * outL);
        if (numCh > 1) buffer.setSample(1, i, dryLevel * inR + wetLevel * outR);
    }
}


