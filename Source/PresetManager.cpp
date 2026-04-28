#include "PresetManager.h"

PresetManager::PresetManager()
{
    initializeDefaultPresets();
}

PresetManager::~PresetManager()
{
}

void PresetManager::initializeDefaultPresets()
{
    presets.clear();
    addDefaultPreset("Init Empty", 100.0f, 0.1f, 0.0f, 1.0f, 0.0f, 0.0f, "Factory");

    appendFactoryPresetsIfMissing();
}

void PresetManager::appendFactoryPresetsIfMissing()
{
    const auto ensurePreset = [this](const juce::String& name,
                                     float grainSize,
                                     float density,
                                     float position,
                                     float pitch,
                                     float reverseChance,
                                     float reverbWet,
                                     const juce::String& category)
    {
        for (auto& preset : presets)
        {
            if (preset.name == name)
            {
                if (preset.category == "Factory" || preset.category.isEmpty() || preset.category == "Uncategorized")
                    preset.category = category;
                return;
            }
        }

        addDefaultPreset(name, grainSize, density, position, pitch, reverseChance, reverbWet, category);
    };

    // Factory bank target: 30 total presets including "Init Empty".
    ensurePreset("Classic Grain", 100.0f, 10.0f, 0.0f, 1.0f, 0.0f, 0.0f, "Lead");
    ensurePreset("Dense Cloud", 50.0f, 25.0f, 0.5f, 1.0f, 0.0f, 0.2f, "Texture");
    ensurePreset("Slow Motion", 500.0f, 2.0f, 0.0f, 0.5f, 0.0f, 0.3f, "Texture");
    ensurePreset("Reverse Echo", 200.0f, 8.0f, 0.3f, 1.0f, 0.7f, 0.4f, "FX");
    ensurePreset("Granular Pad", 300.0f, 15.0f, 0.2f, 1.2f, 0.2f, 0.5f, "Pad");
    ensurePreset("Glitchy Texture", 80.0f, 20.0f, 0.6f, 1.5f, 0.3f, 0.1f, "FX");
    ensurePreset("Micro Grains", 20.0f, 30.0f, 0.8f, 2.0f, 0.1f, 0.0f, "FX");
    ensurePreset("Stretched Time", 600.0f, 5.0f, 0.1f, 0.25f, 0.5f, 0.6f, "Texture");
    ensurePreset("Airy Bloom", 280.0f, 12.0f, 0.18f, 1.08f, 0.05f, 0.42f, "Pad");
    ensurePreset("Silver Mist", 360.0f, 9.5f, 0.34f, 0.92f, 0.12f, 0.48f, "Pad");
    ensurePreset("Pulse Dust", 38.0f, 28.0f, 0.72f, 1.62f, 0.22f, 0.08f, "Perc");
    ensurePreset("Tape Haze", 175.0f, 13.0f, 0.27f, 0.84f, 0.18f, 0.24f, "Texture");
    ensurePreset("Drone Veil", 520.0f, 3.5f, 0.09f, 0.42f, 0.44f, 0.58f, "Pad");
    ensurePreset("Frozen Choir", 430.0f, 7.0f, 0.41f, 0.67f, 0.36f, 0.62f, "Pad");
    ensurePreset("Perc Shards", 24.0f, 30.0f, 0.65f, 2.25f, 0.05f, 0.03f, "Perc");
    ensurePreset("Neon Swarm", 68.0f, 26.0f, 0.55f, 1.82f, 0.27f, 0.17f, "Lead");
    ensurePreset("Orbital Wash", 340.0f, 11.0f, 0.29f, 0.78f, 0.31f, 0.52f, "Pad");
    ensurePreset("Velvet Drift", 260.0f, 14.0f, 0.23f, 1.14f, 0.08f, 0.33f, "Pad");
    ensurePreset("Crushed Rain", 46.0f, 27.0f, 0.83f, 1.96f, 0.29f, 0.06f, "Perc");
    ensurePreset("Clockwork Grain", 94.0f, 18.0f, 0.47f, 1.32f, 0.14f, 0.11f, "Perc");
    ensurePreset("Harmonic Spray", 112.0f, 16.0f, 0.36f, 1.48f, 0.09f, 0.2f, "Lead");
    ensurePreset("Broken Cassette", 210.0f, 10.5f, 0.14f, 0.72f, 0.24f, 0.28f, "Texture");
    ensurePreset("Dark Halo", 390.0f, 6.0f, 0.22f, 0.58f, 0.4f, 0.55f, "Pad");
    ensurePreset("Wide Mirage", 145.0f, 17.5f, 0.5f, 1.26f, 0.17f, 0.25f, "Texture");
    ensurePreset("Pluck Spray", 32.0f, 29.0f, 0.75f, 2.4f, 0.04f, 0.02f, "Perc");
    ensurePreset("Low Ember", 470.0f, 4.2f, 0.06f, 0.38f, 0.46f, 0.5f, "Pad");
    ensurePreset("Crystal Bloom", 235.0f, 12.8f, 0.44f, 1.56f, 0.11f, 0.36f, "Texture");
    ensurePreset("Reverse Bloom", 310.0f, 9.0f, 0.31f, 0.94f, 0.64f, 0.46f, "Texture");
    ensurePreset("Dust Choir", 410.0f, 7.8f, 0.19f, 0.62f, 0.38f, 0.57f, "Pad");
}

