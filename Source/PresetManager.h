#pragma once

#include <JuceHeader.h>

//==============================================================================
class PresetManager
{
public:
    struct Preset
    {
        juce::String name;
        juce::ValueTree state;
        juce::String category;
        bool favorite = false;
        
        Preset(const juce::String& presetName, const juce::ValueTree& presetState,
               const juce::String& presetCategory = "Uncategorized", bool isFavorite = false)
            : name(presetName), state(presetState), category(presetCategory), favorite(isFavorite) {}
    };
    
    PresetManager();
    ~PresetManager();
    
    void initializeDefaultPresets();
    void savePreset(const juce::String& name, const juce::ValueTree& state,
                    const juce::String& category = "Uncategorized", bool favorite = false);
    bool loadPreset(const juce::String& name, juce::ValueTree& state);
    void deletePreset(const juce::String& name);
    bool renamePreset(const juce::String& oldName, const juce::String& newName);
    bool setPresetCategory(const juce::String& name, const juce::String& category);
    bool setPresetFavorite(const juce::String& name, bool favorite);
    juce::String getPresetCategory(const juce::String& name) const;
    bool isPresetFavorite(const juce::String& name) const;
    juce::StringArray getPresetNames(const juce::String& categoryFilter, bool favoritesOnly) const;
    
    juce::StringArray getPresetNames() const;
    juce::StringArray getCategories() const;
    int getNumPresets() const { return static_cast<int>(presets.size()); }
    
    // File operations
    bool savePresetsToFile(const juce::File& file);
    bool loadPresetsFromFile(const juce::File& file);
    juce::File getDefaultPresetsFile();
    
private:
    std::vector<Preset> presets;

    void appendFactoryPresetsIfMissing();
    
    void addDefaultPreset(const juce::String& name,
                         float grainSize, float density, float position, float pitch,
                         float reverseChance = 0.0f, float reverbWet = 0.0f,
                         const juce::String& category = "Factory");
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetManager)
};
