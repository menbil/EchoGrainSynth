#pragma once

#include <JuceHeader.h>
#include "GrainEngine.h"
#include "Effects/ReverbEffect.h"
#include "Effects/FormantFilter.h"
#include "Effects/GlitchEffect.h"

// Forward declaration
class EchoGrainSynthAudioProcessorEditor;

//==============================================================================
class EchoGrainSynthAudioProcessor : public juce::AudioProcessor
{
public:
    //==============================================================================
    EchoGrainSynthAudioProcessor();
    ~EchoGrainSynthAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==============================================================================
    // Granular synth specific methods
    void loadSample(const juce::File& file);
    void setSampleName(const juce::String& name) { sampleName = name; }
    juce::String getSampleName() const { return sampleName; }
    
    // Parameter management
    juce::AudioProcessorValueTreeState& getValueTreeState() { return apvts; }

    void setSelectedPresetName(const juce::String& presetName) { selectedPresetName = presetName; }
    juce::String getSelectedPresetName() const { return selectedPresetName; }
    bool wasStateRestoredFromProject() const { return stateRestoredFromProject; }
    void setXYMidiLinkEnabled(bool enabled) { xyMidiLinkEnabled = enabled; }
    bool isXYMidiLinkEnabled() const { return xyMidiLinkEnabled; }
    
    // Removed Foleys GUI Magic - migrated to React-JUCE
    
    // Sample loading
    
    // Access to engine and effects for UI
    GrainEngine* getGrainEngine() { return grainEngine.get(); }
    ReverbEffect& getReverbEffect() { return reverbEffect; }
    FormantFilter& getFormantFilter() { return formantFilter; }
    GlitchEffect& getGlitchEffect() { return glitchEffect; }
    
    // Helper method for UI components
    int getActiveGrainCount() const { return grainEngine ? grainEngine->getActiveGrains() : 0; }
    
    // Access to loaded sample for visualization
    const juce::AudioBuffer<float>& getLoadedSample() const { return loadedSample; }
    bool hasSampleLoaded() const { return loadedSample.getNumSamples() > 0; }
    juce::File getLoadedSampleFile() const { return juce::File(currentSampleData.originalPath); }
    
    // Audio input recording for real-time sampling
    void startRecording();
    void stopRecording();
    bool isRecording = false;  // Public access for UI
    
    // Tempo sync
    void updateHostInfo();
    bool isTempoSyncEnabled() const { return tempoSyncEnabled; }
    void setTempoSync(bool enabled) { tempoSyncEnabled = enabled; }
    
    // Sidechain detection
    bool hasSidechainInput() const;
    void processSidechain(const juce::AudioBuffer<float>& sidechainBuffer);
    void recordAudio(const juce::AudioBuffer<float>& buffer);
    
    // MIDI-triggered granular synthesis
    void processMidiTriggeredGrains(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages);
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }
    
private:
    //==============================================================================
    juce::Synthesiser synthesiser;
    std::unique_ptr<GrainEngine> grainEngine;
    
    // Effects
    ReverbEffect reverbEffect;
    FormantFilter formantFilter;
    GlitchEffect glitchEffect;
    
    // Targeted smoothing for click-prone effect mix controls.
    juce::LinearSmoothedValue<float> smoothedReverbWet;
    juce::LinearSmoothedValue<float> smoothedFormantMix;
    
    // Parameter management
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Sample management
    juce::AudioFormatManager formatManager;
    juce::String sampleName;
    juce::AudioBuffer<float> loadedSample;
    
    // LFO system
    struct LFO
    {
        float phase = 0.0f;
        float frequency = 1.0f;
        float depth = 0.0f;
        
        float getNextValue(float sampleRate)
        {
            float value = std::sin(phase * 2.0f * juce::MathConstants<float>::pi);
            phase += frequency / sampleRate;
            if (phase >= 1.0f) phase -= 1.0f;
            return value * depth;
        }
    };
    
    LFO positionLFO, pitchLFO, densityLFO;
    
    // Audio input recording
    juce::AudioBuffer<float> recordingBuffer;
    int recordingSamplePosition = 0;
    
    // Sidechain processing
    float lastSidechainLevel = 0.0f;
    double currentBPM = 120.0;
    bool isPlaying = false;
    
    // Tempo sync
    bool tempoSyncEnabled = false;
    double hostSampleRate = 44100.0;
    juce::AudioPlayHead::CurrentPositionInfo hostInfo;

    // UI keyboard state to allow note playing directly from the plugin editor.
    juce::MidiKeyboardState keyboardState;
    
    // Sidechain processing (removed duplicate)
    juce::AudioBuffer<float> sidechainBuffer;
    bool hasSidechain = false;
    
    // Sample data persistence
    struct SampleData
    {
        juce::MemoryBlock audioData;
        double sampleRate = 44100.0;
        int numChannels = 1;
        int numSamples = 0;
        juce::String originalPath;
        
        void clear()
        {
            audioData.reset();
            numSamples = 0;
            originalPath.clear();
        }
        
        bool isEmpty() const { return numSamples == 0; }
    };
    
    SampleData currentSampleData;
    juce::String selectedPresetName { "Init Empty" };
    bool xyMidiLinkEnabled = true;
    bool stateRestoredFromProject = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoGrainSynthAudioProcessor)
};
