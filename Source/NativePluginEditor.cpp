#include "NativePluginEditor.h"

namespace
{
bool isSupportedSampleFile(const juce::File& file)
{
    return file.existsAsFile() && file.hasFileExtension("wav;mp3;aif;aiff;flac;ogg");
}

juce::String getXYTargetParameterId(int targetIndex)
{
    switch (targetIndex)
    {
        case 1: return "grainSize";
        case 2: return "density";
        case 3: return "position";
        case 4: return "pitch";
        case 5: return "pan";
        case 6: return "formantFreq";
        case 7: return "reverbWet";
        default: return {};
    }
}
}

//==============================================================================
NativePluginEditor::NativePluginEditor(EchoGrainSynthAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    juce::LookAndFeel::setDefaultLookAndFeel(&lookAndFeel);

    // Set editor constraints
    setResizable(true, true);
    setResizeLimits(MIN_WIDTH, MIN_HEIGHT, 2400, 1400);
    
    //==========================================================================
    // HEADER (Magenta zone)
    //==========================================================================
    addAndMakeVisible(titleLabel);
    titleLabel.setText("ECHO GRAIN SYNTH", juce::dontSendNotification);
    titleLabel.setFont(juce::Font(juce::FontOptions(27.0f, juce::Font::bold)));
    titleLabel.setColour(juce::Label::textColourId, juce::Colours::transparentBlack);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Solar Bumper's Ethereal Granular Instrument", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::transparentBlack);
    subtitleLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(loadSampleButton);
    loadSampleButton.setButtonText("BROWSE");
    loadSampleButton.onClick = [this] { loadSampleFile(); };
    loadSampleButton.setColour(juce::TextButton::buttonColourId, magentaColor);
    loadSampleButton.setLookAndFeel(&lookAndFeel);
    
    addAndMakeVisible(sampleNameLabel);
    auto initialSampleName = audioProcessor.getSampleName();
    if (initialSampleName.isEmpty())
        initialSampleName = "Drop a sample anywhere in the plugin";
    sampleNameLabel.setText(initialSampleName, juce::dontSendNotification);
    sampleNameLabel.setFont(juce::Font(juce::FontOptions(11.0f)));
    sampleNameLabel.setColour(juce::Label::textColourId, juce::Colours::transparentBlack);
    sampleNameLabel.setJustificationType(juce::Justification::centredLeft);

    addAndMakeVisible(presetCombo);
    presetCombo.setEditableText(false);
    presetCombo.setTextWhenNothingSelected("Select preset v");
    presetCombo.onChange = [this]
    {
        // Only auto-load when an existing preset item is selected from the list.
        if (presetCombo.getSelectedItemIndex() < 0)
            return;

        const auto selected = presetCombo.getText();
        if (selected.isNotEmpty())
        {
            audioProcessor.setSelectedPresetName(selected);
            presetCategoryCombo.setText(presetManager.getPresetCategory(selected), juce::dontSendNotification);
            presetFavoriteToggle.setToggleState(presetManager.isPresetFavorite(selected), juce::dontSendNotification);
            presetNameEditor.setText(selected, juce::dontSendNotification);
            loadSelectedPreset();
        }
    };

    addAndMakeVisible(presetNameEditor);
    presetNameEditor.setTextToShowWhenEmpty("Save as / Rename...", juce::Colours::white.withAlpha(0.4f));
    presetNameEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xff14243a));
    presetNameEditor.setColour(juce::TextEditor::outlineColourId, cyanColor.withAlpha(0.35f));
    presetNameEditor.setColour(juce::TextEditor::textColourId, juce::Colours::white.withAlpha(0.95f));
    presetNameEditor.setColour(juce::TextEditor::focusedOutlineColourId, cyanColor.withAlpha(0.75f));
    presetNameEditor.setColour(juce::CaretComponent::caretColourId, juce::Colours::white.withAlpha(0.9f));

    addAndMakeVisible(presetCategoryCombo);
    presetCategoryCombo.addItem("All", 1);
    presetCategoryCombo.addItem("Factory", 2);
    presetCategoryCombo.addItem("Pad", 3);
    presetCategoryCombo.addItem("Texture", 4);
    presetCategoryCombo.addItem("Perc", 5);
    presetCategoryCombo.addItem("Lead", 6);
    presetCategoryCombo.addItem("FX", 7);
    presetCategoryCombo.addItem("Uncategorized", 8);
    presetCategoryCombo.setSelectedId(1, juce::dontSendNotification);
    presetCategoryCombo.onChange = [this]
    {
        refreshPresetControls();
    };

    addAndMakeVisible(presetFavoriteToggle);
    presetFavoriteToggle.setButtonText("Fav");
    presetFavoriteToggle.onClick = [this]
    {
        const auto selected = presetCombo.getText();
        if (selected.isNotEmpty())
        {
            presetManager.setPresetFavorite(selected, presetFavoriteToggle.getToggleState());
            savePresetsToDisk();
        }
    };

    addAndMakeVisible(presetFavoritesOnlyToggle);
    presetFavoritesOnlyToggle.setButtonText("Fav Only");
    presetFavoritesOnlyToggle.onClick = [this] { refreshPresetControls(); };

    addAndMakeVisible(presetSaveButton);
    presetSaveButton.setButtonText("SAVE");
    presetSaveButton.onClick = [this] { saveCurrentPresetAs(); };

    addAndMakeVisible(presetRenameButton);
    presetRenameButton.setButtonText("RENAME");
    presetRenameButton.onClick = [this] { renameSelectedPreset(); };

    addAndMakeVisible(presetDeleteButton);
    presetDeleteButton.setButtonText("DELETE");
    presetDeleteButton.onClick = [this] { deleteSelectedPreset(); };

    addAndMakeVisible(cpuModeCombo);
    cpuModeCombo.addItem("Eco", 1);
    cpuModeCombo.addItem("High", 2);

    setupSlider(maxGrainsSlider, maxGrainsLabel, "MAX GRAINS",
                juce::NormalisableRange<double>(8.0, 64.0, 1.0), 40.0);
    maxGrainsLabel.setText("MAX ACTIVE GRAINS", juce::dontSendNotification);
    maxGrainsLabel.attachToComponent(nullptr, false); // pas de label flottant dans le header
    maxGrainsSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    maxGrainsSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 54, 18);
    maxGrainsSlider.setNumDecimalPlacesToDisplay(0);
    maxGrainsSlider.textFromValueFunction = [](double v)
    {
        return juce::String(static_cast<int>(std::round(v)));
    };
    maxGrainsSlider.valueFromTextFunction = [](const juce::String& text)
    {
        return text.getDoubleValue();
    };

    auto setupResetButton = [this](juce::TextButton& button, const juce::String& sectionName)
    {
        addAndMakeVisible(button);
        button.setButtonText("RESET");
        button.setOpaque(true);
        button.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff1a2434));
        button.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff24354d));
        button.setColour(juce::TextButton::textColourOffId, juce::Colours::white.withAlpha(0.96f));
        button.setColour(juce::TextButton::textColourOnId, juce::Colours::white.withAlpha(0.96f));
        const auto safeEditor = juce::Component::SafePointer<NativePluginEditor>(this);

        button.onClick = [safeEditor, sectionName]
        {
            if (safeEditor == nullptr)
                return;

            const auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
                juce::MessageBoxIconType::WarningIcon,
                "Reset " + sectionName,
                "Reset " + sectionName + " to defaults?",
                "Reset",
                "Cancel",
                safeEditor.getComponent());

            juce::AlertWindow::showAsync(options, [safeEditor, sectionName](int result)
            {
                if (result != 0 && safeEditor != nullptr)
                    safeEditor->applySectionReset(sectionName);
            });
        };
    };

    setupResetButton(resetGranularButton, "GRANULAR");
    setupResetButton(resetAdsrButton, "ADSR");
    setupResetButton(resetLfoButton, "LFO");
    setupResetButton(resetEffectsButton, "EFFECTS");
    setupResetButton(resetXYButton, "XY/MIDI");

    waveformView = std::make_unique<WaveformView>();
    addAndMakeVisible(waveformView.get());
    waveformView->setProcessor(&audioProcessor);
    if (auto sampleFile = audioProcessor.getLoadedSampleFile(); sampleFile.existsAsFile())
        waveformView->setSource(new juce::FileInputSource(sampleFile));

    // A/B bypass toggle
    addAndMakeVisible(abToggle);
    abToggle.setClickingTogglesState(true);
    abToggle.setButtonText("A");
    abToggle.setToggleState(audioProcessor.isABBypassEnabled(), juce::dontSendNotification);
    abToggle.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff1a2434));
    abToggle.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xffB06EC7));  // magenta accent when B active
    abToggle.setColour(juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha(0.85f));
    abToggle.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    abToggle.setLookAndFeel(&lookAndFeel);
    abToggle.onClick = [this]
    {
        const bool isB = abToggle.getToggleState();
        abToggle.setButtonText(isB ? "B" : "A");
        audioProcessor.setABBypass(isB);
    };

    // Connecter les marqueurs start/end au GrainEngine
    waveformView->onStartMarkerChanged = [this](float norm)
    {
        if (auto* engine = audioProcessor.getGrainEngine())
            engine->setSampleRange(norm, waveformView->getSampleEndNorm());
    };
    waveformView->onEndMarkerChanged = [this](float norm)
    {
        if (auto* engine = audioProcessor.getGrainEngine())
            engine->setSampleRange(waveformView->getSampleStartNorm(), norm);
    };
    // Drag de la barre jaune -> paramètre position
    waveformView->onPositionChanged = [this](float norm)
    {
        if (auto* param = audioProcessor.getValueTreeState().getParameter("position"))
            param->setValueNotifyingHost(param->convertTo0to1(norm));
    };
    
    //==========================================================================
    // COLUMN 1: CYAN - Granular Controls
    //==========================================================================
    addAndMakeVisible(grainGroup);
    styleSection(grainGroup, cyanColor);
    grainGroup.setText("GRANULAR");

    // Freeze button
    addAndMakeVisible(freezeButton);
    freezeButton.setClickingTogglesState(true);
    freezeButton.setButtonText("FREEZE");
    freezeButton.setColour(juce::TextButton::buttonColourId,   juce::Colour(0xff1a2434));
    freezeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff3A8AB0));
    freezeButton.setColour(juce::TextButton::textColourOffId,  juce::Colours::white.withAlpha(0.85f));
    freezeButton.setColour(juce::TextButton::textColourOnId,   juce::Colours::white);
    freezeButton.setLookAndFeel(&lookAndFeel);
    freezeButton.onClick = [this]
    {
        if (auto* param = audioProcessor.getValueTreeState().getParameter("freeze"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(freezeButton.getToggleState() ? 1.0f : 0.0f);
            param->endChangeGesture();
        }
    };

    // Window type combo
    addAndMakeVisible(windowTypeCombo);
    windowTypeCombo.addItem("Hanning",     1);
    windowTypeCombo.addItem("Gaussian",    2);
    windowTypeCombo.addItem("Rectangular", 3);
    windowTypeCombo.addItem("Tukey",       4);
    windowTypeCombo.setSelectedId(1, juce::dontSendNotification);
    windowTypeCombo.onChange = [this]
    {
        if (auto* param = audioProcessor.getValueTreeState().getParameter("windowType"))
        {
            const float v = static_cast<float>(windowTypeCombo.getSelectedItemIndex());
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(v));
            param->endChangeGesture();
        }
    };

    // Row 1: Grain Size + GrainSizeSpread + Density
    setupSlider(grainSizeSlider, grainSizeLabel, "GRAIN SIZE",
                juce::NormalisableRange<double>(10.0, 600.0, 1.0), 100.0);
    setupSlider(grainSizeSpreadSlider, grainSizeSpreadLabel, "SIZE SPREAD",
                juce::NormalisableRange<double>(0.0, 200.0, 1.0), 0.0);
    setupSlider(densitySlider, densityLabel, "DENSITY",
                juce::NormalisableRange<double>(0.1, 30.0, 0.1), 10.0);
    
    // Row 2: Position + Pitch  
    setupSlider(positionSlider, positionLabel, "POSITION", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);
    setupSlider(pitchSlider, pitchLabel, "PITCH", 
                juce::NormalisableRange<double>(0.1, 4.0, 0.01), 1.0);
    
    // Row 3: Position Spread + Pitch Spread
    setupSlider(positionSpreadSlider, positionSpreadLabel, "POS SPREAD", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.1);
    setupSlider(pitchSpreadSlider, pitchSpreadLabel, "PITCH SPREAD", 
                juce::NormalisableRange<double>(0.0, 12.0, 0.1), 0.0);
    
    // Row 4: Pan + Pan Spread
    setupSlider(panSlider, panLabel, "PAN", 
                juce::NormalisableRange<double>(-1.0, 1.0, 0.01), 0.0);
    setupSlider(panSpreadSlider, panSpreadLabel, "PAN SPREAD", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);
    
    // Row 5: Reverse
    setupSlider(reverseSlider, reverseLabel, "REVERSE", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);
    
    // Row 6: ADSR Component
    adsrComponent = std::make_unique<AdsrComponent>(audioProcessor.getValueTreeState());
    addAndMakeVisible(adsrComponent.get());
    
    //==========================================================================
    // COLUMN 2: ORANGE - LFO Controls
    //==========================================================================
    addAndMakeVisible(lfoGroup);
    styleSection(lfoGroup, orangeColor);
    lfoGroup.setText("LFO MODULATION");

    // LFO waveform shape combo
    addAndMakeVisible(lfoWaveformCombo);
    lfoWaveformCombo.addItem("Sine",     1);
    lfoWaveformCombo.addItem("Square",   2);
    lfoWaveformCombo.addItem("Triangle", 3);
    lfoWaveformCombo.addItem("S&H",      4);
    lfoWaveformCombo.setSelectedId(1, juce::dontSendNotification);
    lfoWaveformCombo.onChange = [this]
    {
        if (auto* param = audioProcessor.getValueTreeState().getParameter("lfoWaveform"))
        {
            const float v = static_cast<float>(lfoWaveformCombo.getSelectedItemIndex());
            param->beginChangeGesture();
            param->setValueNotifyingHost(param->convertTo0to1(v));
            param->endChangeGesture();
        }
    };
    
    setupSlider(positionLfoFreqSlider, positionLfoFreqLabel, "POS FREQ", 
                juce::NormalisableRange<double>(0.1, 12.0, 0.1), 1.0);
    setupSlider(positionLfoDepthSlider, positionLfoDepthLabel, "POS DEPTH", 
                juce::NormalisableRange<double>(0.0, 0.5, 0.01), 0.0);
    
    setupSlider(pitchLfoFreqSlider, pitchLfoFreqLabel, "PIT FREQ", 
                juce::NormalisableRange<double>(0.1, 12.0, 0.1), 1.0);
    setupSlider(pitchLfoDepthSlider, pitchLfoDepthLabel, "PIT DEPTH", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);
    
    setupSlider(densityLfoFreqSlider, densityLfoFreqLabel, "DEN FREQ",
                juce::NormalisableRange<double>(0.1, 12.0, 0.1), 1.0);
    setupSlider(densityLfoDepthSlider, densityLfoDepthLabel, "DEN DEPTH",
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);

    // GrainSize LFO
    setupSlider(grainSizeLfoFreqSlider, grainSizeLfoFreqLabel, "SZ FREQ",
                juce::NormalisableRange<double>(0.01, 10.0, 0.01), 1.0);
    setupSlider(grainSizeLfoDepthSlider, grainSizeLfoDepthLabel, "SZ DEPTH",
                juce::NormalisableRange<double>(0.0, 100.0, 1.0), 0.0);
    
    lfoMonitor = std::make_unique<LfoMonitorComponent>(audioProcessor);
    addAndMakeVisible(lfoMonitor.get());
    
    //==========================================================================
    // COLUMN 3: VIOLET - Effects
    //==========================================================================
    addAndMakeVisible(effectsGroup);
    styleSection(effectsGroup, violetColor);
    effectsGroup.setText("EFFECTS");
    
    // Reverb
    setupSlider(reverbRoomSlider, reverbRoomLabel, "ROOM", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.5);
    setupSlider(reverbDampingSlider, reverbDampingLabel, "DAMP", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.5);
    setupSlider(reverbWetSlider, reverbWetLabel, "REVERB WET", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);
    

    // Formant
    setupSlider(formantFreqSlider, formantFreqLabel, "FORMANT FREQ", 
                juce::NormalisableRange<double>(150.0, 3200.0, 1.0), 800.0);
    setupSlider(formantMixSlider, formantMixLabel, "FORMANT MIX", 
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);

    // Delay
    setupSlider(delayTimeSlider, delayTimeLabel, "DELAY TIME",
                juce::NormalisableRange<double>(1.0, 2000.0, 1.0), 375.0);
    setupSlider(delayFeedbackSlider, delayFeedbackLabel, "FEEDBACK",
                juce::NormalisableRange<double>(0.0, 0.95, 0.01), 0.3);
    setupSlider(delayWetSlider, delayWetLabel, "DELAY WET",
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);

    addAndMakeVisible(delayBpmSyncButton);
    delayBpmSyncButton.setButtonText("BPM SYNC");
    delayBpmSyncButton.setColour(juce::ToggleButton::textColourId, violetColor);

    addAndMakeVisible(delaySubdivisionCombo);
    delaySubdivisionCombo.addItem("1/1",  1);
    delaySubdivisionCombo.addItem("1/2",  2);
    delaySubdivisionCombo.addItem("1/4",  3);
    delaySubdivisionCombo.addItem("1/8",  4);
    delaySubdivisionCombo.addItem("1/16", 5);
    delaySubdivisionCombo.setSelectedId(3, juce::dontSendNotification);

    addAndMakeVisible(delaySubdivisionLabel);
    delaySubdivisionLabel.setText("SUBDIV", juce::dontSendNotification);
    delaySubdivisionLabel.setFont(juce::Font(juce::FontOptions(9.0f)));
    delaySubdivisionLabel.setColour(juce::Label::textColourId, violetColor);
    delaySubdivisionLabel.setJustificationType(juce::Justification::centred);

    // Master Gain (sous la Formant Mix)
    setupSlider(masterGainSlider, masterGainLabel, "MASTER GAIN", juce::NormalisableRange<double>(0.0, 2.0, 0.01), 1.0);

    
    //==========================================================================
    // COLUMN 4: VERT - XY Pad + Mapping
    //==========================================================================
    addAndMakeVisible(xyGroup);
    styleSection(xyGroup, greenColor);
    xyGroup.setText("XY PAD");
    
    xyPad = std::make_unique<XYPadComponent>();
    addAndMakeVisible(xyPad.get());

    xySmoothedX.reset(60.0, 0.05);
    xySmoothedY.reset(60.0, 0.05);
    xySmoothedX.setCurrentAndTargetValue(0.5f);
    xySmoothedY.setCurrentAndTargetValue(0.5f);

    xyPad->onPositionChanged = [this](juce::Point<float> normalizedPosition)
    {
        xyTargetPosition = {
            juce::jlimit(0.0f, 1.0f, normalizedPosition.x),
            juce::jlimit(0.0f, 1.0f, normalizedPosition.y)
        };

        xySmoothedX.setTargetValue(xyTargetPosition.x);
        xySmoothedY.setTargetValue(xyTargetPosition.y);
        xyMappingNeedsUpdate = true;
    };
    
    xyMappingPanel = std::make_unique<XYPadMappingPanel>(audioProcessor.getValueTreeState());
    addAndMakeVisible(xyMappingPanel.get());

    addAndMakeVisible(xyMidiLinkToggle);
    xyMidiLinkToggle.setButtonText("XY -> MIDI");
    xyMidiLinkToggle.setToggleState(audioProcessor.isXYMidiLinkEnabled(), juce::dontSendNotification);
    xyMidiLinkToggle.onClick = [this]
    {
        audioProcessor.setXYMidiLinkEnabled(xyMidiLinkToggle.getToggleState());
    };
    xyMidiLinkToggle.setTooltip("When enabled, XY also controls Root Note, Fine Tune, and PB Range");
    xyMidiLinkToggle.setColour(juce::ToggleButton::textColourId, juce::Colours::white.withAlpha(0.88f));
    
    // MIDI Sampler (in green zone)
    setupSlider(rootNoteSlider, rootNoteLabel, "ROOT NOTE", 
                juce::NormalisableRange<double>(24.0, 96.0, 1.0), 60.0);
    setupSlider(fineTuneSlider, fineTuneLabel, "FINE TUNE", 
                juce::NormalisableRange<double>(-50.0, 50.0, 1.0), 0.0);
    setupSlider(pitchBendRangeSlider, pitchBendRangeLabel, "PB RANGE", 
                juce::NormalisableRange<double>(1.0, 7.0, 1.0), 2.0);
    
    //==========================================================================
    // CREATE APVTS ATTACHMENTS
    //==========================================================================
    // APVTS attachments + MIDI Learn registration (right-click on any knob to map)
    attachAndRegister ("grainSize",       grainSizeSlider);
    attachAndRegister ("grainSizeSpread", grainSizeSpreadSlider);
    attachAndRegister ("density",         densitySlider);
    attachAndRegister ("position",        positionSlider);
    attachAndRegister ("positionSpread",  positionSpreadSlider);
    attachAndRegister ("pitch",           pitchSlider);
    attachAndRegister ("pitchSpread",     pitchSpreadSlider);
    attachAndRegister ("reverse",         reverseSlider);
    attachAndRegister ("pan",             panSlider);
    attachAndRegister ("panSpread",       panSpreadSlider);

    attachAndRegister ("positionLfoFreq",  positionLfoFreqSlider);
    attachAndRegister ("positionLfoDepth", positionLfoDepthSlider);
    attachAndRegister ("pitchLfoFreq",     pitchLfoFreqSlider);
    attachAndRegister ("pitchLfoDepth",    pitchLfoDepthSlider);
    attachAndRegister ("densityLfoFreq",   densityLfoFreqSlider);
    attachAndRegister ("densityLfoDepth",  densityLfoDepthSlider);
    attachAndRegister ("grainSizeLfoFreq",  grainSizeLfoFreqSlider);
    attachAndRegister ("grainSizeLfoDepth", grainSizeLfoDepthSlider);

    attachAndRegister ("reverbRoom",    reverbRoomSlider);
    attachAndRegister ("reverbDamping", reverbDampingSlider);
    attachAndRegister ("reverbWet",     reverbWetSlider);
    attachAndRegister ("formantFreq",   formantFreqSlider);
    attachAndRegister ("formantMix",    formantMixSlider);
    attachAndRegister ("delayTimeMs",   delayTimeSlider);
    attachAndRegister ("delayFeedback", delayFeedbackSlider);
    attachAndRegister ("delayWet",      delayWetSlider);
    delayBpmSyncAttachment    = std::make_unique<ButtonAttachment>(audioProcessor.getValueTreeState(), "delayBpmSync", delayBpmSyncButton);
    delaySubdivisionAttachment = std::make_unique<ComboAttachment>(audioProcessor.getValueTreeState(), "delaySubdivision", delaySubdivisionCombo);

    attachAndRegister ("rootNote",       rootNoteSlider);
    attachAndRegister ("fineTuneCents",  fineTuneSlider);
    attachAndRegister ("pitchBendRange", pitchBendRangeSlider);
    attachAndRegister ("masterGain",     masterGainSlider);

    maxGrainsAttachment = std::make_unique<SliderAttachment>(audioProcessor.getValueTreeState(), "maxActiveGrains", maxGrainsSlider);
    cpuModeAttachment   = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(audioProcessor.getValueTreeState(), "cpuMode", cpuModeCombo);

    presetManager.loadPresetsFromFile(presetManager.getDefaultPresetsFile());
    refreshPresetControls();

    if (!audioProcessor.wasStateRestoredFromProject() && presetCombo.getText().isNotEmpty())
        loadSelectedPreset();
    
    // Start timer for real-time updates (60 FPS)
    startTimerHz(60);
    
    // Set initial size
    setSize(DEFAULT_WIDTH, DEFAULT_HEIGHT);

    // Some hosts may delay or skip the initial resize callback.
    // Force one layout pass so controls are always visible on first open.
    resized();
}

