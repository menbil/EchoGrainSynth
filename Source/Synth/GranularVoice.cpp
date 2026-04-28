#include "GranularVoice.h"

GranularVoice::GranularVoice()
{
}

bool GranularVoice::canPlaySound(juce::SynthesiserSound* sound)
{
    return dynamic_cast<GranularSound*>(sound) != nullptr;
}

void GranularVoice::startNote(int midiNoteNumber, float noteVelocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition)
{
    juce::ignoreUnused(currentPitchWheelPosition);
    currentSound = dynamic_cast<GranularSound*>(sound);
    velocity = noteVelocity;
    
    // Get current sample rate from the synthesiser
    sampleRate = static_cast<float>(getSampleRate());
    if (sampleRate <= 0.0f)
        sampleRate = 44100.0f; // fallback
    
    if (currentSound != nullptr && currentSound->hasSampleData())
    {
        currentMidiNote = static_cast<float>(midiNoteNumber);
        this->velocity = velocity;
        isPlaying = true;
        
        // Convert MIDI note to pitch ratio relative to the sample's root note
        pitchRatio = std::pow(2.0f, (currentMidiNote - rootNoteNumber) / 12.0f);
        
        grainTimer = 0.0f;
        playbackPosition = 0.0f; // Start from beginning
        
        DBG("GranularVoice started: Note " + juce::String(midiNoteNumber) + 
            " (Root: " + juce::String(rootNoteNumber) + ")" +
            ", Velocity " + juce::String(velocity) + 
            ", Pitch ratio " + juce::String(pitchRatio));
    }
}

void GranularVoice::stopNote(float noteVelocity, bool allowTailOff)
{
    juce::ignoreUnused(noteVelocity, allowTailOff);
    // Stop immediately when note is released (classic synth behavior)
    isPlaying = false;
    currentSound = nullptr;
    DBG("GranularVoice stopped");
}

void GranularVoice::pitchWheelMoved(int newPitchWheelValue)
{
    // Convert pitch wheel to pitch bend factor
    float pitchBend = (newPitchWheelValue - 8192.0f) / 8192.0f; // -1 to +1
    pitchRatio *= std::pow(2.0f, pitchBend / 12.0f); // ±1 semitone range
}

void GranularVoice::controllerMoved(int controllerNumber, int newControllerValue)
{
    // Handle MIDI CC for real-time parameter control
    float normalizedValue = newControllerValue / 127.0f;
    
    switch (controllerNumber)
    {
        case 1:  // Modulation wheel - control grain size
            grainSize = 10.0f + normalizedValue * 990.0f; // 10ms to 1000ms
            break;
        case 74: // Filter cutoff - control density
            grainDensity = 0.1f + normalizedValue * 49.9f; // 0.1 to 50 grains/sec
            break;
        case 71: // Filter resonance - control position
            playbackPosition = normalizedValue;
            break;
        default:
            break;
    }
}

void GranularVoice::renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples)
{
    if (!isPlaying || currentSound == nullptr || !currentSound->hasSampleData())
    {
        return;
    }
    
    const auto& sampleData = currentSound->getSampleData();
    if (sampleData.getNumSamples() == 0)
        return;
    
    for (int sample = startSample; sample < startSample + numSamples; ++sample)
    {
        // Calculate current sample position with pitch ratio
        int samplePos = static_cast<int>(playbackPosition * sampleData.getNumSamples());
        samplePos = juce::jlimit(0, sampleData.getNumSamples() - 1, samplePos);
        
        // Get audio sample with simple linear interpolation
        float audioSample = 0.0f;
        if (samplePos < sampleData.getNumSamples() - 1)
        {
            float frac = (playbackPosition * sampleData.getNumSamples()) - samplePos;
            float sample1 = sampleData.getSample(0, samplePos);
            float sample2 = sampleData.getSample(0, samplePos + 1);
            audioSample = sample1 + frac * (sample2 - sample1);
        }
        else
        {
            audioSample = sampleData.getSample(0, samplePos);
        }
        
        // Apply velocity and reduce overall volume
        audioSample *= velocity * 0.5f;
        
        // Add to all output channels
        for (int channel = 0; channel < outputBuffer.getNumChannels(); ++channel)
        {
            outputBuffer.addSample(channel, sample, audioSample);
        }
        
        // Advance playback position with correct pitch calculation
        // For a normalized position (0.0 to 1.0), we need to calculate how much to advance per sample
        // Higher pitchRatio = faster playback = bigger increment
        float samplesPerOutputSample = pitchRatio; // At normal pitch, advance 1 sample per output sample
        float normalizedIncrement = samplesPerOutputSample / sampleData.getNumSamples();
        playbackPosition += normalizedIncrement;
        
        // Loop the sample
        if (playbackPosition >= 1.0f)
            playbackPosition -= 1.0f;
    }
}

bool GranularVoice::isVoiceActive() const
{
    return isPlaying;
}

void GranularVoice::setGrainParameters(float newGrainSize, float newDensity, float newPosition, float newPitch)
{
    grainSize = newGrainSize;
    grainDensity = newDensity;
    playbackPosition = newPosition;
    pitchRatio = newPitch;
}
