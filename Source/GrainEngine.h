#pragma once

#include <JuceHeader.h>
#include <vector>
#include <random>
#include <array>

struct Grain
{
    bool isActive = false;
    float* sampleData = nullptr;
    int sampleLength = 0;
    int position = 0;
    int startPos = 0;
    int grainSize = 0;
    float amplitude = 1.0f;
    float pan = 0.0f;
    float pitch = 1.0f;
    int age = 0;
    float envelope = 0.0f;
    bool reverse = false;
    
    // ADSR state
    enum ADSRPhase { Attack, Decay, Sustain, Release, Inactive };
    ADSRPhase adsrPhase = Inactive;
    float adsrValue = 0.0f;
    int adsrSamples = 0;
    
    // MIDI info
    int midiNote = -1;
    float velocity = 1.0f;
    bool isNoteOn = false;
    
    void reset()
    {
        isActive = false;
        position = 0;
        age = 0;
        envelope = 0.0f;
        adsrPhase = Inactive;
        adsrValue = 0.0f;
        adsrSamples = 0;
        midiNote = -1;
        velocity = 1.0f;
        isNoteOn = false;
    }
};

struct GrainVisualizationPoint
{
    int index = -1;
    float progress = 0.0f;  // 0..1 life progression in current grain
    float panNorm = 0.5f;   // 0..1 from left to right
    float energy = 0.0f;    // 0..1 envelope/velocity proxy
    bool reverse = false;
};

class GrainEngine
{
public:
    static constexpr int maxGrains = 64;
    GrainEngine();
    ~GrainEngine();
    // Accès lecture seule aux grains pour la visualisation
    const std::array<Grain, maxGrains>& getGrains() const { return grains; }
    
    void prepareToPlay(double sampleRate, int samplesPerBlock);
    void processBlock(juce::AudioBuffer<float>& buffer, int numSamples);
    void setSample(const juce::AudioBuffer<float>& newSample);
    
    // Grain parameters
    void setGrainSize(float size) { grainSize = juce::jlimit(10.0f, 600.0f, size); }
    void setGrainDensity(float density) { grainDensity = juce::jlimit(0.1f, 30.0f, density); }
    void setPosition(float pos) { playbackPosition = juce::jlimit(0.0f, 1.0f, pos); }
    void setPositionSpread(float spread) { positionSpread = juce::jlimit(0.0f, 1.0f, spread); }
    void setPitch(float pitchRatio) { pitch = juce::jlimit(0.1f, 4.0f, pitchRatio); }
    void setPitchSpread(float spread) { pitchSpread = juce::jlimit(0.0f, 12.0f, spread); }
    void setReverse(float reverseAmount) { reverseChance = juce::jlimit(0.0f, 1.0f, reverseAmount); }
    void setPan(float panAmount) { pan = juce::jlimit(-1.0f, 1.0f, panAmount); }
    void setPanSpread(float spread) { panSpread = juce::jlimit(0.0f, 1.0f, spread); }
    void setMaxActiveGrains(int maxCount) { maxActiveGrains = juce::jlimit(4, maxGrains, maxCount); }
    void setEcoMode(bool shouldUseEcoMode) { ecoMode = shouldUseEcoMode; }
    void setSampleRange(float startNorm, float endNorm)
    {
        sampleRangeStart = juce::jlimit(0.0f, 1.0f, startNorm);
        sampleRangeEnd   = juce::jlimit(sampleRangeStart + 0.001f, 1.0f, endNorm);
    }
    float getSampleRangeStart() const { return sampleRangeStart; }
    float getSampleRangeEnd()   const { return sampleRangeEnd; }
    
    // ADSR Envelope (replacing simple attack/release)
    void setADSRAttack(float attack) { adsrAttack = juce::jlimit(0.0f, 600.0f, attack); }
    void setADSRDecay(float decay) { adsrDecay = juce::jlimit(0.0f, 600.0f, decay); }
    void setADSRSustain(float sustain) { adsrSustain = juce::jlimit(0.0f, 1.0f, sustain); }
    void setADSRRelease(float release) { adsrRelease = juce::jlimit(0.0f, 600.0f, release); }
    
    // MIDI Sampler support
    void setMIDIPitchRatio(float ratio) { midiPitchRatio = ratio; }
    void setPitchBend(float bendSemitones) { pitchBendSemitones = bendSemitones; }
    void setMIDITriggered(bool triggered) { midiTriggered = triggered; }
    void triggerGrainForNote(int noteNumber, float velocity);
    void releaseNote(int noteNumber);
    
    // Trigger grain manually
    void triggerGrain();
    
    // Get current parameters
    float getGrainSize() const { return grainSize; }
    float getGrainDensity() const { return grainDensity; }
    float getPosition() const { return playbackPosition; }
    int getActiveGrains() const { return activeGrains; }
    std::vector<GrainVisualizationPoint> getVisualizationPoints() const;
    
    // Sample info
    bool hasSample() const { return sampleBuffer.getNumSamples() > 0; }
    float getSampleLength() const { return hasSample() ? static_cast<float>(sampleBuffer.getNumSamples()) / static_cast<float>(sampleRate) : 0.0f; }
    
private:
    // (plus de redéfinition ici)
    std::array<Grain, maxGrains> grains;
    int activeGrains = 0;
    int maxActiveGrains = maxGrains;
    bool ecoMode = false;
    
    float sampleRangeStart = 0.0f;
    float sampleRangeEnd   = 1.0f;

    juce::AudioBuffer<float> sampleBuffer;
    double sampleRate = 44100.0;
    
    // Grain parameters
    float grainSize = 100.0f;        // ms
    float grainDensity = 10.0f;      // grains per second
    float playbackPosition = 0.0f;   // 0-1
    float positionSpread = 0.1f;     // 0-1
    float pitch = 1.0f;              // pitch ratio
    float pitchSpread = 0.0f;        // semitones
    float reverseChance = 0.0f;      // 0-1
    float pan = 0.0f;                // -1 to 1
    float panSpread = 0.0f;          // 0-1
    
    // ADSR Envelope parameters
    float adsrAttack = 10.0f;        // ms
    float adsrDecay = 100.0f;        // ms
    float adsrSustain = 0.7f;        // 0-1
    float adsrRelease = 100.0f;      // ms
    
    // MIDI Sampler parameters
    float midiPitchRatio = 1.0f;     // Current MIDI pitch ratio
    float pitchBendSemitones = 0.0f; // Current pitch bend
    bool midiTriggered = false;      // Whether grains are triggered by MIDI only
    
    struct ActiveNote
    {
        int noteNumber = -1;
        float velocity = 1.0f;
        std::vector<int> grainIndices; // Grains associated with this note
    };
    std::vector<ActiveNote> activeNotes;
    
    // Internal timing
    float grainTimer = 0.0f;
    float samplesPerGrain = 0.0f;
    
    // Random generator
    std::random_device rd;
    std::mt19937 gen;
    std::uniform_real_distribution<float> uniformDist;
    
    // Private methods
    void spawnGrain();
    void spawnGrainForNote(int noteNumber, float velocity); // NEW: MIDI-triggered grain
    float calculateADSREnvelope(Grain& grain) const;        // NEW: ADSR calculation
    float calculateEnvelope(const Grain& grain) const;      // Legacy envelope (keep for compatibility)
    void updateGrainTimer(int numSamples);
    void updateADSRPhase(Grain& grain) const;               // NEW: ADSR phase management
    int findInactiveGrain();
};