NativePluginEditor::~NativePluginEditor()
{
    stopTimer();

    loadSampleButton.setLookAndFeel(nullptr);

    juce::LookAndFeel::setDefaultLookAndFeel(nullptr);
}

//==============================================================================
// MIDI Learn helpers
//==============================================================================
void NativePluginEditor::attachAndRegister (const juce::String& paramId, juce::Slider& slider)
{
    auto& apvts = audioProcessor.getValueTreeState();
    sliderAttachments.push_back (std::make_unique<SliderAttachment>(apvts, paramId, slider));
    sliderParamIds[&slider] = paramId;
    slider.addMouseListener (this, false);
}

void NativePluginEditor::showMidiLearnMenu (juce::Slider& slider)
{
    auto it = sliderParamIds.find (&slider);
    if (it == sliderParamIds.end()) return;

    const juce::String paramId = it->second;
    auto& manager = audioProcessor.getMidiLearnManager();
    const int currentCC = manager.getMappedCC (paramId);

    juce::PopupMenu menu;
    menu.addItem (1, "MIDI Learn \u2014 move a knob on your controller");
    if (currentCC >= 0)
        menu.addItem (2, "Clear CC " + juce::String (currentCC));
    menu.addSeparator();
    menu.addItem (3, "Clear all MIDI mappings");

    const juce::Component::SafePointer<NativePluginEditor> safeThis (this);
    const juce::String capturedParamId = paramId;

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&slider),
        [safeThis, capturedParamId, &manager] (int result)
        {
            if (safeThis == nullptr) return;
            if (result == 1)
                manager.startLearning (capturedParamId);
            else if (result == 2)
                manager.clearMapping (capturedParamId);
            else if (result == 3)
                manager.clearAllMappings();
            safeThis->repaint();
        });
}

