#pragma once

#include <JuceHeader.h>

/**
 * Manages MIDI CC ↔ APVTS parameter mappings with full persistence.
 *
 * Thread safety:
 *   - Learning state is set/cleared from the message thread.
 *   - handleCC() is called from the audio thread (uses ScopedTryLock).
 *   - CC values are applied directly to atomic raw parameter floats (audio thread safe).
 *   - pollNewMapping() is called from the timer (message thread) to get UI feedback.
 */
class MidiLearnManager
{
public:
    MidiLearnManager() = default;

    //==========================================================================
    // Message-thread API

    /** Enter learn mode: the next CC received maps to this paramId. */
    void startLearning (const juce::String& paramId);

    /** Cancel learn mode without storing a mapping. */
    void stopLearning();

    /** True while waiting for an incoming CC. */
    bool isLearning() const;

    /** The paramId currently being learned, or empty string when idle. */
    juce::String getLearningParamId() const;

    /** Returns the CC number mapped to paramId, or -1 if none. */
    int getMappedCC (const juce::String& paramId) const;

    /** Removes the mapping for this paramId (both directions). */
    void clearMapping (const juce::String& paramId);

    /** Clears every mapping. */
    void clearAllMappings();

    /**
     * Poll for a just-learned mapping (call from timer / message thread).
     * Returns true once per new mapping and fills the out params.
     */
    bool pollNewMapping (juce::String& outParamId, int& outCC);

    //==========================================================================
    // Audio-thread API

    /**
     * Call once per CC MIDI message inside processBlock.
     * - If in learn mode, stores the CC → param mapping and exits learn mode.
     * - Otherwise, applies the CC value (0-127) to the mapped APVTS parameter.
     * Returns true if a new mapping was just stored.
     */
    bool handleCC (int ccNumber, int ccValue,
                   juce::AudioProcessorValueTreeState& apvts);

    //==========================================================================
    // Persistence

    /** Appends a <MidiLearn> child node to tree containing all mappings. */
    void saveToValueTree   (juce::ValueTree& tree) const;

    /** Restores mappings from a <MidiLearn> child node of tree. */
    void loadFromValueTree (const juce::ValueTree& tree);

private:
    mutable juce::CriticalSection lock;

    std::unordered_map<juce::String, int> paramToCC;  // paramId → CC number
    std::unordered_map<int, juce::String> ccToParam;  // CC number → paramId
    juce::String learningParamId;                      // empty = idle

    // Written under lock from audio thread, read under lock from message thread
    bool         pendingMappingReady = false;
    juce::String pendingParamId;
    int          pendingCC = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiLearnManager)
};
