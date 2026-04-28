#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "WaveformView.h"
#include "AdsrComponent.h"
#include "LfoMonitorComponent.h"
#include "XYPadComponent.h"
#include "XYPadMappingPanel.h"
#include "ActiveGrainsDisplay.h"
#include "PresetManager.h"
#include "lookandfeel/EchoGrainLookAndFeel.h"

//==============================================================================
/**
 * Native JUCE Plugin Editor - UI stable et responsive
 * Layout: Header (Magenta) + 4 colonnes (Cyan/Orange/Violet/Vert) + Footer
 * 
 * Paramètres APVTS utilisés:
 * - Grain: grainSize, density, position, positionSpread, pitch, pitchSpread, reverse, pan, panSpread
 * - ADSR: adsrAttack, adsrDecay, adsrSustain, adsrRelease
 * - LFO: positionLfoFreq, positionLfoDepth, pitchLfoFreq, pitchLfoDepth, densityLfoFreq, densityLfoDepth
 * - Effects: reverbRoom, reverbDamping, reverbWet, formantFreq, formantMix
 * - XY Pad: xySlot1..4Target/Axis/Min/Max/Invert
 * - MIDI: rootNote, fineTuneCents, pitchBendRange
 */
class NativePluginEditor : public juce::AudioProcessorEditor,
                          public juce::FileDragAndDropTarget,
                          private juce::Timer
{
public:
    explicit NativePluginEditor(EchoGrainSynthAudioProcessor& p);
    ~NativePluginEditor() override;

    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;
    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray& files, int x, int y) override;
    void fileDragExit(const juce::StringArray& files) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;
    