void NativePluginEditor::mouseDown (const juce::MouseEvent& e)
{
    if (!e.mods.isRightButtonDown()) return;

    if (auto* slider = dynamic_cast<juce::Slider*> (e.eventComponent))
        if (sliderParamIds.count (slider) > 0)
            showMidiLearnMenu (*slider);
}

//==============================================================================
void NativePluginEditor::styleSection(juce::GroupComponent& group, juce::Colour accent)
{
    group.setTextLabelPosition(juce::Justification::centredTop);
    group.setColour(juce::GroupComponent::outlineColourId, accent.withAlpha(0.55f));
    group.setColour(juce::GroupComponent::textColourId, juce::Colours::white.withAlpha(0.85f));
}

//==============================================================================
void NativePluginEditor::setupSlider(juce::Slider& slider, juce::Label& label, 
                                    const juce::String& labelText,
                                    juce::NormalisableRange<double> range, 
                                    double defaultValue)
{
    addAndMakeVisible(slider);
    slider.setName(labelText);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setRange(range.start, range.end, range.interval);
    slider.setNumDecimalPlacesToDisplay(range.interval >= 1.0 ? 0 : 2);
    slider.textFromValueFunction = [range](double v)
    {
        if (range.interval >= 1.0)
            return juce::String(static_cast<int>(std::round(v)));

        auto text = juce::String(v, 2);
        while (text.contains(".") && (text.endsWith("0") || text.endsWith(".")))
            text = text.dropLastCharacters(1);
        return text;
    };
    slider.valueFromTextFunction = [](const juce::String& t)
    {
        return t.getDoubleValue();
    };
    slider.setValue(defaultValue);
    slider.setColour(juce::Slider::thumbColourId, cyanColor);
    slider.setColour(juce::Slider::rotarySliderFillColourId, cyanColor.withAlpha(0.7f));
    slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colours::white.withAlpha(0.3f));
    slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha(0.95f));
    slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff09131d).withAlpha(0.9f));
    slider.setColour(juce::Slider::textBoxOutlineColourId, cyanColor.withAlpha(0.25f));
    
    addAndMakeVisible(label);
    label.setText(labelText, juce::dontSendNotification);
    label.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));
    label.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.98f));
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
}

