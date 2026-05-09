#pragma once

#include <JuceHeader.h>
#include "GrainEngine.h"
#include "Effects/ReverbEffect.h"
#include "Effects/FormantFilter.h"
#include "Effects/DelayEffect.h"
#include "MidiLearnManager.h"

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
    bool isMPEEnabled() const { return true; }
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

    // A/B bypass: when enabled, outputs the loaded sample directly (no granular / effects)
    void setABBypass(bool enabled) { if (enabled && !abBypassEnabled.load()) abPlaybackPosition.store(0); abBypassEnabled.store(enabled); }
    bool isABBypassEnabled() const { return abBypassEnabled.load(); }
    
    // Removed Foleys GUI Magic - migrated to React-JUCE
    
    // Sample loading
    
    // Access to engine and effects for UI
    GrainEngine* getGrainEngine() { return grainEngine.get(); }
    ReverbEffect& getReverbEffect() { return reverbEffect; }
    FormantFilter& getFormantFilter() { return formantFilter; }
    DelayEffect& getDelayEffect() { return delayEffect; }
    
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
    
    // MIDI keyboard state for UI
    juce::MidiKeyboardState& getKeyboardState() noexcept { return keyboardState; }

    // Output peak level (for meter UI)
    float getPeakLevel() const noexcept { return outputPeakLevel.load(std::memory_order_relaxed); }

    // WAV export recording
    void startWavRecording (const juce::File& destFile);
    void stopWavRecording();
    bool isRecordingWav() const noexcept { return isWavRecording.load(); }

    MidiLearnManager& getMidiLearnManager() { return midiLearnManager; }
    
private:
    //==============================================================================
    std::unique_ptr<GrainEngine> grainEngine;
    juce::MPEInstrument       mpeInstrument;   // Per-note pitch/pressure/slide (MPE)
    
    // Effects
    ReverbEffect reverbEffect;
    FormantFilter formantFilter;
    DelayEffect  delayEffect;

    // Per-instance MIDI state (must NOT be static — each instance is independent)
    std::array<bool,  128> activeNotes {};
    std::array<float, 128> noteVelocities {}; // MIDI velocity per active note
    float currentPitchWheel = 0.0f;
    
    // Targeted smoothing for click-prone controls.
    juce::LinearSmoothedValue<float> smoothedReverbWet;
    juce::LinearSmoothedValue<float> smoothedFormantMix;
    juce::LinearSmoothedValue<float> smoothedDelayWet;
    juce::LinearSmoothedValue<float> smoothedMasterGain;
    
    // Parameter management
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    
    // Sample management
    juce::AudioFormatManager formatManager;
    juce::String sampleName;
    juce::AudioBuffer<float> loadedSample;
    
    // LFO system  (0=Sine, 1=Square, 2=Triangle, 3=S&H)
    struct LFO
    {
        float phase = 0.0f;
        float frequency = 1.0f;
        float depth = 0.0f;
        int   waveform = 0;

        // S&H state (private to struct)
        float sAndHValue = 0.0f;
        std::mt19937 rng { std::random_device{}() };
        std::uniform_real_distribution<float> dist { -1.0f, 1.0f };

        float getNextValue(float sampleRate)
        {
            float value = 0.0f;
            switch (waveform)
            {
                case 1:  // Square
                    value = phase < 0.5f ? 1.0f : -1.0f;
                    break;
                case 2:  // Triangle
                    value = phase < 0.5f ? (4.0f * phase - 1.0f) : (3.0f - 4.0f * phase);
                    break;
                case 3:  // Sample & Hold (updated at each cycle)
                    value = sAndHValue;
                    break;
                default: // Sine
                    value = std::sin(phase * juce::MathConstants<float>::twoPi);
                    break;
            }

            phase += frequency / sampleRate;
            if (phase >= 1.0f)
            {
                phase -= 1.0f;
                if (waveform == 3)
                    sAndHValue = dist(rng);
            }

            return value * depth;
        }
    };
    
    LFO positionLFO, pitchLFO, densityLFO, grainSizeLFO;
    
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
    
    // Sidechain processing
    juce::AudioBuffer<float> sidechainBuffer;
    bool hasSidechain = false;

    // MPE expression tracking (pressure and slide, updated from MIDI events)
    float mpePressure = 0.5f; // aftertouch / channel pressure (0..1, 0.5 = neutral)
    float mpeSlide    = 0.5f; // CC74 timbre / slide (0..1, 0.5 = neutral)

    // Output peak meter (written audio thread, read UI thread)
    std::atomic<float> outputPeakLevel { 0.0f };

    // Sample-swap lock: write-lock in loadSample (msg thread), read-lock in processBlock
    juce::ReadWriteLock sampleLock;
    
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

    // A/B bypass state (accessed from both UI and audio threads)
    std::atomic<bool> abBypassEnabled { false };
    std::atomic<int>  abPlaybackPosition { 0 };

    MidiLearnManager midiLearnManager;

    // WAV export recording (background thread, lock-free FIFO to writer)
    juce::TimeSliceThread                                  wavRecordThread { "WavRecord" };
    std::unique_ptr<juce::AudioFormatWriter::ThreadedWriter> wavWriter;
    std::atomic<bool>                                      isWavRecording  { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EchoGrainSynthAudioProcessor)
};
