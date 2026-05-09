#include "MidiLearnManager.h"

//==============================================================================
void MidiLearnManager::startLearning (const juce::String& paramId)
{
    const juce::ScopedLock sl (lock);
    learningParamId = paramId;
    pendingMappingReady = false;
}

void MidiLearnManager::stopLearning()
{
    const juce::ScopedLock sl (lock);
    learningParamId.clear();
}

bool MidiLearnManager::isLearning() const
{
    const juce::ScopedLock sl (lock);
    return learningParamId.isNotEmpty();
}

juce::String MidiLearnManager::getLearningParamId() const
{
    const juce::ScopedLock sl (lock);
    return learningParamId;
}

int MidiLearnManager::getMappedCC (const juce::String& paramId) const
{
    const juce::ScopedLock sl (lock);
    auto it = paramToCC.find (paramId);
    return (it != paramToCC.end()) ? it->second : -1;
}

void MidiLearnManager::clearMapping (const juce::String& paramId)
{
    const juce::ScopedLock sl (lock);
    auto it = paramToCC.find (paramId);
    if (it != paramToCC.end())
    {
        ccToParam.erase (it->second);
        paramToCC.erase (it);
    }
}

void MidiLearnManager::clearAllMappings()
{
    const juce::ScopedLock sl (lock);
    paramToCC.clear();
    ccToParam.clear();
}

bool MidiLearnManager::pollNewMapping (juce::String& outParamId, int& outCC)
{
    const juce::ScopedLock sl (lock);
    if (!pendingMappingReady) return false;
    outParamId = pendingParamId;
    outCC = pendingCC;
    pendingMappingReady = false;
    return true;
}

//==============================================================================
bool MidiLearnManager::handleCC (int ccNumber, int ccValue,
                                  juce::AudioProcessorValueTreeState& apvts)
{
    juce::String mappedParam;
    bool justLearned = false;

    {
        // ScopedTryLock: never block the audio thread
        const juce::ScopedTryLock stl (lock);
        if (!stl.isLocked()) return false;

        if (learningParamId.isNotEmpty())
        {
            // Remove any existing mapping that would conflict
            auto existingParamIt = ccToParam.find (ccNumber);
            if (existingParamIt != ccToParam.end())
                paramToCC.erase (existingParamIt->second);

            auto existingCCIt = paramToCC.find (learningParamId);
            if (existingCCIt != paramToCC.end())
                ccToParam.erase (existingCCIt->second);

            // Store new mapping
            paramToCC[learningParamId] = ccNumber;
            ccToParam[ccNumber]        = learningParamId;

            // Signal to message thread
            pendingParamId      = learningParamId;
            pendingCC           = ccNumber;
            pendingMappingReady = true;

            learningParamId.clear();
            justLearned = true;
        }
        else
        {
            auto it = ccToParam.find (ccNumber);
            if (it != ccToParam.end())
                mappedParam = it->second;
        }
    }

    // Apply the CC value to the mapped parameter (outside lock, audio thread safe)
    if (!justLearned && mappedParam.isNotEmpty())
    {
        if (auto* param = apvts.getParameter (mappedParam))
        {
            const float normalized = static_cast<float>(ccValue) / 127.0f;
            // convertFrom0to1 maps the normalized CC range to the param's actual range
            if (auto* raw = apvts.getRawParameterValue (mappedParam))
                raw->store (param->convertFrom0to1 (normalized));
        }
    }

    return justLearned;
}

//==============================================================================
void MidiLearnManager::saveToValueTree (juce::ValueTree& tree) const
{
    const juce::ScopedLock sl (lock);

    // Remove old node if present to avoid duplicates
    if (auto old = tree.getChildWithName ("MidiLearn"); old.isValid())
        tree.removeChild (old, nullptr);

    juce::ValueTree midiNode ("MidiLearn");
    for (const auto& [paramId, cc] : paramToCC)
    {
        juce::ValueTree mapping ("Mapping");
        mapping.setProperty ("paramId", paramId, nullptr);
        mapping.setProperty ("cc", cc, nullptr);
        midiNode.addChild (mapping, -1, nullptr);
    }
    tree.addChild (midiNode, -1, nullptr);
}

void MidiLearnManager::loadFromValueTree (const juce::ValueTree& tree)
{
    const juce::ScopedLock sl (lock);
    paramToCC.clear();
    ccToParam.clear();

    const auto midiNode = tree.getChildWithName ("MidiLearn");
    if (!midiNode.isValid()) return;

    for (int i = 0; i < midiNode.getNumChildren(); ++i)
    {
        const auto child  = midiNode.getChild (i);
        const juce::String paramId = child.getProperty ("paramId", "");
        const int cc = static_cast<int> (child.getProperty ("cc", -1));

        if (paramId.isNotEmpty() && cc >= 0 && cc <= 127)
        {
            paramToCC[paramId] = cc;
            ccToParam[cc]      = paramId;
        }
    }
}