//==============================================================================
void NativePluginEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();

    juce::ColourGradient background(juce::Colour::fromString("#090E16"), bounds.getTopLeft(),
                                    juce::Colour::fromString("#111A28"), bounds.getBottomRight(), false);
    g.setGradientFill(background);
    g.fillAll();

    juce::ColourGradient mist(juce::Colours::white.withAlpha(0.03f), bounds.getCentreX(), bounds.getY(),
                              juce::Colours::transparentBlack, bounds.getCentreX(), bounds.getBottom(), false);
    g.setGradientFill(mist);
    g.fillRect(getLocalBounds());

    const auto contentTop = static_cast<float>(HEADER_HEIGHT);
    const auto contentBottom = static_cast<float>(getHeight() - FOOTER_HEIGHT);
    const auto contentHeight = contentBottom - contentTop;
    const float totalRatio = 2.4f + 1.8f + 1.8f + 2.0f;
    const float contentWidth = static_cast<float>(getWidth() - MARGIN * 2 - MARGIN * 3);
    const float left = static_cast<float>(MARGIN);
    const float col1 = contentWidth * (2.4f / totalRatio);
    const float col2 = contentWidth * (1.8f / totalRatio);
    const float col3 = contentWidth * (1.8f / totalRatio);
    const float gap = static_cast<float>(MARGIN);

    const std::array<std::pair<juce::Colour, juce::Rectangle<float>>, 4> ambientColumns = {{
        std::make_pair(cyanColor, juce::Rectangle<float>(left, contentTop, col1, contentHeight)),
        std::make_pair(orangeColor, juce::Rectangle<float>(left + col1 + gap, contentTop, col2, contentHeight)),
        std::make_pair(violetColor, juce::Rectangle<float>(left + col1 + col2 + gap * 2.0f, contentTop, col3, contentHeight)),
        std::make_pair(greenColor, juce::Rectangle<float>(left + col1 + col2 + col3 + gap * 3.0f,
                                                          contentTop,
                                                          static_cast<float>(getWidth()) - left - col1 - col2 - col3 - gap * 3.0f - static_cast<float>(MARGIN),
                                                          contentHeight))
    }};

    for (const auto& [accent, rect] : ambientColumns)
    {
        // Ambient column glow
        juce::ColourGradient glow(accent.withAlpha(0.11f), rect.getCentreX(), rect.getY(),
                                  juce::Colours::transparentBlack, rect.getCentreX(), rect.getBottom(), false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(rect.reduced(1.0f, 4.0f), 16.0f);

        // Bright LED accent strip at column top
        const float stripX = rect.getX() + 10.0f;
        const float stripW = rect.getWidth() - 20.0f;
        g.setColour(accent.withAlpha(0.68f));
        g.fillRoundedRectangle(stripX, rect.getY() + 3.0f, stripW, 1.8f, 0.9f);

        // Soft halo below the strip
        juce::ColourGradient lineHalo(accent.withAlpha(0.26f), rect.getCentreX(), rect.getY() + 5.0f,
                                      juce::Colours::transparentBlack, rect.getCentreX(), rect.getY() + 24.0f, false);
        g.setGradientFill(lineHalo);
        g.fillRect(stripX, rect.getY() + 5.0f, stripW, 19.0f);
    }
    
    // Header background (magenta)
    auto headerArea = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    juce::ColourGradient headerGradient(
        juce::Colour::fromString("#28324A").withAlpha(0.65f), headerArea.getTopLeft().toFloat(),
        juce::Colour::fromString("#1A2234").withAlpha(0.9f), headerArea.getBottomRight().toFloat(), false
    );
    g.setGradientFill(headerGradient);
    g.fillRect(headerArea);

    // ══ TITLE — Dreamcast style ════════════════════════════════
    {
        const auto titleR = titleLabel.getBounds().toFloat();
        const juce::Colour dcOrange(0xFFFF8C32);
        const juce::Font titleFont(juce::FontOptions(38.0f, juce::Font::bold | juce::Font::italic));
        g.setFont(titleFont);
        // Shadow
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.drawText("ECHO GRAIN SYNTH", titleR.translated(2.0f, 2.5f), juce::Justification::centredLeft);
        // Glow
        g.setColour(dcOrange.withAlpha(0.18f));
        g.drawText("ECHO GRAIN SYNTH", titleR.expanded(3.0f, 0.0f).translated(-1.5f, 0.0f), juce::Justification::centredLeft);
        // Main text
        g.setColour(dcOrange);
        g.drawText("ECHO GRAIN SYNTH", titleR, juce::Justification::centredLeft);
        // Cyan underline
        g.setColour(cyanColor.withAlpha(0.82f));
        g.fillRoundedRectangle(titleR.getX(), titleR.getBottom() - 2.5f,
                               titleR.getWidth() * 0.70f, 2.2f, 1.1f);
    }

    // ══ SUBTITLE ═════════════════════════════════════════
    {
        const auto subR = subtitleLabel.getBounds().toFloat();
        g.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::italic)));
        g.setColour(magentaColor.withAlpha(0.94f));
        g.drawText("Solar Bumper's Ethereal Granular Instrument",
                   subR, juce::Justification::centredLeft);
    }

    // ══ SAMPLE NAME — cyan tag chip ══════════════════════════
    {
        const auto sampleR = sampleNameLabel.getBounds().toFloat().reduced(0.0f, 2.0f);
        g.setColour(cyanColor.withAlpha(0.09f));
        g.fillRoundedRectangle(sampleR, 5.0f);
        g.setColour(cyanColor.withAlpha(0.40f));
        g.drawRoundedRectangle(sampleR, 5.0f, 1.0f);
        g.setFont(juce::Font(juce::FontOptions(11.0f)));
        g.setColour(cyanColor.withAlpha(0.96f));
        const auto rawText = sampleNameLabel.getText();
        const auto displayText = rawText.isNotEmpty() ? rawText : "Drop a sample anywhere in the plugin";
        g.drawText(displayText, sampleR.reduced(7.0f, 0.0f), juce::Justification::centredLeft);
    }
    
    // Footer background
    auto footerArea = getLocalBounds().removeFromBottom(FOOTER_HEIGHT);
    juce::ColourGradient footerGradient(juce::Colour::fromString("#080E17"), footerArea.getTopLeft().toFloat(),
                                        juce::Colour::fromString("#101A27"), footerArea.getBottomRight().toFloat(), false);
    g.setGradientFill(footerGradient);
    g.fillRect(footerArea);

    g.setColour(cyanColor.withAlpha(0.12f));
    g.drawHorizontalLine(HEADER_HEIGHT, static_cast<float>(MARGIN), static_cast<float>(getWidth() - MARGIN));
    g.setColour(magentaColor.withAlpha(0.12f));
    g.drawHorizontalLine(getHeight() - FOOTER_HEIGHT, static_cast<float>(MARGIN), static_cast<float>(getWidth() - MARGIN));

    if (isDraggingSample)
    {
        auto overlay = getLocalBounds().toFloat().reduced(20.0f);
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(overlay, 14.0f);

        g.setColour(cyanColor.withAlpha(0.95f));
        g.drawRoundedRectangle(overlay, 14.0f, 2.0f);

        g.setFont(juce::Font(juce::FontOptions(24.0f, juce::Font::bold)));
        g.drawText("Drop Sample To Load", overlay, juce::Justification::centred);
    }

    // ── MIDI Learn overlays ────────────────────────────────────────────────
    auto& mlManager = audioProcessor.getMidiLearnManager();
    const juce::String learningParam = mlManager.getLearningParamId();
    const double timeNow = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    for (auto& [sliderPtr, paramId] : sliderParamIds)
    {
        if (sliderPtr == nullptr || !sliderPtr->isShowing()) continue;

        const auto sliderBounds = getLocalArea (sliderPtr, sliderPtr->getLocalBounds()).toFloat();

        if (paramId == learningParam)
        {
            // Pulsing red ring: waiting for CC
            const float pulse = 0.55f + 0.45f * static_cast<float>(std::sin(timeNow * 5.0));
            g.setColour(juce::Colour(0xFFFF3A3A).withAlpha(pulse));
            g.drawRoundedRectangle(sliderBounds.expanded(3.0f), 8.0f, 2.2f);
            g.setColour(juce::Colour(0xFFFF3A3A).withAlpha(pulse * 0.15f));
            g.fillRoundedRectangle(sliderBounds.expanded(3.0f), 8.0f);
        }
        else
        {
            const int cc = mlManager.getMappedCC (paramId);
            if (cc >= 0)
            {
                // Small cyan CC badge in top-right corner of slider
                const juce::String badge = "CC" + juce::String(cc);
                const float bw = 28.0f, bh = 12.0f;
                const float bx = sliderBounds.getRight() - bw - 1.0f;
                const float by = sliderBounds.getY() + 1.0f;
                g.setColour(cyanColor.withAlpha(0.82f));
                g.fillRoundedRectangle(bx, by, bw, bh, 4.0f);
                g.setColour(juce::Colour(0xFF060D15));
                g.setFont(juce::Font(juce::FontOptions(8.5f, juce::Font::bold)));
                g.drawText(badge, juce::Rectangle<float>(bx, by, bw, bh), juce::Justification::centred);
            }
        }
    }

    // Status bar at the bottom when learning
    if (learningParam.isNotEmpty())
    {
        const auto statusBar = getLocalBounds().removeFromBottom(22).toFloat();
        g.setColour(juce::Colour(0xFFFF3A3A).withAlpha(0.88f));
        g.fillRect(statusBar);
        g.setColour(juce::Colours::white.withAlpha(0.96f));
        g.setFont(juce::Font(juce::FontOptions(11.0f, juce::Font::bold)));
        g.drawText("MIDI LEARN — Move a knob on your controller to map it to: " + learningParam
                   + "   (Right-click to cancel)",
                   statusBar.reduced(8.0f, 0.0f), juce::Justification::centredLeft);
    }

    // ── Peak meter ────────────────────────────────────────────────────────
    if (!peakMeterBounds.isEmpty())
    {
        const auto meterF = peakMeterBounds.toFloat();
        // Background track
        g.setColour(juce::Colours::black.withAlpha(0.55f));
        g.fillRoundedRectangle(meterF, 3.0f);
        g.setColour(violetColor.withAlpha(0.25f));
        g.drawRoundedRectangle(meterF, 3.0f, 1.0f);

        // Level fill (green → yellow → red gradient)
        const float fill = juce::jlimit(0.0f, 1.0f, currentPeakLevel);
        if (fill > 0.001f)
        {
            const auto fillRect = meterF.withWidth(meterF.getWidth() * fill);
            const juce::Colour levelColour = fill < 0.7f  ? juce::Colour(0xFF33CC66)
                                           : fill < 0.9f  ? juce::Colour(0xFFFFCC00)
                                                          : juce::Colour(0xFFFF3333);
            juce::ColourGradient meterGrad(levelColour.brighter(0.15f), fillRect.getX(), 0.0f,
                                           levelColour, fillRect.getRight(), 0.0f, false);
            g.setGradientFill(meterGrad);
            g.fillRoundedRectangle(fillRect, 3.0f);
        }

        // "OUT" label
        g.setFont(juce::Font(juce::FontOptions(8.5f)));
        g.setColour(violetColor.withAlpha(0.75f));
        g.drawText("OUT", meterF.expanded(0.0f, 0.0f), juce::Justification::centredRight);
    }
}

