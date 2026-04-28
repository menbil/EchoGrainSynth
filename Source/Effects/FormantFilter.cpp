#include "FormantFilter.h"

// Approximate formant frequencies for vowels (F1, F2)
const float FormantFilter::vowelFormants[5][2] = {
    {730.0f, 1090.0f}, // A
    {270.0f, 2290.0f}, // E
    {390.0f, 1990.0f}, // I
    {570.0f, 840.0f},  // O
    {440.0f, 1020.0f}  // U
};

FormantFilter::FormantFilter()
{
}

FormantFilter::~FormantFilter()
{
}

void FormantFilter::prepare(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = static_cast<juce::uint32>(512); // Use a default block size
    spec.numChannels = 2;
    
    formantFilter1.prepare(spec);
    formantFilter2.prepare(spec);
    
    updateFilter();
}

void FormantFilter::processBlock(juce::AudioBuffer<float>& buffer)
{
    if (dryWetMix <= 0.0f)
        return;
        
    // Create a copy for wet signal processing
    juce::AudioBuffer<float> wetBuffer;
    wetBuffer.makeCopyOf(buffer);
    
    // Process with formant filters
    juce::dsp::AudioBlock<float> block(wetBuffer);
    juce::dsp::ProcessContextReplacing<float> context(block);
    
    formantFilter1.process(context);
    formantFilter2.process(context);
    
    // Mix dry and wet signals
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
        {
            float drySignal = buffer.getSample(channel, sample);
            float wetSignal = wetBuffer.getSample(channel, sample) * gain;
            
            float output = drySignal * (1.0f - dryWetMix) + wetSignal * dryWetMix;
            buffer.setSample(channel, sample, output);
        }
    }
}

void FormantFilter::setFormantFrequency(float frequency)
{
    formantFreq = juce::jlimit(100.0f, 5000.0f, frequency);
    updateFilter();
}

void FormantFilter::setBandwidth(float newBandwidth)
{
    bandwidth = juce::jlimit(10.0f, 500.0f, newBandwidth);
    updateFilter();
}

void FormantFilter::setGain(float newGain)
{
    gain = juce::jlimit(0.1f, 5.0f, newGain);
}

void FormantFilter::setDryWetMix(float mix)
{
    dryWetMix = juce::jlimit(0.0f, 1.0f, mix);
}

void FormantFilter::setVowel(int vowelType)
{
    if (vowelType >= 0 && vowelType < 5)
    {
        // Use the first formant frequency as the main frequency
        formantFreq = vowelFormants[vowelType][0];
        
        // Set bandwidth based on vowel characteristics
        bandwidth = 60.0f + static_cast<float>(vowelType) * 20.0f; // Varying bandwidth
        
        updateFilter();
    }
}

void FormantFilter::updateFilter()
{
    if (sampleRate > 0)
    {
        float Q = formantFreq / bandwidth;
        
        // Create bandpass filters for formants
        coefficients1 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, formantFreq, Q);
        coefficients2 = juce::dsp::IIR::Coefficients<float>::makeBandPass(sampleRate, formantFreq * 1.5f, Q * 0.8f);
        
        formantFilter1.coefficients = coefficients1;
        formantFilter2.coefficients = coefficients2;
    }
}
