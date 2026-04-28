#pragma once

#include <JuceHeader.h>
#include "GranularSound.h"

//==============================================================================
class GranularVoice : public juce::SynthesiserVoice
{
public:
    GranularVoice();
    
    bool canPlaySound(juce::SynthesiserSound* sound) override;
    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound* sound, int currentPitchWheelPosition) override;
    void stopNote(float velocity, bool allowTailOff) override;
    void pitchWheelMoved(int newPitchWheelValue) override;
    void controllerMoved(int controllerNumber, int newControllerValue) override;
    void renderNextBlock(juce::AudioBuffer<float>& outputBuffer, int startSample, int numSamples) override;
    
    bool isVoiceActive() const override;
    
    void setGrainParameters(float grainSize, float density, float position, float pitch);
    void setRootNote(float midiNoteNumber) { rootNoteNumber = midiNoteNumber; }
    
private:
    GranularSound* currentSound = nullptr;
    
    float currentMidiNote = 0.0f;
    float velocity = 0.0f;
    bool isPlaying = false;
    
    // Grain parameters
    float grainSize = 100.0f;      // ms
    float grainDensity = 10.0f;    // grains per second
    float playbackPosition = 0.0f; // 0-1 in sample
    float pitchRatio = 1.0f;
    float rootNoteNumber = 60.0f;  // C4 by default
    
    // Internal state
    float grainTimer = 0.0f;
    float sampleRate = 44100.0f;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GranularVoice)
};