//==============================================================================
void NativePluginEditor::resized()
{
    constexpr int groupLabelHeight = 25;
    constexpr int labelReserve = 24;
    constexpr int columnGap = MARGIN;
    constexpr int knobSize = 58;

    auto bounds = getLocalBounds();
    
    //==========================================================================
    // HEADER (Magenta - fixed height)
    //==========================================================================
    auto headerArea = bounds.removeFromTop(HEADER_HEIGHT);
    headerArea.reduce(MARGIN * 2, MARGIN);

    const int waveformHeight = 120;
    auto waveformArea = headerArea.removeFromBottom(waveformHeight);
    headerArea.removeFromBottom(4);

    // Left info panel + right controls panel.
    const auto titleWidth = juce::jlimit(280, 420, headerArea.getWidth() / 3);
    auto titleArea = headerArea.removeFromLeft(titleWidth);
    headerArea.removeFromLeft(MARGIN * 2);

    const bool compactHeader = headerArea.getWidth() < 700;

    titleLabel.setBounds(titleArea.removeFromTop(48));
    subtitleLabel.setBounds(titleArea.removeFromTop(20));
    titleArea.removeFromTop(4);
    sampleNameLabel.setBounds(titleArea.removeFromTop(24));

    auto controlsRow1 = headerArea.removeFromTop(28);
    const int browseW = compactHeader ? 92 : 98;
    const int cpuModeW = compactHeader ? 70 : 78;
    const int maxGrainsW = compactHeader ? 200 : 240;
    const int abW = 56; // A/B toggle

    loadSampleButton.setBounds(controlsRow1.removeFromLeft(browseW));
    controlsRow1.removeFromLeft(4);
    // A/B bypass: label "A" on left, toggle on right (looks like "A [B]")
    abToggle.setBounds(controlsRow1.removeFromLeft(abW));
    controlsRow1.removeFromLeft(6);
    cpuModeCombo.setBounds(controlsRow1.removeFromLeft(cpuModeW));
    controlsRow1.removeFromLeft(6);
    auto maxGrainsArea = controlsRow1.removeFromLeft(maxGrainsW);
    // Label "MAX GRAINS" à gauche, slider prend le reste
    maxGrainsLabel.setBounds(maxGrainsArea.removeFromLeft(78));
    maxGrainsSlider.setBounds(maxGrainsArea);

    headerArea.removeFromTop(6);

    auto controlsRow2 = headerArea.removeFromTop(26);
    const int presetSelectW = compactHeader ? 120 : 168;
    const int presetCategoryW = compactHeader ? 90 : 120;
    const int favW = compactHeader ? 44 : 52;
    const int favOnlyW = compactHeader ? 62 : 72;

    const int saveW = compactHeader ? 52 : 58;
    const int renameW = compactHeader ? 72 : 82;
    const int deleteW = compactHeader ? 72 : 84;

    auto actionsArea = controlsRow2.removeFromRight(saveW + renameW + deleteW + 8);
    presetSaveButton.setBounds(actionsArea.removeFromLeft(saveW));
    actionsArea.removeFromLeft(4);
    presetRenameButton.setBounds(actionsArea.removeFromLeft(renameW));
    actionsArea.removeFromLeft(4);
    presetDeleteButton.setBounds(actionsArea);

    controlsRow2.removeFromRight(8);

    presetCombo.setBounds(controlsRow2.removeFromLeft(presetSelectW));
    controlsRow2.removeFromLeft(6);

    auto leftForNameAndMeta = controlsRow2;
    auto metaArea = leftForNameAndMeta.removeFromRight(presetCategoryW + favW + favOnlyW + 14);
    presetNameEditor.setBounds(leftForNameAndMeta);

    presetCategoryCombo.setBounds(metaArea.removeFromLeft(presetCategoryW));
    metaArea.removeFromLeft(6);
    presetFavoriteToggle.setBounds(metaArea.removeFromLeft(favW));
    metaArea.removeFromLeft(4);
    presetFavoritesOnlyToggle.setBounds(metaArea);

    if (waveformView)
        waveformView->setBounds(waveformArea);

    //==========================================================================
    // MAIN CONTENT AREA - 4 COLUMNS (Manual Layout)
    //==========================================================================
    auto contentArea = bounds.reduced(MARGIN);
    
    // Calculate column widths (ratios: 2.4 : 1.8 : 1.8 : 2.0)
    float totalRatio = 2.4f + 1.8f + 1.8f + 2.0f;
    int totalWidth = contentArea.getWidth() - (3 * columnGap);
    
    int col1Width = static_cast<int>(totalWidth * (2.4f / totalRatio));
    int col2Width = static_cast<int>(totalWidth * (1.8f / totalRatio));
    int col3Width = static_cast<int>(totalWidth * (1.8f / totalRatio));
    // col4Width uses remaining space (calculated implicitly in layout)
    
    //==========================================================================
    // COLUMN 1: CYAN - Granular Controls
    //==========================================================================
    auto col1Area = contentArea.removeFromLeft(col1Width);
    grainGroup.setBounds(col1Area);
    resetGranularButton.setBounds(col1Area.getRight() - 76, col1Area.getY() + 3, 70, 18);
    
    auto innerArea = col1Area.reduced(MARGIN * 2);
    innerArea.removeFromTop(groupLabelHeight);

    // Toolbar: [FREEZE] [WindowType combo]
    auto toolbarRow = innerArea.removeFromTop(22);
    innerArea.removeFromTop(4);
    freezeButton.setBounds(toolbarRow.removeFromLeft(70));
    toolbarRow.removeFromLeft(6);
    windowTypeCombo.setBounds(toolbarRow);

    auto adsrHeight = juce::jlimit(132, 186, innerArea.getHeight() / 2);
    auto adsrArea = innerArea.removeFromBottom(adsrHeight);
    innerArea.removeFromBottom(MARGIN);
    resetAdsrButton.setBounds(adsrArea.getRight() - 76, adsrArea.getY() + 2, 70, 18);

    const int granularColWidth = (innerArea.getWidth() - (2 * columnGap)) / 3;
    const int granularRowHeight = innerArea.getHeight() / 3;
    const int granularSizeFromHeight = (innerArea.getHeight() / 3) - labelReserve - 4;
    const int granularSizeFromWidth = granularColWidth - 6;
    const int granularCapacity = juce::jmin(granularSizeFromHeight, granularSizeFromWidth);
    const int granularSliderSize = juce::jlimit(46, 72, granularCapacity);

    auto placeSlider = [&](juce::Rectangle<int> cell, juce::Slider& slider)
    {
        cell.removeFromTop(labelReserve);
        slider.setBounds(cell.withSizeKeepingCentre(granularSliderSize, granularSliderSize));
    };

    // Row 1: Grain Size / Size Spread / Density / Reverse (4 columns)
    auto row1 = innerArea.removeFromTop(granularRowHeight);
    const int col4Width_g = (row1.getWidth() - (3 * columnGap)) / 4;
    auto grainSizeArea = row1.removeFromLeft(col4Width_g);
    placeSlider(grainSizeArea, grainSizeSlider);
    row1.removeFromLeft(columnGap);
    auto grainSizeSpreadArea = row1.removeFromLeft(col4Width_g);
    placeSlider(grainSizeSpreadArea, grainSizeSpreadSlider);
    row1.removeFromLeft(columnGap);
    auto densityArea = row1.removeFromLeft(col4Width_g);
    placeSlider(densityArea, densitySlider);
    row1.removeFromLeft(columnGap);
    placeSlider(row1, reverseSlider);

    // Row 2: Position / Pitch / Pan
    auto row2 = innerArea.removeFromTop(granularRowHeight);
    auto positionArea = row2.removeFromLeft(granularColWidth);
    placeSlider(positionArea, positionSlider);
    row2.removeFromLeft(columnGap);
    auto pitchArea = row2.removeFromLeft(granularColWidth);
    placeSlider(pitchArea, pitchSlider);
    row2.removeFromLeft(columnGap);
    placeSlider(row2, panSlider);

    // Row 3: Position Spread / Pitch Spread / Pan Spread
    auto row3 = innerArea.removeFromTop(granularRowHeight);
    auto posSpreadArea = row3.removeFromLeft(granularColWidth);
    placeSlider(posSpreadArea, positionSpreadSlider);
    row3.removeFromLeft(columnGap);
    auto pitchSpreadArea = row3.removeFromLeft(granularColWidth);
    placeSlider(pitchSpreadArea, pitchSpreadSlider);
    row3.removeFromLeft(columnGap);
    placeSlider(row3, panSpreadSlider);
    
    // Row 6: ADSR Component (remaining space)
    if (adsrComponent)
        adsrComponent->setBounds(adsrArea.reduced(MARGIN, 0));
    
    contentArea.removeFromLeft(columnGap);
    
    //==========================================================================
    // COLUMN 2: ORANGE - LFO Controls
    //==========================================================================
    auto col2Area = contentArea.removeFromLeft(col2Width);
    lfoGroup.setBounds(col2Area);
    resetLfoButton.setBounds(col2Area.getRight() - 76, col2Area.getY() + 3, 70, 18);
    
    innerArea = col2Area.reduced(MARGIN * 2);
    innerArea.removeFromTop(groupLabelHeight);

    // LFO waveform shape combo
    auto lfoWaveRow = innerArea.removeFromTop(22);
    innerArea.removeFromTop(4);
    lfoWaveformCombo.setBounds(lfoWaveRow);

    // Give the LFO monitor a generous slice at the bottom (bigger waveform displays)
    auto lfoMonitorHeight = juce::jlimit(100, 150, innerArea.getHeight() / 4);
    auto lfoMonitorArea = innerArea.removeFromBottom(lfoMonitorHeight);
    innerArea.removeFromBottom(MARGIN);

    // 4 LFO rows — distribute remaining height evenly
    int rowHeight = innerArea.getHeight() / 4;
    int sliderSize = juce::jlimit(44, 72, rowHeight - labelReserve - 6);
    
    // Position LFO (Freq + Depth)
    row1 = innerArea.removeFromTop(rowHeight);
    auto posFreqArea = row1.removeFromLeft((row1.getWidth() - columnGap) / 2);
    posFreqArea.removeFromTop(labelReserve);
    positionLfoFreqSlider.setBounds(posFreqArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    row1.removeFromLeft(columnGap);
    auto posDepthArea = row1;
    posDepthArea.removeFromTop(labelReserve);
    positionLfoDepthSlider.setBounds(posDepthArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    // Pitch LFO
    row2 = innerArea.removeFromTop(rowHeight);
    auto pitchFreqArea = row2.removeFromLeft((row2.getWidth() - columnGap) / 2);
    pitchFreqArea.removeFromTop(labelReserve);
    pitchLfoFreqSlider.setBounds(pitchFreqArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    row2.removeFromLeft(columnGap);
    auto pitchDepthArea = row2;
    pitchDepthArea.removeFromTop(labelReserve);
    pitchLfoDepthSlider.setBounds(pitchDepthArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    // Density LFO
    row3 = innerArea.removeFromTop(rowHeight);
    auto densFreqArea = row3.removeFromLeft((row3.getWidth() - columnGap) / 2);
    densFreqArea.removeFromTop(labelReserve);
    densityLfoFreqSlider.setBounds(densFreqArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    row3.removeFromLeft(columnGap);
    auto densDepthArea = row3;
    densDepthArea.removeFromTop(labelReserve);
    densityLfoDepthSlider.setBounds(densDepthArea.withSizeKeepingCentre(sliderSize, sliderSize));

    // GrainSize LFO (row 4)
    auto row4 = innerArea.removeFromTop(rowHeight);
    auto gszFreqArea = row4.removeFromLeft((row4.getWidth() - columnGap) / 2);
    gszFreqArea.removeFromTop(labelReserve);
    grainSizeLfoFreqSlider.setBounds(gszFreqArea.withSizeKeepingCentre(sliderSize, sliderSize));
    row4.removeFromLeft(columnGap);
    auto gszDepthArea = row4;
    gszDepthArea.removeFromTop(labelReserve);
    grainSizeLfoDepthSlider.setBounds(gszDepthArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    // LFO Monitor (remaining space)
    if (lfoMonitor)
        lfoMonitor->setBounds(lfoMonitorArea.reduced(MARGIN, 0));
    
    contentArea.removeFromLeft(columnGap);
    
    //==========================================================================
    // COLUMN 3: VIOLET - Effects
    //==========================================================================
    auto col3Area = contentArea.removeFromLeft(col3Width);
    effectsGroup.setBounds(col3Area);
    resetEffectsButton.setBounds(col3Area.getRight() - 76, col3Area.getY() + 3, 70, 18);
    
    innerArea = col3Area.reduced(MARGIN * 2);
    innerArea.removeFromTop(groupLabelHeight);

    // 4 effect sections: Reverb (3 knobs), Formant (2 knobs), Master Gain (1 knob), Delay (3 knobs)
    // Divide total height into 4 equal rows, compute knob size from row height
    const int numEffectRows = 4;
    const int effectGap = MARGIN;
    const int totalEffectGaps = (numEffectRows - 1) * effectGap;
    const int effectRowH = (innerArea.getHeight() - totalEffectGaps) / numEffectRows;
    sliderSize = juce::jlimit(42, 68, effectRowH - labelReserve - 6);

    // Row 1 — Reverb: Room / Damping / Wet
    {
        auto row = innerArea.removeFromTop(effectRowH);
        int w = (row.getWidth() - columnGap * 2) / 3;
        auto a1 = row.removeFromLeft(w); a1.removeFromTop(labelReserve);
        reverbRoomSlider.setBounds(a1.withSizeKeepingCentre(sliderSize, sliderSize));
        row.removeFromLeft(columnGap);
        auto a2 = row.removeFromLeft(w); a2.removeFromTop(labelReserve);
        reverbDampingSlider.setBounds(a2.withSizeKeepingCentre(sliderSize, sliderSize));
        row.removeFromLeft(columnGap);
        auto a3 = row; a3.removeFromTop(labelReserve);
        reverbWetSlider.setBounds(a3.withSizeKeepingCentre(sliderSize, sliderSize));
    }
    innerArea.removeFromTop(effectGap);

    // Row 2 — Formant: Freq / Mix
    {
        auto row = innerArea.removeFromTop(effectRowH);
        int w = (row.getWidth() - columnGap) / 2;
        auto a1 = row.removeFromLeft(w); a1.removeFromTop(labelReserve);
        formantFreqSlider.setBounds(a1.withSizeKeepingCentre(sliderSize, sliderSize));
        row.removeFromLeft(columnGap);
        auto a2 = row; a2.removeFromTop(labelReserve);
        formantMixSlider.setBounds(a2.withSizeKeepingCentre(sliderSize, sliderSize));
    }
    innerArea.removeFromTop(effectGap);

    // Row 3 — Master Gain (centred, larger knob for emphasis)
    {
        auto row = innerArea.removeFromTop(effectRowH);
        const int bigKnob = juce::jlimit(sliderSize, sliderSize + 10, effectRowH - labelReserve - 2);
        auto a = row; a.removeFromTop(labelReserve);
        masterGainSlider.setBounds(a.withSizeKeepingCentre(bigKnob, bigKnob));
    }
    innerArea.removeFromTop(effectGap);

    // Row 4 — Delay: Time / Feedback / Wet  +  BPM Sync + Subdivision
    {
        auto row = innerArea.removeFromTop(effectRowH);
        int w = (row.getWidth() - columnGap * 2) / 3;
        auto a1 = row.removeFromLeft(w); a1.removeFromTop(labelReserve);
        delayTimeSlider.setBounds(a1.withSizeKeepingCentre(sliderSize, sliderSize));
        row.removeFromLeft(columnGap);
        auto a2 = row.removeFromLeft(w); a2.removeFromTop(labelReserve);
        delayFeedbackSlider.setBounds(a2.withSizeKeepingCentre(sliderSize, sliderSize));
        row.removeFromLeft(columnGap);
        auto a3 = row; a3.removeFromTop(labelReserve);
        delayWetSlider.setBounds(a3.withSizeKeepingCentre(sliderSize, sliderSize));
    }
    // Row 5 — BPM Sync toggle + Subdivision combo (compact strip)
    {
        auto row = innerArea.removeFromTop(20);
        const int syncW = 76;
        delayBpmSyncButton.setBounds(row.removeFromLeft(syncW));
        row.removeFromLeft(4);
        delaySubdivisionLabel.setBounds(row.removeFromLeft(38));
        row.removeFromLeft(2);
        delaySubdivisionCombo.setBounds(row.removeFromLeft(70));
    }
    innerArea.removeFromTop(2);

    // Peak meter — thin vertical bar at the right edge of the effects column
    peakMeterBounds = innerArea.removeFromTop(12).reduced(0, 2);

    contentArea.removeFromLeft(columnGap);
    
    //==========================================================================
    // COLUMN 4: VERT - XY Pad + Mapping
    //==========================================================================
    auto col4Area = contentArea; // Remaining space
    xyGroup.setBounds(col4Area);
    resetXYButton.setBounds(col4Area.getRight() - 76, col4Area.getY() + 3, 70, 18);
    
    innerArea = col4Area.reduced(MARGIN * 2);
    innerArea.removeFromTop(groupLabelHeight);
    
    // Reserve mapping panel and bottom controls while keeping XY pad large enough to perform.
    const int midiRowHeight = 92;
    const int toggleHeight = 22;
    const int mappingPanelHeight = juce::jlimit(130, 190, innerArea.getHeight() / 3);
    auto bottomControls = innerArea.removeFromBottom(midiRowHeight + toggleHeight + MARGIN);
    innerArea.removeFromBottom(MARGIN);
    auto mappingArea = innerArea.removeFromBottom(mappingPanelHeight);
    innerArea.removeFromBottom(MARGIN);

    // Guarantee a minimum playable XY surface by stealing space from mapping when needed.
    constexpr int minPadHeight = 170;
    if (innerArea.getHeight() < minPadHeight && mappingArea.getHeight() > 120)
    {
        const int missing = minPadHeight - innerArea.getHeight();
        const int transferable = juce::jmax(0, mappingArea.getHeight() - 120);
        const int shift = juce::jmin(missing, transferable);
        if (shift > 0)
        {
            auto donated = mappingArea.removeFromTop(shift);
            innerArea = innerArea.withBottom(innerArea.getBottom() + donated.getHeight());
        }
    }

    auto toggleArea = bottomControls.removeFromTop(toggleHeight);
    bottomControls.removeFromTop(MARGIN);
    auto midiRow = bottomControls;
    auto xyPadArea = innerArea;
    if (xyPad)
        xyPad->setBounds(xyPadArea);
    if (xyMappingPanel)
        xyMappingPanel->setBounds(mappingArea);

    xyMidiLinkToggle.setBounds(toggleArea.removeFromLeft(132));

    // MIDI Sampler controls (3 small sliders)
    int midiKnobWidth = (midiRow.getWidth() - (columnGap * 2)) / 3;
    
    auto rootNoteArea = midiRow.removeFromLeft(midiKnobWidth);
    rootNoteArea.removeFromTop(labelReserve - 4);
    rootNoteSlider.setBounds(rootNoteArea.withSizeKeepingCentre(knobSize, knobSize));
    
    midiRow.removeFromLeft(columnGap);
    auto fineTuneArea = midiRow.removeFromLeft(midiKnobWidth);
    fineTuneArea.removeFromTop(labelReserve - 4);
    fineTuneSlider.setBounds(fineTuneArea.withSizeKeepingCentre(knobSize, knobSize));
    
    midiRow.removeFromLeft(columnGap);
    auto pbRangeArea = midiRow;
    pbRangeArea.removeFromTop(labelReserve - 4);
    pitchBendRangeSlider.setBounds(pbRangeArea.withSizeKeepingCentre(knobSize, knobSize));
    
}

void NativePluginEditor::applyXYPadMappings(juce::Point<float> normalizedPosition)
{
    auto& apvts = audioProcessor.getValueTreeState();
    const float x = juce::jlimit(0.0f, 1.0f, normalizedPosition.x);
    const float y = juce::jlimit(0.0f, 1.0f, normalizedPosition.y);

    bool hasActiveMapping = false;

    for (int slotIndex = 1; slotIndex <= 4; ++slotIndex)
    {
        const auto prefix = "xySlot" + juce::String(slotIndex);

        auto* targetChoice = apvts.getRawParameterValue(prefix + "Target");
        auto* axisValue = apvts.getRawParameterValue(prefix + "Axis");
        auto* minValue = apvts.getRawParameterValue(prefix + "Min");
        auto* maxValue = apvts.getRawParameterValue(prefix + "Max");
        auto* invertValue = apvts.getRawParameterValue(prefix + "Invert");

        if (targetChoice == nullptr || axisValue == nullptr || minValue == nullptr || maxValue == nullptr || invertValue == nullptr)
            continue;

        const int targetIndex = juce::roundToInt(targetChoice->load());
        const auto targetParamId = getXYTargetParameterId(targetIndex);
        if (targetParamId.isEmpty())
            continue;

        auto* targetParam = apvts.getParameter(targetParamId);
        if (targetParam == nullptr)
            continue;

        hasActiveMapping = true;

        float sourceValue = juce::roundToInt(axisValue->load()) == 0 ? x : y;
        if (invertValue->load() > 0.5f)
            sourceValue = 1.0f - sourceValue;

        const float mappedValue = juce::jmap(sourceValue, minValue->load(), maxValue->load());
        targetParam->setValueNotifyingHost(juce::jlimit(0.0f, 1.0f, mappedValue));
    }

    // Fallback so XY always does something audible even when all slots are "none".
    if (!hasActiveMapping)
    {
        if (auto* positionParam = apvts.getParameter("position"))
            positionParam->setValueNotifyingHost(x);

        if (auto* reverbParam = apvts.getParameter("reverbWet"))
            reverbParam->setValueNotifyingHost(y);
    }

    if (xyMidiLinkToggle.getToggleState())
    {
        if (auto* rootNoteParam = apvts.getParameter("rootNote"))
            rootNoteParam->setValueNotifyingHost(x);

        if (auto* fineTuneParam = apvts.getParameter("fineTuneCents"))
            fineTuneParam->setValueNotifyingHost(y);

        const float dx = x - 0.5f;
        const float dy = y - 0.5f;
        const float radial = juce::jlimit(0.0f, 1.0f, std::sqrt(dx * dx + dy * dy) * 1.41421356f);
        if (auto* pbRangeParam = apvts.getParameter("pitchBendRange"))
            pbRangeParam->setValueNotifyingHost(radial);
    }

}

//==============================================================================
void NativePluginEditor::timerCallback()
{
    // Update grain seed position cursor (yellow dashed line)
    if (waveformView)
        waveformView->setGrainSeedPosition((float)positionSlider.getValue());
    if (xyMappingNeedsUpdate || xySmoothedX.isSmoothing() || xySmoothedY.isSmoothing())
    {
        const juce::Point<float> smoothedPosition {
            xySmoothedX.getNextValue(),
            xySmoothedY.getNextValue()
        };

        const bool hasMeaningfulChange = std::abs(smoothedPosition.x - xyLastAppliedPosition.x) > 0.0005f
                                       || std::abs(smoothedPosition.y - xyLastAppliedPosition.y) > 0.0005f;

        if (hasMeaningfulChange)
        {
            applyXYPadMappings(smoothedPosition);
            xyLastAppliedPosition = smoothedPosition;
        }

        if (!xySmoothedX.isSmoothing() && !xySmoothedY.isSmoothing())
            xyMappingNeedsUpdate = false;
    }

    // Recover from host-side editor size quirks that can leave controls at 0x0.
    if (grainSizeSlider.getWidth() <= 0 || grainSizeSlider.getHeight() <= 0)
        resized();

    // Update sample name if changed
    if (audioProcessor.getSampleName() != sampleNameLabel.getText())
        sampleNameLabel.setText(audioProcessor.getSampleName(), juce::dontSendNotification);

    // Poll for newly-learned MIDI CC mappings and refresh paint
    {
        juce::String learnedParam;
        int learnedCC = -1;
        if (audioProcessor.getMidiLearnManager().pollNewMapping (learnedParam, learnedCC))
            repaint(); // refresh CC badges
    }

    // Poll peak level (exponential decay on UI side so meter falls smoothly)
    {
        const float newPeak = audioProcessor.getPeakLevel();
        currentPeakLevel = std::max(newPeak, currentPeakLevel * 0.85f);
        repaint(peakMeterBounds);
    }

    // Keep repainting while in learn mode (pulsing animation)
    if (audioProcessor.getMidiLearnManager().isLearning())
        repaint();
}

void NativePluginEditor::refreshPresetControls()
{
    const auto selectedName = presetCombo.getText();
    const auto persistedName = audioProcessor.getSelectedPresetName();
    const auto preferredName = selectedName.isNotEmpty() ? selectedName : persistedName;
    const auto selectedCategory = presetCategoryCombo.getText().isEmpty() ? juce::String("All") : presetCategoryCombo.getText();
    const bool favoritesOnly = presetFavoritesOnlyToggle.getToggleState();

    presetCombo.clear(juce::dontSendNotification);

    const auto names = presetManager.getPresetNames(selectedCategory, favoritesOnly);
    for (int i = 0; i < names.size(); ++i)
        presetCombo.addItem(names[i], i + 1);

    const auto categories = presetManager.getCategories();
    const auto selectedCategoryId = presetCategoryCombo.getSelectedId();
    presetCategoryCombo.clear(juce::dontSendNotification);
    for (int i = 0; i < categories.size(); ++i)
        presetCategoryCombo.addItem(categories[i], i + 1);
    if (selectedCategoryId > 0 && selectedCategoryId <= presetCategoryCombo.getNumItems())
        presetCategoryCombo.setSelectedId(selectedCategoryId, juce::dontSendNotification);
    else
        presetCategoryCombo.setSelectedId(1, juce::dontSendNotification);

    int restoredId = 0;
    for (int i = 0; i < presetCombo.getNumItems(); ++i)
    {
        if (presetCombo.getItemText(i) == preferredName)
        {
            restoredId = presetCombo.getItemId(i);
            break;
        }
    }

    if (restoredId > 0)
        presetCombo.setSelectedId(restoredId, juce::dontSendNotification);
    else if (presetCombo.getNumItems() > 0)
        presetCombo.setSelectedId(1, juce::dontSendNotification);

    const auto current = presetCombo.getText();
    if (current.isNotEmpty())
    {
        presetFavoriteToggle.setToggleState(presetManager.isPresetFavorite(current), juce::dontSendNotification);
        presetNameEditor.setText(current, juce::dontSendNotification);
        audioProcessor.setSelectedPresetName(current);
    }
    else
    {
        audioProcessor.setSelectedPresetName("Init Empty");
    }
}

void NativePluginEditor::savePresetsToDisk()
{
    presetManager.savePresetsToFile(presetManager.getDefaultPresetsFile());
}

void NativePluginEditor::loadSelectedPreset()
{
    const auto name = presetCombo.getText();
    if (name.isEmpty())
        return;

    audioProcessor.setSelectedPresetName(name);

    juce::ValueTree state;
    if (presetManager.loadPreset(name, state))
    {
        auto& apvts = audioProcessor.getValueTreeState();

        // User presets saved from APVTS contain parameter child nodes.
        if (state.getNumChildren() > 0)
        {
            apvts.replaceState(state);

            // Restore sample markers if saved
            if (state.hasProperty("sampleRangeStart") && state.hasProperty("sampleRangeEnd"))
            {
                const float s = static_cast<float>(state.getProperty("sampleRangeStart"));
                const float e = static_cast<float>(state.getProperty("sampleRangeEnd"));
                if (waveformView) waveformView->setSampleRange(s, e);
                if (auto* eng = audioProcessor.getGrainEngine()) eng->setSampleRange(s, e);
            }

            return;
        }

        // Legacy/factory flat presets are stored as properties.
        // Apply each property directly to APVTS parameters so knobs update reliably.
        auto applyPropertyToParameter = [&apvts, &state](const juce::String& propertyId,
                                                         const juce::String& parameterId)
        {
            if (!state.hasProperty(propertyId))
                return;

            if (auto* param = apvts.getParameter(parameterId))
            {
                const float rawValue = static_cast<float>(state.getProperty(propertyId));
                param->beginChangeGesture();
                param->setValueNotifyingHost(param->convertTo0to1(rawValue));
                param->endChangeGesture();
            }
        };

        applyPropertyToParameter("grainSize", "grainSize");
        applyPropertyToParameter("density", "density");
        applyPropertyToParameter("position", "position");
        applyPropertyToParameter("positionSpread", "positionSpread");
        applyPropertyToParameter("pitch", "pitch");
        applyPropertyToParameter("pitchSpread", "pitchSpread");
        applyPropertyToParameter("reverse", "reverse");
        applyPropertyToParameter("pan", "pan");
        applyPropertyToParameter("panSpread", "panSpread");

        applyPropertyToParameter("adsrAttack", "adsrAttack");
        applyPropertyToParameter("adsrDecay", "adsrDecay");
        applyPropertyToParameter("adsrSustain", "adsrSustain");
        applyPropertyToParameter("adsrRelease", "adsrRelease");
        applyPropertyToParameter("attack", "adsrAttack");   // legacy mapping
        applyPropertyToParameter("release", "adsrRelease"); // legacy mapping

        applyPropertyToParameter("positionLfoFreq", "positionLfoFreq");
        applyPropertyToParameter("positionLfoDepth", "positionLfoDepth");
        applyPropertyToParameter("pitchLfoFreq", "pitchLfoFreq");
        applyPropertyToParameter("pitchLfoDepth", "pitchLfoDepth");
        applyPropertyToParameter("densityLfoFreq", "densityLfoFreq");
        applyPropertyToParameter("densityLfoDepth", "densityLfoDepth");

        applyPropertyToParameter("reverbRoom", "reverbRoom");
        applyPropertyToParameter("reverbDamping", "reverbDamping");
        applyPropertyToParameter("reverbWet", "reverbWet");
        applyPropertyToParameter("formantFreq", "formantFreq");
        applyPropertyToParameter("formantMix", "formantMix");

        applyPropertyToParameter("rootNote", "rootNote");
        applyPropertyToParameter("fineTuneCents", "fineTuneCents");
        applyPropertyToParameter("pitchBendRange", "pitchBendRange");
        applyPropertyToParameter("maxActiveGrains", "maxActiveGrains");
        applyPropertyToParameter("cpuMode", "cpuMode");

        // Restore sample markers if saved in legacy preset
        if (state.hasProperty("sampleRangeStart") && state.hasProperty("sampleRangeEnd"))
        {
            const float s = static_cast<float>(state.getProperty("sampleRangeStart"));
            const float e = static_cast<float>(state.getProperty("sampleRangeEnd"));
            if (waveformView) waveformView->setSampleRange(s, e);
            if (auto* eng = audioProcessor.getGrainEngine()) eng->setSampleRange(s, e);
        }
    }
}

void NativePluginEditor::saveCurrentPresetAs()
{
    auto name = presetNameEditor.getText().trim();
    if (name.isEmpty())
        name = "User Preset " + juce::String(presetManager.getNumPresets() + 1);

    if (name.isEmpty())
        return;

    const auto category = presetCategoryCombo.getText().isEmpty() ? juce::String("Uncategorized") : presetCategoryCombo.getText();
    auto savedState = audioProcessor.getValueTreeState().copyState();
    // Persist marker positions alongside the APVTS state
    if (waveformView)
    {
        savedState.setProperty("sampleRangeStart", waveformView->getSampleStartNorm(), nullptr);
        savedState.setProperty("sampleRangeEnd",   waveformView->getSampleEndNorm(),   nullptr);
    }
    presetManager.savePreset(name,
                             savedState,
                             category,
                             presetFavoriteToggle.getToggleState());
    savePresetsToDisk();
    refreshPresetControls();

    int matchingId = 0;
    for (int i = 0; i < presetCombo.getNumItems(); ++i)
    {
        if (presetCombo.getItemText(i) == name)
        {
            matchingId = presetCombo.getItemId(i);
            break;
        }
    }

    if (matchingId > 0)
        presetCombo.setSelectedId(matchingId, juce::sendNotificationSync);
    else
        presetNameEditor.setText(name, juce::dontSendNotification);
}

void NativePluginEditor::renameSelectedPreset()
{
    const int selectedId = presetCombo.getSelectedId();
    const int selectedIndex = presetCombo.getSelectedItemIndex();
    if (selectedId <= 0 || selectedIndex < 0)
        return;

    const auto oldName = presetCombo.getItemText(selectedIndex);
    if (oldName.isEmpty())
        return;

    const auto newName = presetNameEditor.getText().trim();
    if (newName.isEmpty() || newName == oldName)
        return;

    if (presetManager.renamePreset(oldName, newName))
    {
        savePresetsToDisk();
        refreshPresetControls();

        int matchingId = 0;
        for (int i = 0; i < presetCombo.getNumItems(); ++i)
        {
            if (presetCombo.getItemText(i) == newName)
            {
                matchingId = presetCombo.getItemId(i);
                break;
            }
        }

        if (matchingId > 0)
            presetCombo.setSelectedId(matchingId, juce::sendNotificationSync);
        else
            presetNameEditor.setText(newName, juce::dontSendNotification);
    }
}

void NativePluginEditor::deleteSelectedPreset()
{
    const auto name = presetCombo.getText();
    if (name.isEmpty())
        return;

    const auto options = juce::MessageBoxOptions::makeOptionsOkCancel(
        juce::MessageBoxIconType::WarningIcon,
        "Delete Preset",
        "Delete preset '" + name + "'?",
        "Delete",
        "Cancel",
        this);

    juce::AlertWindow::showAsync(options, [safeThis = juce::Component::SafePointer<NativePluginEditor>(this), name](int result)
    {
        if (result == 0 || safeThis == nullptr)
            return;

        safeThis->presetManager.deletePreset(name);
        safeThis->savePresetsToDisk();
        safeThis->refreshPresetControls();
    });
}

void NativePluginEditor::applySectionReset(const juce::String& sectionName)
{
    auto& apvts = audioProcessor.getValueTreeState();

    auto resetParams = [&](std::initializer_list<const char*> ids)
    {
        for (const auto* id : ids)
        {
            if (auto* param = apvts.getParameter(id))
                param->setValueNotifyingHost(param->getDefaultValue());
        }
    };

    if (sectionName == "GRANULAR")
    {
        resetParams({"grainSize", "density", "position", "positionSpread", "pitch", "pitchSpread", "reverse", "pan", "panSpread"});
    }
    else if (sectionName == "ADSR")
    {
        resetParams({"adsrAttack", "adsrDecay", "adsrSustain", "adsrRelease"});
    }
    else if (sectionName == "LFO")
    {
        resetParams({"positionLfoFreq", "positionLfoDepth", "pitchLfoFreq", "pitchLfoDepth", "densityLfoFreq", "densityLfoDepth"});
    }
    else if (sectionName == "EFFECTS")
    {
        resetParams({"reverbRoom", "reverbDamping", "reverbWet", "formantFreq", "formantMix"});
    }
    else if (sectionName == "XY/MIDI")
    {
        resetParams({"xySlot1Target", "xySlot1Axis", "xySlot1Min", "xySlot1Max", "xySlot1Invert",
                     "xySlot2Target", "xySlot2Axis", "xySlot2Min", "xySlot2Max", "xySlot2Invert",
                     "xySlot3Target", "xySlot3Axis", "xySlot3Min", "xySlot3Max", "xySlot3Invert",
                     "xySlot4Target", "xySlot4Axis", "xySlot4Min", "xySlot4Max", "xySlot4Invert",
                     "rootNote", "fineTuneCents", "pitchBendRange"});

        if (xyPad)
            xyPad->resetToCenter(false);

        xyTargetPosition = { 0.5f, 0.5f };
        xyLastAppliedPosition = xyTargetPosition;
        xySmoothedX.setCurrentAndTargetValue(xyTargetPosition.x);
        xySmoothedY.setCurrentAndTargetValue(xyTargetPosition.y);
        xyMappingNeedsUpdate = false;
    }
}

//==============================================================================
void NativePluginEditor::loadSampleFile()
{
    fileChooser = std::make_unique<juce::FileChooser>(
        "Load Audio Sample",
        juce::File{},
        "*.wav;*.mp3;*.aif;*.aiff;*.flac;*.ogg"
    );
    
    auto browserFlags = juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles;
    
    fileChooser->launchAsync(browserFlags, [this](const juce::FileChooser& chooser)
    {
        auto file = chooser.getResult();
        if (isSupportedSampleFile(file))
        {
            audioProcessor.loadSample(file);
            sampleNameLabel.setText(audioProcessor.getSampleName(), juce::dontSendNotification);
            if (waveformView)
            {
                waveformView->setSource(new juce::FileInputSource(file));
                waveformView->repaint();
            }
        }
    });
}

bool NativePluginEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (isSupportedSampleFile(juce::File(path)))
            return true;

    return false;
}

void NativePluginEditor::fileDragEnter(const juce::StringArray& files, int, int)
{
    if (isInterestedInFileDrag(files))
    {
        isDraggingSample = true;
        repaint();
    }
}

void NativePluginEditor::fileDragExit(const juce::StringArray&)
{
    isDraggingSample = false;
    repaint();
}

void NativePluginEditor::filesDropped(const juce::StringArray& files, int, int)
{
    isDraggingSample = false;
    if (tryLoadDroppedSample(files))
        repaint();
}

bool NativePluginEditor::tryLoadDroppedSample(const juce::StringArray& files)
{
    for (const auto& path : files)
    {
        const juce::File file(path);

        if (!isSupportedSampleFile(file))
            continue;

        audioProcessor.loadSample(file);
        sampleNameLabel.setText(audioProcessor.getSampleName(), juce::dontSendNotification);

        if (waveformView)
        {
            waveformView->setSource(new juce::FileInputSource(file));
            waveformView->repaint();
        }

        return true;
    }

    return false;
}