void PresetManager::addDefaultPreset(const juce::String& name, 
                                    float grainSize, float density, float position, float pitch,
                                    float reverseChance, float reverbWet,
                                    const juce::String& category)
{
    juce::ValueTree presetState("Parameters");
    
    // Set granular parameters
    presetState.setProperty("grainSize", grainSize, nullptr);
    presetState.setProperty("density", density, nullptr);
    presetState.setProperty("position", position, nullptr);
    presetState.setProperty("pitch", pitch, nullptr);
    presetState.setProperty("reverse", reverseChance, nullptr);
    
    // Set default values for other parameters
    presetState.setProperty("positionSpread", 0.1f, nullptr);
    presetState.setProperty("pitchSpread", 0.0f, nullptr);
    presetState.setProperty("pan", 0.0f, nullptr);
    presetState.setProperty("panSpread", 0.0f, nullptr);
    presetState.setProperty("adsrAttack", 10.0f, nullptr);
    presetState.setProperty("adsrDecay", 100.0f, nullptr);
    presetState.setProperty("adsrSustain", 0.7f, nullptr);
    presetState.setProperty("adsrRelease", 100.0f, nullptr);
    
    // LFO parameters
    presetState.setProperty("positionLfoFreq", 1.0f, nullptr);
    presetState.setProperty("positionLfoDepth", 0.0f, nullptr);
    presetState.setProperty("pitchLfoFreq", 1.0f, nullptr);
    presetState.setProperty("pitchLfoDepth", 0.0f, nullptr);
    presetState.setProperty("densityLfoFreq", 1.0f, nullptr);
    presetState.setProperty("densityLfoDepth", 0.0f, nullptr);
    
    // Effects parameters
    presetState.setProperty("reverbRoom", 0.5f, nullptr);
    presetState.setProperty("reverbDamping", 0.5f, nullptr);
    presetState.setProperty("reverbWet", reverbWet, nullptr);
    presetState.setProperty("formantFreq", 800.0f, nullptr);
    presetState.setProperty("formantMix", 0.0f, nullptr);
    
    presets.emplace_back(name, presetState, category, false);
}

void PresetManager::savePreset(const juce::String& name, const juce::ValueTree& state,
                               const juce::String& category, bool favorite)
{
    // Check if preset already exists
    for (auto& preset : presets)
    {
        if (preset.name == name)
        {
            preset.state = state.createCopy();
            preset.category = category;
            preset.favorite = favorite;
            return;
        }
    }
    
    // Add new preset
    presets.emplace_back(name, state.createCopy(), category, favorite);
}

bool PresetManager::loadPreset(const juce::String& name, juce::ValueTree& state)
{
    for (const auto& preset : presets)
    {
        if (preset.name == name)
        {
            state = preset.state.createCopy();
            return true;
        }
    }
    return false;
}

void PresetManager::deletePreset(const juce::String& name)
{
    presets.erase(std::remove_if(presets.begin(), presets.end(),
                                [&name](const Preset& preset) { return preset.name == name; }),
                 presets.end());
}

bool PresetManager::renamePreset(const juce::String& oldName, const juce::String& newName)
{
    if (newName.isEmpty())
        return false;

    for (const auto& preset : presets)
        if (preset.name == newName)
            return false;

    for (auto& preset : presets)
    {
        if (preset.name == oldName)
        {
            preset.name = newName;
            return true;
        }
    }

    return false;
}