private:
    void timerCallback() override;
    void setupSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText,
                     juce::NormalisableRange<double> range, double defaultValue);
    void styleSection(juce::GroupComponent& group, juce::Colour accent);
    void applyXYPadMappings(juce::Point<float> normalizedPosition);
    void loadSampleFile();
    bool tryLoadDroppedSample(const juce::StringArray& files);
    void refreshPresetControls();
    void savePresetsToDisk();
    void loadSelectedPreset();
    void saveCurrentPresetAs();
    void renameSelectedPreset();
    void deleteSelectedPreset();
    void applySectionReset(const juce::String& sectionName);
    
    // Processor reference
    EchoGrainSynthAudioProcessor& audioProcessor;
    
    //==============================================================================
    // UI SECTIONS (5 zones + header + footer)
    
    // === HEADER (Magenta) ===
    juce::Label titleLabel;
    juce::Label subtitleLabel;
    juce::TextButton loadSampleButton;
    juce::Label sampleNameLabel;
    juce::ComboBox presetCombo;
    juce::TextEditor presetNameEditor;
    juce::ComboBox presetCategoryCombo;
    juce::ToggleButton presetFavoriteToggle;
    juce::ToggleButton presetFavoritesOnlyToggle;
    juce::TextButton presetSaveButton;
    juce::TextButton presetRenameButton;
    juce::TextButton presetDeleteButton;
    juce::ComboBox cpuModeCombo;
    juce::Slider maxGrainsSlider;
    juce::Label maxGrainsLabel;
    std::unique_ptr<WaveformView> waveformView;
    std::unique_ptr<juce::FileChooser> fileChooser;
    bool isDraggingSample = false;
    
    // === COLUMN 1: CYAN - Granular Controls ===
    juce::GroupComponent grainGroup;
    
    // Row 1: Grain Size + Density
    juce::Slider grainSizeSlider, densitySlider;
    juce::Label grainSizeLabel, densityLabel;
    
    // Row 2: Position + Pitch
    juce::Slider positionSlider, pitchSlider;
    juce::Label positionLabel, pitchLabel;
    
    // Row 3: Position Spread + Pitch Spread
    juce::Slider positionSpreadSlider, pitchSpreadSlider;
    juce::Label positionSpreadLabel, pitchSpreadLabel;
    
    // Row 4: Pan + Pan Spread
    juce::Slider panSlider, panSpreadSlider;
    juce::Label panLabel, panSpreadLabel;
    
    // Row 5: Reverse
    juce::Slider reverseSlider;
    juce::Label reverseLabel;
    
    // Row 6: ADSR Component
    std::unique_ptr<AdsrComponent> adsrComponent;
    
    // === COLUMN 2: ORANGE - LFO Controls ===
    juce::GroupComponent lfoGroup;
    
    juce::Slider positionLfoFreqSlider, positionLfoDepthSlider;
    juce::Label positionLfoFreqLabel, positionLfoDepthLabel;
    
    juce::Slider pitchLfoFreqSlider, pitchLfoDepthSlider;
    juce::Label pitchLfoFreqLabel, pitchLfoDepthLabel;
    
    juce::Slider densityLfoFreqSlider, densityLfoDepthSlider;
    juce::Label densityLfoFreqLabel, densityLfoDepthLabel;
    
    std::unique_ptr<LfoMonitorComponent> lfoMonitor;
    
    // === COLUMN 3: VIOLET - Effects ===
    juce::GroupComponent effectsGroup;
    
    // Reverb controls
    juce::Slider reverbRoomSlider, reverbDampingSlider, reverbWetSlider;
    juce::Label reverbRoomLabel, reverbDampingLabel, reverbWetLabel;
    
    // Formant filter
    juce::Slider formantFreqSlider, formantMixSlider;
    juce::Label formantFreqLabel, formantMixLabel;

    // Master Gain
    juce::Slider masterGainSlider;
    juce::Label masterGainLabel;
    
    // === COLUMN 4: VERT - XY Pad + Mapping ===
    juce::GroupComponent xyGroup;
    std::unique_ptr<XYPadComponent> xyPad;
    std::unique_ptr<XYPadMappingPanel> xyMappingPanel;
    juce::ToggleButton xyMidiLinkToggle;
    juce::LinearSmoothedValue<float> xySmoothedX;
    juce::LinearSmoothedValue<float> xySmoothedY;
    juce::Point<float> xyTargetPosition{ 0.5f, 0.5f };
    juce::Point<float> xyLastAppliedPosition{ 0.5f, 0.5f };
    bool xyMappingNeedsUpdate = false;
    
    // MIDI Sampler controls (in green zone)
    juce::Slider rootNoteSlider, fineTuneSlider, pitchBendRangeSlider;
    juce::Label rootNoteLabel, fineTuneLabel, pitchBendRangeLabel;
    
    // === FOOTER ===
    std::unique_ptr<ActiveGrainsDisplay> activeGrainsDisplay;

    // Per-section reset actions
    juce::TextButton resetGranularButton;
    juce::TextButton resetAdsrButton;
    juce::TextButton resetLfoButton;
    juce::TextButton resetEffectsButton;
    juce::TextButton resetXYButton;
    
    //==============================================================================
    // APVTS Attachments (ownership)
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::unique_ptr<SliderAttachment> maxGrainsAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cpuModeAttachment;
    PresetManager presetManager;
    EchoGrainLookAndFeel lookAndFeel;
    
    //==============================================================================
    // Colors (matching capture)
    const juce::Colour bgColor = juce::Colour::fromString("#0A0F17");
    const juce::Colour magentaColor = juce::Colour::fromString("#CDA7E8");
    const juce::Colour cyanColor = juce::Colour::fromString("#8ED9EA");
    const juce::Colour orangeColor = juce::Colour::fromString("#D6B28D");
    const juce::Colour violetColor = juce::Colour::fromString("#A89CE8");
    const juce::Colour greenColor = juce::Colour::fromString("#8FD7BC");
    
    // Layout constants
    static constexpr int MIN_WIDTH = 1000;
    static constexpr int MIN_HEIGHT = 600;
    static constexpr int DEFAULT_WIDTH = 1200;
    static constexpr int DEFAULT_HEIGHT = 700;
    static constexpr int HEADER_HEIGHT = 178;
    static constexpr int FOOTER_HEIGHT = 112;
    static constexpr int MARGIN = 8;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NativePluginEditor)
};
