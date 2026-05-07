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
    titleLabel.setColour(juce::Label::textColourId, magentaColor);
    titleLabel.setJustificationType(juce::Justification::centredLeft);
    
    addAndMakeVisible(subtitleLabel);
    subtitleLabel.setText("Solar Bumper's Ethereal Granular Instrument", juce::dontSendNotification);
    subtitleLabel.setFont(juce::Font(juce::FontOptions(13.0f, juce::Font::plain)));
    subtitleLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.75f));
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
    sampleNameLabel.setColour(juce::Label::textColourId, juce::Colours::white.withAlpha(0.8f));
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
    
    // Row 1: Grain Size + Density
    setupSlider(grainSizeSlider, grainSizeLabel, "GRAIN SIZE", 
                juce::NormalisableRange<double>(10.0, 600.0, 1.0), 100.0);
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

    // Master Gain (sous la Formant Mix)
    setupSlider(masterGainSlider, masterGainLabel, "MASTER GAIN", juce::NormalisableRange<double>(0.0, 2.0, 0.01), 1.0);

    setupSlider(glitchIntensitySlider, glitchIntensityLabel, "GLITCH INT",
                juce::NormalisableRange<double>(0.0, 1.0, 0.01), 0.0);
    setupSlider(glitchRateSlider, glitchRateLabel, "GLITCH RATE",
                juce::NormalisableRange<double>(0.5, 20.0, 0.1), 4.0);
    
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
    auto& apvts = audioProcessor.getValueTreeState();
    
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "grainSize", grainSizeSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "density", densitySlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "position", positionSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "positionSpread", positionSpreadSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "pitch", pitchSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "pitchSpread", pitchSpreadSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "reverse", reverseSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "pan", panSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "panSpread", panSpreadSlider));
    
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "positionLfoFreq", positionLfoFreqSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "positionLfoDepth", positionLfoDepthSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "pitchLfoFreq", pitchLfoFreqSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "pitchLfoDepth", pitchLfoDepthSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "densityLfoFreq", densityLfoFreqSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "densityLfoDepth", densityLfoDepthSlider));
    
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "reverbRoom", reverbRoomSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "reverbDamping", reverbDampingSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "reverbWet", reverbWetSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "formantFreq", formantFreqSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "formantMix", formantMixSlider));
    
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "rootNote", rootNoteSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "fineTuneCents", fineTuneSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "pitchBendRange", pitchBendRangeSlider));
    // Master Gain attachment
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "masterGain", masterGainSlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "glitchIntensity", glitchIntensitySlider));
    sliderAttachments.push_back(std::make_unique<SliderAttachment>(apvts, "glitchRate", glitchRateSlider));
    maxGrainsAttachment = std::make_unique<SliderAttachment>(apvts, "maxActiveGrains", maxGrainsSlider);
    cpuModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "cpuMode", cpuModeCombo);

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
    const float totalRatio = 2.5f + 1.5f + 1.5f + 2.0f;
    const float contentWidth = static_cast<float>(getWidth() - MARGIN * 2 - MARGIN * 3);
    const float left = static_cast<float>(MARGIN);
    const float col1 = contentWidth * (2.5f / totalRatio);
    const float col2 = contentWidth * (1.5f / totalRatio);
    const float col3 = contentWidth * (1.5f / totalRatio);
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
        juce::ColourGradient glow(accent.withAlpha(0.08f), rect.getCentreX(), rect.getY(),
                                  juce::Colours::transparentBlack, rect.getCentreX(), rect.getBottom(), false);
        g.setGradientFill(glow);
        g.fillRoundedRectangle(rect.reduced(1.0f, 4.0f), 16.0f);
    }
    
    // Header background (magenta)
    auto headerArea = getLocalBounds().removeFromTop(HEADER_HEIGHT);
    juce::ColourGradient headerGradient(
        juce::Colour::fromString("#28324A").withAlpha(0.65f), headerArea.getTopLeft().toFloat(),
        juce::Colour::fromString("#1A2234").withAlpha(0.9f), headerArea.getBottomRight().toFloat(), false
    );
    g.setGradientFill(headerGradient);
    g.fillRect(headerArea);
    
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

    titleLabel.setBounds(titleArea.removeFromTop(32));
    subtitleLabel.setBounds(titleArea.removeFromTop(18));
    titleArea.removeFromTop(4);
    sampleNameLabel.setBounds(titleArea.removeFromTop(20));

    auto controlsRow1 = headerArea.removeFromTop(28);
    const int browseW = compactHeader ? 92 : 98;
    const int cpuModeW = compactHeader ? 70 : 78;
    const int maxGrainsW = compactHeader ? 200 : 240;

    loadSampleButton.setBounds(controlsRow1.removeFromLeft(browseW));
    controlsRow1.removeFromLeft(6);
    cpuModeCombo.setBounds(controlsRow1.removeFromLeft(cpuModeW));
    controlsRow1.removeFromLeft(6);
    auto maxGrainsArea = controlsRow1.removeFromLeft(maxGrainsW);
    maxGrainsLabel.setBounds(maxGrainsArea.removeFromTop(14));
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
    
    // Calculate column widths (ratios: 2.5 : 1.5 : 1.5 : 2.0)
    float totalRatio = 2.5f + 1.5f + 1.5f + 2.0f;
    int totalWidth = contentArea.getWidth() - (3 * columnGap);
    
    int col1Width = static_cast<int>(totalWidth * (2.5f / totalRatio));
    int col2Width = static_cast<int>(totalWidth * (1.5f / totalRatio));
    int col3Width = static_cast<int>(totalWidth * (1.5f / totalRatio));
    // col4Width uses remaining space (calculated implicitly in layout)
    
    //==========================================================================
    // COLUMN 1: CYAN - Granular Controls
    //==========================================================================
    auto col1Area = contentArea.removeFromLeft(col1Width);
    grainGroup.setBounds(col1Area);
    resetGranularButton.setBounds(col1Area.getRight() - 76, col1Area.getY() + 3, 70, 18);
    
    auto innerArea = col1Area.reduced(MARGIN * 2);
    innerArea.removeFromTop(groupLabelHeight);

    auto adsrHeight = juce::jlimit(132, 186, innerArea.getHeight() / 2);
    auto adsrArea = innerArea.removeFromBottom(adsrHeight);
    innerArea.removeFromBottom(MARGIN);
    resetAdsrButton.setBounds(adsrArea.getRight() - 76, adsrArea.getY() + 2, 70, 18);

    const int granularColWidth = (innerArea.getWidth() - (2 * columnGap)) / 3;
    const int granularRowHeight = innerArea.getHeight() / 3;
    const int granularSizeFromHeight = (innerArea.getHeight() / 3) - labelReserve - 4;
    const int granularSizeFromWidth = granularColWidth - 6;
    const int granularCapacity = juce::jmin(granularSizeFromHeight, granularSizeFromWidth);
    const int granularSliderSize = juce::jlimit(46, knobSize, granularCapacity);

    auto placeSlider = [&](juce::Rectangle<int> cell, juce::Slider& slider)
    {
        cell.removeFromTop(labelReserve);
        slider.setBounds(cell.withSizeKeepingCentre(granularSliderSize, granularSliderSize));
    };

    // Row 1: Grain Size / Density / Reverse
    auto row1 = innerArea.removeFromTop(granularRowHeight);
    auto grainSizeArea = row1.removeFromLeft(granularColWidth);
    placeSlider(grainSizeArea, grainSizeSlider);
    row1.removeFromLeft(columnGap);
    auto densityArea = row1.removeFromLeft(granularColWidth);
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

    auto lfoMonitorHeight = juce::jlimit(96, 150, innerArea.getHeight() / 3);
    auto lfoMonitorArea = innerArea.removeFromBottom(lfoMonitorHeight);
    innerArea.removeFromBottom(MARGIN);

    int sliderSize = knobSize;
    int rowHeight = innerArea.getHeight() / 3;
    
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

    sliderSize = knobSize;

    const int effectControlRowHeight = sliderSize + labelReserve + 24;
    auto effectRowsArea = innerArea.removeFromTop(2 * effectControlRowHeight + MARGIN * 2);

    // Reverb controls (3 knobs - Room, Damping, Wet)
    auto reverbRow = effectRowsArea.removeFromTop(effectControlRowHeight);
    int knobWidth = (reverbRow.getWidth() - (columnGap * 2)) / 3;
    
    auto roomArea = reverbRow.removeFromLeft(knobWidth);
    roomArea.removeFromTop(labelReserve);
    reverbRoomSlider.setBounds(roomArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    reverbRow.removeFromLeft(columnGap);
    auto dampingArea = reverbRow.removeFromLeft(knobWidth);
    dampingArea.removeFromTop(labelReserve);
    reverbDampingSlider.setBounds(dampingArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    reverbRow.removeFromLeft(columnGap);
    auto wetArea = reverbRow;
    wetArea.removeFromTop(labelReserve);
    reverbWetSlider.setBounds(wetArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    effectRowsArea.removeFromTop(MARGIN * 2);

    // Formant controls (2 knobs)
    auto formantRow = effectRowsArea.removeFromTop(effectControlRowHeight);
    auto formantFreqArea = formantRow.removeFromLeft((formantRow.getWidth() - columnGap) / 2);
    formantFreqArea.removeFromTop(labelReserve);
    formantFreqSlider.setBounds(formantFreqArea.withSizeKeepingCentre(sliderSize, sliderSize));
    
    formantRow.removeFromLeft(columnGap);
    auto formantMixArea = formantRow;
    formantMixArea.removeFromTop(labelReserve);
    formantMixSlider.setBounds(formantMixArea.withSizeKeepingCentre(sliderSize, sliderSize));

    // Master Gain — toujours affiché, centré dans une rangée dédiée
    {
        innerArea.removeFromTop(MARGIN);
        auto masterRow = innerArea.removeFromTop(effectControlRowHeight);
        auto masterArea = masterRow.withSizeKeepingCentre(sliderSize * 2, masterRow.getHeight());
        masterArea.removeFromTop(labelReserve);
        masterGainSlider.setBounds(masterArea.withSizeKeepingCentre(sliderSize, sliderSize));
    }

    // Glitch controls (2 knobs) — dans l'espace restant, si disponible
    if (innerArea.getHeight() >= effectControlRowHeight)
    {
        innerArea.removeFromTop(MARGIN);
        auto glitchRow = innerArea.removeFromTop(effectControlRowHeight);
        auto glitchIntArea = glitchRow.removeFromLeft((glitchRow.getWidth() - columnGap) / 2);
        glitchIntArea.removeFromTop(labelReserve);
        glitchIntensitySlider.setBounds(glitchIntArea.withSizeKeepingCentre(sliderSize, sliderSize));
        glitchRow.removeFromLeft(columnGap);
        auto glitchRateArea = glitchRow;
        glitchRateArea.removeFromTop(labelReserve);
        glitchRateSlider.setBounds(glitchRateArea.withSizeKeepingCentre(sliderSize, sliderSize));
    }

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