bool PresetManager::setPresetCategory(const juce::String& name, const juce::String& category)
{
    for (auto& preset : presets)
    {
        if (preset.name == name)
        {
            preset.category = category;
            return true;
        }
    }

    return false;
}

bool PresetManager::setPresetFavorite(const juce::String& name, bool favorite)
{
    for (auto& preset : presets)
    {
        if (preset.name == name)
        {
            preset.favorite = favorite;
            return true;
        }
    }

    return false;
}

juce::String PresetManager::getPresetCategory(const juce::String& name) const
{
    for (const auto& preset : presets)
        if (preset.name == name)
            return preset.category;

    return "Uncategorized";
}

bool PresetManager::isPresetFavorite(const juce::String& name) const
{
    for (const auto& preset : presets)
        if (preset.name == name)
            return preset.favorite;

    return false;
}

juce::StringArray PresetManager::getPresetNames(const juce::String& categoryFilter, bool favoritesOnly) const
{
    juce::StringArray names;
    for (const auto& preset : presets)
    {
        if (favoritesOnly && !preset.favorite)
            continue;

        if (categoryFilter.isNotEmpty() && categoryFilter != "All" && preset.category != categoryFilter)
            continue;

        names.add(preset.name);
    }

    names.sort(true);

    if (names.contains("Init Empty"))
    {
        names.removeString("Init Empty");
        names.insert(0, "Init Empty");
    }

    return names;
}

juce::StringArray PresetManager::getPresetNames() const
{
    return getPresetNames("All", false);
}

juce::StringArray PresetManager::getCategories() const
{
    juce::StringArray categories;
    categories.add("All");
    categories.add("Uncategorized");

    for (const auto& preset : presets)
    {
        if (preset.category.isNotEmpty() && !categories.contains(preset.category))
            categories.add(preset.category);
    }

    categories.sort(true);
    if (!categories.contains("All"))
        categories.insert(0, "All");
    return categories;
}

bool PresetManager::savePresetsToFile(const juce::File& file)
{
    juce::ValueTree presetsTree("Presets");
    
    for (const auto& preset : presets)
    {
        juce::ValueTree presetTree("Preset");
        presetTree.setProperty("name", preset.name, nullptr);
        presetTree.setProperty("category", preset.category, nullptr);
        presetTree.setProperty("favorite", preset.favorite, nullptr);
        presetTree.addChild(preset.state.createCopy(), -1, nullptr);
        presetsTree.addChild(presetTree, -1, nullptr);
    }
    
    std::unique_ptr<juce::XmlElement> xml(presetsTree.createXml());
    return xml->writeTo(file);
}

bool PresetManager::loadPresetsFromFile(const juce::File& file)
{
    if (!file.existsAsFile())
        return false;
        
    std::unique_ptr<juce::XmlElement> xml(juce::XmlDocument::parse(file));
    if (xml == nullptr)
        return false;
        
    juce::ValueTree presetsTree = juce::ValueTree::fromXml(*xml);
    if (!presetsTree.hasType("Presets"))
        return false;
        
    presets.clear();
    
    for (int i = 0; i < presetsTree.getNumChildren(); ++i)
    {
        juce::ValueTree presetTree = presetsTree.getChild(i);
        if (presetTree.hasType("Preset"))
        {
            juce::String name = presetTree.getProperty("name");
            juce::String category = presetTree.getProperty("category", "Uncategorized");
            bool favorite = static_cast<bool>(presetTree.getProperty("favorite", false));
            if (presetTree.getNumChildren() > 0)
            {
                juce::ValueTree state = presetTree.getChild(0);
                presets.emplace_back(name, state, category, favorite);
            }
        }
    }

    bool hasInitEmpty = false;
    for (const auto& preset : presets)
    {
        if (preset.name == "Init Empty")
        {
            hasInitEmpty = true;
            break;
        }
    }

    if (!hasInitEmpty)
        addDefaultPreset("Init Empty", 100.0f, 0.1f, 0.0f, 1.0f, 0.0f, 0.0f, "Factory");

    // Backfill factory bank for users with older preset files.
    appendFactoryPresetsIfMissing();
    
    return true;
}

juce::File PresetManager::getDefaultPresetsFile()
{
    juce::File appDataDir = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory);
    juce::File pluginDir = appDataDir.getChildFile("EchoGrainFX");
    pluginDir.createDirectory();
    return pluginDir.getChildFile("presets.xml");
}
