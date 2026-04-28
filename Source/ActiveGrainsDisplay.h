#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
 * ActiveGrainsDisplay - Affichage des grains actifs avec meter lisible
 */
class ActiveGrainsDisplay : public juce::Component,
                           private juce::Timer
{
public:
    ActiveGrainsDisplay();
    ~ActiveGrainsDisplay() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void updateGrainCount(); // Made public for timer callback
    
    // Set the processor reference after construction
    void setProcessor(EchoGrainSynthAudioProcessor* processor) { audioProcessor = processor; }

private:
    void timerCallback() override;
    
    EchoGrainSynthAudioProcessor* audioProcessor = nullptr;
    
    // Données d'affichage
    int currentGrainCount = 0;
    int displayedGrainCount = 0; // Lissé pour éviter les à-coups
    float animationPhase = 0.0f;
    float impactFlash = 0.0f;
    float previousNormalizedActivity = 0.0f;

    static constexpr int HISTORY_SIZE = 120;
    std::array<float, HISTORY_SIZE> activityHistory{};

    static constexpr int MAX_GRAIN_SLOTS = 64;
    std::vector<GrainVisualizationPoint> currentPoints;
    std::array<juce::Point<float>, MAX_GRAIN_SLOTS> previousPointByIndex{};
    std::array<bool, MAX_GRAIN_SLOTS> hadPreviousPoint{};
    
    // Constants
    static constexpr int UPDATE_RATE_HZ = 30;
    static constexpr float SMOOTHING_FACTOR = 0.85f;
    
    // Couleurs conformes à la capture
    juce::Colour cyanColor = juce::Colour::fromString("#00FFFF");
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ActiveGrainsDisplay)
};
