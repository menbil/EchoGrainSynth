#include "PluginProcessor.h"
#include "NativePluginEditor.h"  // Changed from React-JUCE to Native JUCE UI
#include "Synth/GranularVoice.h"
#include "Synth/GranularSound.h"

//==============================================================================
EchoGrainSynthAudioProcessor::EchoGrainSynthAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)
                     #endif
                       ),
       apvts(*this, nullptr, "Parameters", createParameterLayout())
#else
     : apvts(*this, nullptr, "Parameters", createParameterLayout())
#endif
{
    formatManager.registerBasicFormats();
    grainEngine = std::make_unique<GrainEngine>();
    
    // Initialize synthesizer with granular voices for MIDI input
    for (int i = 0; i < 16; ++i)
        synthesiser.addVoice(new GranularVoice());
        
    // Add a granular sound to the synthesizer
    synthesiser.addSound(new GranularSound());
        
    // Initialize recording buffer (10 seconds max at 44.1kHz)
    recordingBuffer.setSize(2, static_cast<int>(44100 * 10));
    recordingSamplePosition = 0;
    
    // Initialize sidechain buffer
    sidechainBuffer.setSize(2, 4096);
}

EchoGrainSynthAudioProcessor::~EchoGrainSynthAudioProcessor()
{
}

//==============================================================================
const juce::String EchoGrainSynthAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool EchoGrainSynthAudioProcessor::acceptsMidi() const
{
   #if JucePlugin_WantsMidiInput
    return true;
   #else
    return false;
   #endif
}

bool EchoGrainSynthAudioProcessor::producesMidi() const
{
   #if JucePlugin_ProducesMidiOutput
    return true;
   #else
    return false;
   #endif
}

bool EchoGrainSynthAudioProcessor::isMidiEffect() const
{
   #if JucePlugin_IsMidiEffect
    return true;
   #else
    return false;
   #endif
}

double EchoGrainSynthAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int EchoGrainSynthAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int EchoGrainSynthAudioProcessor::getCurrentProgram()
{
    return 0;
}

void EchoGrainSynthAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused(index);
}

const juce::String EchoGrainSynthAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused(index);
    return {};
}

void EchoGrainSynthAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void EchoGrainSynthAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    grainEngine->prepareToPlay(sampleRate, samplesPerBlock);
    synthesiser.setCurrentPlaybackSampleRate(sampleRate);

    smoothedReverbWet.reset(sampleRate, 0.03);
    smoothedFormantMix.reset(sampleRate, 0.02);

    const float initialReverbWet = (apvts.getRawParameterValue("reverbWet") != nullptr)
        ? apvts.getRawParameterValue("reverbWet")->load()
        : 0.0f;
    smoothedReverbWet.setCurrentAndTargetValue(initialReverbWet);

    const float initialFormantMix = (apvts.getRawParameterValue("formantMix") != nullptr)
        ? apvts.getRawParameterValue("formantMix")->load()
        : 0.0f;
    smoothedFormantMix.setCurrentAndTargetValue(initialFormantMix);
    
    // Prepare effects
    reverbEffect.prepare(sampleRate, samplesPerBlock);
    formantFilter.prepare(sampleRate, samplesPerBlock);
    stretchEffect.prepare(sampleRate, samplesPerBlock);
}

void EchoGrainSynthAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool EchoGrainSynthAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // Support various configurations for sidechain and effects
    auto mainOutput = layouts.getMainOutputChannelSet();
    
    // Main output must be mono or stereo
    if (mainOutput != juce::AudioChannelSet::mono() && mainOutput != juce::AudioChannelSet::stereo())
        return false;

    // Check main input
    auto mainInput = layouts.getMainInputChannelSet();
    
    // Allow no input (synth mode), mono, or stereo input
    if (mainInput != juce::AudioChannelSet::disabled() &&
        mainInput != juce::AudioChannelSet::mono() &&
        mainInput != juce::AudioChannelSet::stereo())
        return false;

    // Check sidechain input if present
    if (layouts.inputBuses.size() > 1)
    {
        auto sidechainInput = layouts.getChannelSet(true, 1);
        if (sidechainInput != juce::AudioChannelSet::disabled() &&
            sidechainInput != juce::AudioChannelSet::mono() &&
            sidechainInput != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
  #endif
}
#endif

void EchoGrainSynthAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear any output channels that don't contain input data
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    const int numSamples = buffer.getNumSamples();
    const double sampleRateForProcessing = getSampleRate();

    // Merge editor keyboard MIDI so notes can be played directly from the plugin UI.
    keyboardState.processNextMidiBuffer(midiMessages, 0, numSamples, true);

    // Update host info for tempo sync
    updateHostInfo();

    // Handle sidechain input if available
    if (getBusesLayout().inputBuses.size() > 1)
    {
        auto sidechainBus = getBusBuffer(buffer, true, 1);
        if (sidechainBus.getNumChannels() > 0)
        {
            processSidechain(sidechainBus);
        }
    }

    // Record audio if recording is active
    if (isRecording)
    {
        recordAudio(buffer);
    }

    // Update LFOs and apply modulation
    if (auto* positionParam = apvts.getRawParameterValue("position"))
    {
        float lfoMod = positionLFO.getNextValue(static_cast<float>(sampleRateForProcessing));
        float modifiedPosition = juce::jlimit(0.0f, 1.0f, *positionParam + lfoMod);
        grainEngine->setPosition(modifiedPosition);
    }
    
    if (auto* pitchParam = apvts.getRawParameterValue("pitch"))
    {
        float lfoMod = pitchLFO.getNextValue(static_cast<float>(sampleRateForProcessing));
        float modifiedPitch = juce::jlimit(0.1f, 4.0f, *pitchParam + lfoMod);
        grainEngine->setPitch(modifiedPitch);
    }
    
    if (auto* densityParam = apvts.getRawParameterValue("density"))
    {
        float lfoMod = densityLFO.getNextValue(static_cast<float>(sampleRateForProcessing));
        const bool ecoMode = (apvts.getRawParameterValue("cpuMode") != nullptr)
                             ? juce::roundToInt(apvts.getRawParameterValue("cpuMode")->load()) == 0
                             : false;
        const float densityCap = ecoMode ? 18.0f : 30.0f;
        const float lfoAmount = ecoMode ? 2.0f : 3.0f;
        float modifiedDensity = juce::jlimit(0.1f, densityCap, *densityParam + lfoMod * lfoAmount);
        grainEngine->setGrainDensity(modifiedDensity);
    }

    if (auto* maxGrainsParam = apvts.getRawParameterValue("maxActiveGrains"))
        grainEngine->setMaxActiveGrains(juce::roundToInt(maxGrainsParam->load()));

    if (auto* cpuModeParam = apvts.getRawParameterValue("cpuMode"))
        grainEngine->setEcoMode(juce::roundToInt(cpuModeParam->load()) == 0);

    // Update other grain engine parameters from APVTS - UPDATED FOR ADSR
    if (auto* grainSizeParam = apvts.getRawParameterValue("grainSize"))
        grainEngine->setGrainSize(*grainSizeParam);
    if (auto* positionSpreadParam = apvts.getRawParameterValue("positionSpread"))
        grainEngine->setPositionSpread(*positionSpreadParam);
    if (auto* pitchSpreadParam = apvts.getRawParameterValue("pitchSpread"))
        grainEngine->setPitchSpread(*pitchSpreadParam);
    if (auto* reverseParam = apvts.getRawParameterValue("reverse"))
        grainEngine->setReverse(*reverseParam);
    if (auto* panParam = apvts.getRawParameterValue("pan"))
        grainEngine->setPan(*panParam);
    if (auto* panSpreadParam = apvts.getRawParameterValue("panSpread"))
        grainEngine->setPanSpread(*panSpreadParam);
    
    // UPDATE: ADSR parameters instead of attack/release
    if (auto* adsrAttackParam = apvts.getRawParameterValue("adsrAttack"))
        grainEngine->setADSRAttack(*adsrAttackParam);
    if (auto* adsrDecayParam = apvts.getRawParameterValue("adsrDecay"))
        grainEngine->setADSRDecay(*adsrDecayParam);
    if (auto* adsrSustainParam = apvts.getRawParameterValue("adsrSustain"))
        grainEngine->setADSRSustain(*adsrSustainParam);
    if (auto* adsrReleaseParam = apvts.getRawParameterValue("adsrRelease"))
        grainEngine->setADSRRelease(*adsrReleaseParam);

    // Update LFO parameters
    if (auto* posLfoFreqParam = apvts.getRawParameterValue("positionLfoFreq"))
        positionLFO.frequency = *posLfoFreqParam;
    if (auto* posLfoDepthParam = apvts.getRawParameterValue("positionLfoDepth"))
        positionLFO.depth = *posLfoDepthParam;
    if (auto* pitchLfoFreqParam = apvts.getRawParameterValue("pitchLfoFreq"))
        pitchLFO.frequency = *pitchLfoFreqParam;
    if (auto* pitchLfoDepthParam = apvts.getRawParameterValue("pitchLfoDepth"))
        pitchLFO.depth = *pitchLfoDepthParam;
    if (auto* densityLfoFreqParam = apvts.getRawParameterValue("densityLfoFreq"))
        densityLFO.frequency = *densityLfoFreqParam;
    if (auto* densityLfoDepthParam = apvts.getRawParameterValue("densityLfoDepth"))
        densityLFO.depth = *densityLfoDepthParam;

    // Update effects parameters
    if (auto* reverbRoomParam = apvts.getRawParameterValue("reverbRoom"))
        reverbEffect.setRoomSize(*reverbRoomParam);
    if (auto* reverbDampParam = apvts.getRawParameterValue("reverbDamping"))
        reverbEffect.setDamping(*reverbDampParam);
    if (auto* reverbWetParam = apvts.getRawParameterValue("reverbWet"))
    {
        smoothedReverbWet.setTargetValue(reverbWetParam->load());
        reverbEffect.setWetLevel(smoothedReverbWet.skip(numSamples));
    }
    
    if (auto* formantFreqParam = apvts.getRawParameterValue("formantFreq"))
        formantFilter.setFormantFrequency(*formantFreqParam);
    if (auto* formantMixParam = apvts.getRawParameterValue("formantMix"))
    {
        smoothedFormantMix.setTargetValue(formantMixParam->load());
        formantFilter.setDryWetMix(smoothedFormantMix.skip(numSamples));
    }

    // MIDI-triggered granular synthesis
    // Track active notes and pitch wheel to keep pitch controls responsive while notes are held.
    static std::array<bool, 128> activeNotes{false};
    static float currentPitchWheel = 0.0f; // -1..+1
    float rootNote = 60.0f;
    float fineTuneCents = 0.0f;
    float pitchBendRange = 2.0f;

    if (auto* rootNoteParam = apvts.getRawParameterValue("rootNote"))
        rootNote = *rootNoteParam;

    if (auto* fineTuneParam = apvts.getRawParameterValue("fineTuneCents"))
        fineTuneCents = *fineTuneParam;

    if (auto* pitchBendRangeParam = apvts.getRawParameterValue("pitchBendRange"))
        pitchBendRange = *pitchBendRangeParam;
    bool noteOnReceived = false;
    
    // Process MIDI messages for note-triggered granular synthesis
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn())
        {
            int noteNumber = message.getNoteNumber();
            activeNotes[noteNumber] = true;
            noteOnReceived = true;

            // Trigger an immediate grain for responsiveness.
            grainEngine->triggerGrain();
        }
        else if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();
            activeNotes[noteNumber] = false;
        }
        else if (message.isPitchWheel())
        {
            currentPitchWheel = (message.getPitchWheelValue() - 8192) / 8192.0f;
        }
    }

    int highestActiveNote = -1;
    for (int i = 127; i >= 0; --i)
    {
        if (activeNotes[i])
        {
            highestActiveNote = i;
            break;
        }
    }

    if (highestActiveNote >= 0)
    {
        const float baseSemitones = (static_cast<float>(highestActiveNote) - rootNote) + (fineTuneCents / 100.0f);
        const float bendSemitones = currentPitchWheel * pitchBendRange;
        const float targetRatio = std::pow(2.0f, (baseSemitones + bendSemitones) / 12.0f);

        grainEngine->setMIDIPitchRatio(targetRatio);
        grainEngine->setMIDITriggered(false);

        if (noteOnReceived)
            grainEngine->triggerGrain();
    }
    else
    {
        grainEngine->setMIDITriggered(true);
    }
    
    // Always process grain engine
    grainEngine->processBlock(buffer, numSamples);
    
    // Apply effects chain
    if (smoothedReverbWet.getCurrentValue() > 0.0001f || smoothedReverbWet.isSmoothing())
        reverbEffect.processBlock(buffer);

    if (smoothedFormantMix.getCurrentValue() > 0.0001f || smoothedFormantMix.isSmoothing())
        formantFilter.processBlock(buffer);

    // MASTER GAIN & SOFT CLIPPER (ultra-optimisé)
    float masterGain = 1.0f;
    if (auto* gainParam = apvts.getRawParameterValue("masterGain"))
        masterGain = gainParam->load();

    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            // Gain
            float x = data[i] * masterGain;
            // Soft clipper rapide (tanh approx)
            // y = x * (27 + x^2) / (27 + 9 * x^2) ~ tanh(x) pour -2 < x < 2
            float x2 = x * x;
            data[i] = juce::jlimit(-1.0f, 1.0f, x * (27.0f + x2) / (27.0f + 9.0f * x2));
        }
    }
}

//==============================================================================
bool EchoGrainSynthAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* EchoGrainSynthAudioProcessor::createEditor()
{
    // Use the Native JUCE editor with stable layout and all components
    return new NativePluginEditor(*this);
}

//==============================================================================
void EchoGrainSynthAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // Save parameters, sample metadata, and sample audio snapshot.
    auto state = apvts.copyState();
    state.setProperty("selectedPresetName", selectedPresetName, nullptr);
    state.setProperty("xyMidiLinkEnabled", xyMidiLinkEnabled, nullptr);
    
    // Add sample name to the state
    if (!sampleName.isEmpty())
        state.setProperty("sampleName", sampleName, nullptr);

    if (!currentSampleData.isEmpty())
    {
        state.setProperty("samplePath", currentSampleData.originalPath, nullptr);
        state.setProperty("sampleRate", currentSampleData.sampleRate, nullptr);
        state.setProperty("sampleNumChannels", currentSampleData.numChannels, nullptr);
        state.setProperty("sampleNumSamples", currentSampleData.numSamples, nullptr);

        juce::MemoryOutputStream encoded;
        juce::Base64::convertToBase64(encoded, currentSampleData.audioData.getData(), currentSampleData.audioData.getSize());
        state.setProperty("sampleDataBase64", encoded.toString(), nullptr);
    }
    
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void EchoGrainSynthAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // Restore parameters and sample state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    
    if (xmlState.get() != nullptr)
    {
        if (xmlState->hasTagName(apvts.state.getType()))
        {
            auto newState = juce::ValueTree::fromXml(*xmlState);
            apvts.replaceState(newState);
            selectedPresetName = newState.getProperty("selectedPresetName", "Init Empty");
            xyMidiLinkEnabled = static_cast<bool>(newState.getProperty("xyMidiLinkEnabled", true));
            stateRestoredFromProject = true;

            const juce::String restoredSampleName = newState.getProperty("sampleName", "");
            const juce::String restoredPath = newState.getProperty("samplePath", "");

            bool restoredSample = false;

            // Prefer restoring from original file path when available.
            if (restoredPath.isNotEmpty())
            {
                const juce::File originalFile(restoredPath);
                if (originalFile.existsAsFile())
                {
                    loadSample(originalFile);
                    restoredSample = true;
                }
            }

            // Fallback: restore embedded sample snapshot from the project state.
            if (!restoredSample)
            {
                const juce::String sampleDataBase64 = newState.getProperty("sampleDataBase64", "");
                juce::MemoryBlock decoded;
                juce::MemoryOutputStream decodedStream(decoded, false);

                if (sampleDataBase64.isNotEmpty() && juce::Base64::convertFromBase64(decodedStream, sampleDataBase64))
                {
                    const int numChannels = static_cast<int>(newState.getProperty("sampleNumChannels", 0));
                    const int numSamples = static_cast<int>(newState.getProperty("sampleNumSamples", 0));
                    const double restoredSampleRate = static_cast<double>(newState.getProperty("sampleRate", 44100.0));

                    const size_t expectedSize = static_cast<size_t>(numChannels) * static_cast<size_t>(numSamples) * sizeof(float);
                    if (numChannels > 0 && numSamples > 0 && decoded.getSize() == expectedSize)
                    {
                        loadedSample.setSize(numChannels, numSamples);
                        const auto* src = static_cast<const float*>(decoded.getData());

                        for (int ch = 0; ch < numChannels; ++ch)
                            loadedSample.copyFrom(ch, 0, src + (ch * numSamples), numSamples);

                        if (grainEngine != nullptr)
                            grainEngine->setSample(loadedSample);

                        for (int i = 0; i < synthesiser.getNumSounds(); ++i)
                        {
                            if (auto* granularSound = dynamic_cast<GranularSound*>(synthesiser.getSound(i).get()))
                                granularSound->setSampleData(loadedSample);
                        }

                        currentSampleData.audioData = decoded;
                        currentSampleData.numChannels = numChannels;
                        currentSampleData.numSamples = numSamples;
                        currentSampleData.sampleRate = restoredSampleRate;
                        currentSampleData.originalPath = restoredPath;
                        restoredSample = true;
                    }
                }
            }

            // Keep UI naming coherent with restored state.
            if (!restoredSampleName.isEmpty())
                sampleName = restoredSampleName;
            else if (!restoredSample)
                sampleName.clear();
        }
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EchoGrainSynthAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    
    // Grain parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>("grainSize", "Grain Size", 
                                                          juce::NormalisableRange<float>(10.0f, 600.0f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("density", "Density", 
                                                          juce::NormalisableRange<float>(0.1f, 30.0f), 10.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("position", "Position", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("positionSpread", "Position Spread", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pitch", "Pitch", 
                                                          juce::NormalisableRange<float>(0.1f, 4.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pitchSpread", "Pitch Spread", 
                                                          juce::NormalisableRange<float>(0.0f, 12.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverse", "Reverse", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pan", "Pan", 
                                                          juce::NormalisableRange<float>(-1.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("panSpread", "Pan Spread", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    
    // ADSR parameters (replacing attack/release with full ADSR)
    layout.add(std::make_unique<juce::AudioParameterFloat>("adsrAttack", "ADSR Attack", 
                                                          juce::NormalisableRange<float>(0.0f, 600.0f, 0.0f, 0.35f), 10.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("adsrDecay", "ADSR Decay", 
                                                          juce::NormalisableRange<float>(0.0f, 600.0f, 0.0f, 0.35f), 100.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("adsrSustain", "ADSR Sustain", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.7f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("adsrRelease", "ADSR Release", 
                                                          juce::NormalisableRange<float>(0.0f, 600.0f, 0.0f, 0.35f), 100.0f));
    
    // LFO parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>("positionLfoFreq", "Position LFO Freq", 
                                                          juce::NormalisableRange<float>(0.1f, 12.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("positionLfoDepth", "Position LFO Depth", 
                                                          juce::NormalisableRange<float>(0.0f, 0.5f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pitchLfoFreq", "Pitch LFO Freq", 
                                                          juce::NormalisableRange<float>(0.1f, 12.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("pitchLfoDepth", "Pitch LFO Depth", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("densityLfoFreq", "Density LFO Freq", 
                                                          juce::NormalisableRange<float>(0.1f, 12.0f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("densityLfoDepth", "Density LFO Depth", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    
    // Effects parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverbRoom", "Reverb Room", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverbDamping", "Reverb Damping", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.5f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("reverbWet", "Reverb Wet", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("formantFreq", "Formant Freq", 
                                                          juce::NormalisableRange<float>(150.0f, 3200.0f), 800.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("formantMix", "Formant Mix", 
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));

    // Master Gain
    layout.add(std::make_unique<juce::AudioParameterFloat>("masterGain", "Master Gain", juce::NormalisableRange<float>(0.0f, 2.0f), 1.0f));
    
    // XY Pad mapping slots
    layout.add(std::make_unique<juce::AudioParameterChoice>("xySlot1Target", "XY Slot 1 Target", 
                                                           juce::StringArray{"none", "grainSize", "density", "position", "pitch", "pan", "formantFreq", "reverbWet"}, 3));
    layout.add(std::make_unique<juce::AudioParameterInt>("xySlot1Axis", "XY Slot 1 Axis", 0, 1, 0)); // 0=X, 1=Y
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot1Min", "XY Slot 1 Min", 
                                                          juce::NormalisableRange<float>(0.0f, 0.95f), 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot1Max", "XY Slot 1 Max", 
                                                          juce::NormalisableRange<float>(0.05f, 1.0f), 0.9f));
    layout.add(std::make_unique<juce::AudioParameterBool>("xySlot1Invert", "XY Slot 1 Invert", false));
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("xySlot2Target", "XY Slot 2 Target", 
                                                           juce::StringArray{"none", "grainSize", "density", "position", "pitch", "pan", "formantFreq", "reverbWet"}, 7));
    layout.add(std::make_unique<juce::AudioParameterInt>("xySlot2Axis", "XY Slot 2 Axis", 0, 1, 1));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot2Min", "XY Slot 2 Min", 
                                                          juce::NormalisableRange<float>(0.0f, 0.95f), 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot2Max", "XY Slot 2 Max", 
                                                          juce::NormalisableRange<float>(0.05f, 1.0f), 0.9f));
    layout.add(std::make_unique<juce::AudioParameterBool>("xySlot2Invert", "XY Slot 2 Invert", false));
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("xySlot3Target", "XY Slot 3 Target", 
                                                           juce::StringArray{"none", "grainSize", "density", "position", "pitch", "pan", "formantFreq", "reverbWet"}, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>("xySlot3Axis", "XY Slot 3 Axis", 0, 1, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot3Min", "XY Slot 3 Min", 
                                                          juce::NormalisableRange<float>(0.0f, 0.95f), 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot3Max", "XY Slot 3 Max", 
                                                          juce::NormalisableRange<float>(0.05f, 1.0f), 0.9f));
    layout.add(std::make_unique<juce::AudioParameterBool>("xySlot3Invert", "XY Slot 3 Invert", false));
    
    layout.add(std::make_unique<juce::AudioParameterChoice>("xySlot4Target", "XY Slot 4 Target", 
                                                           juce::StringArray{"none", "grainSize", "density", "position", "pitch", "pan", "formantFreq", "reverbWet"}, 0));
    layout.add(std::make_unique<juce::AudioParameterInt>("xySlot4Axis", "XY Slot 4 Axis", 0, 1, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot4Min", "XY Slot 4 Min", 
                                                          juce::NormalisableRange<float>(0.0f, 0.95f), 0.1f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("xySlot4Max", "XY Slot 4 Max", 
                                                          juce::NormalisableRange<float>(0.05f, 1.0f), 0.9f));
    layout.add(std::make_unique<juce::AudioParameterBool>("xySlot4Invert", "XY Slot 4 Invert", false));
    
    // MIDI sampler parameters
    layout.add(std::make_unique<juce::AudioParameterInt>("rootNote", "Root Note", 24, 96, 60)); // C1..C7, default C4
    layout.add(std::make_unique<juce::AudioParameterFloat>("fineTuneCents", "Fine Tune Cents", 
                                                          juce::NormalisableRange<float>(-50.0f, 50.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterInt>("pitchBendRange", "Pitch Bend Range", 1, 7, 2));
    
    
    // Sidechain and recording parameters
    layout.add(std::make_unique<juce::AudioParameterFloat>("sidechainThreshold", "Sidechain Threshold", 
                                                          juce::NormalisableRange<float>(0.01f, 1.0f), 0.1f));
    layout.add(std::make_unique<juce::AudioParameterBool>("tempoSync", "Tempo Sync", false));
    layout.add(std::make_unique<juce::AudioParameterInt>("maxActiveGrains", "Max Active Grains", 8, 64, 40));
    layout.add(std::make_unique<juce::AudioParameterChoice>("cpuMode", "CPU Mode",
                                                           juce::StringArray{"Eco", "High"}, 1));
    
    return layout;
}

// Implementation of new methods
void EchoGrainSynthAudioProcessor::updateHostInfo()
{
    auto hostPlayHead = getPlayHead();
    if (hostPlayHead != nullptr)
    {
        auto positionInfo = hostPlayHead->getPosition();
        if (positionInfo.hasValue())
        {
            if (auto bpm = positionInfo->getBpm())
                currentBPM = *bpm;
            isPlaying = positionInfo->getIsPlaying();
            
            // Sync grain density to tempo if enabled
            if (auto* tempoSyncParam = apvts.getRawParameterValue("tempoSync"))
            {
                if (*tempoSyncParam > 0.5f && currentBPM > 0.0)
                {
                    // Calculate grain density based on BPM (4 grains per beat)
                    float syncedDensity = static_cast<float>((currentBPM / 60.0) * 4.0);
                    grainEngine->setGrainDensity(syncedDensity);
                }
            }
        }
    }
}

void EchoGrainSynthAudioProcessor::processSidechain(const juce::AudioBuffer<float>& inputBuffer)
{
    // Analyze sidechain signal for grain triggering
    float sidechainLevel = 0.0f;
    for (int sample = 0; sample < inputBuffer.getNumSamples(); ++sample)
    {
        float sampleValue = 0.0f;
        for (int channel = 0; channel < inputBuffer.getNumChannels(); ++channel)
        {
            sampleValue += std::abs(inputBuffer.getSample(channel, sample));
        }
        sampleValue /= inputBuffer.getNumChannels();
        sidechainLevel = juce::jmax(sidechainLevel, sampleValue);
    }
    
    // Trigger grains based on sidechain level
    if (auto* sidechainThresholdParam = apvts.getRawParameterValue("sidechainThreshold"))
    {
        float threshold = *sidechainThresholdParam;
        if (sidechainLevel > threshold && sidechainLevel > lastSidechainLevel + 0.1f)
        {
            grainEngine->triggerGrain();
            lastSidechainLevel = sidechainLevel;
        }
        else
        {
            lastSidechainLevel = sidechainLevel * 0.95f; // Decay
        }
    }
}

void EchoGrainSynthAudioProcessor::startRecording()
{
    if (!isRecording)
    {
        recordingBuffer.clear();
        recordingBuffer.setSize(2, static_cast<int>(getSampleRate() * 10.0)); // 10 seconds max
        recordingSamplePosition = 0;
        isRecording = true;
    }
}

void EchoGrainSynthAudioProcessor::stopRecording()
{
    if (isRecording)
    {
        isRecording = false;
        
        // Copy recorded buffer to grain engine
        if (recordingSamplePosition > 0)
        {
            juce::AudioBuffer<float> trimmedBuffer(recordingBuffer.getNumChannels(), recordingSamplePosition);
            for (int ch = 0; ch < trimmedBuffer.getNumChannels(); ++ch)
            {
                trimmedBuffer.copyFrom(ch, 0, recordingBuffer, ch, 0, recordingSamplePosition);
            }
            grainEngine->setSample(trimmedBuffer);
            setSampleName("Recorded Sample");
        }
    }
}

void EchoGrainSynthAudioProcessor::recordAudio(const juce::AudioBuffer<float>& buffer)
{
    if (isRecording && recordingSamplePosition < recordingBuffer.getNumSamples())
    {
        int samplesToRecord = juce::jmin(buffer.getNumSamples(), 
                                        recordingBuffer.getNumSamples() - recordingSamplePosition);
        
        for (int ch = 0; ch < juce::jmin(buffer.getNumChannels(), recordingBuffer.getNumChannels()); ++ch)
        {
            recordingBuffer.copyFrom(ch, recordingSamplePosition, buffer, ch, 0, samplesToRecord);
        }
        
        recordingSamplePosition += samplesToRecord;
        
        // Stop recording if buffer is full
        if (recordingSamplePosition >= recordingBuffer.getNumSamples())
        {
            stopRecording();
        }
    }
}

bool EchoGrainSynthAudioProcessor::hasSidechainInput() const
{
    return getBusesLayout().inputBuses.size() > 1;
}

void EchoGrainSynthAudioProcessor::loadSample(const juce::File& file)
{
    if (!file.existsAsFile())
        return;
        
    std::unique_ptr<juce::AudioFormatReader> reader(formatManager.createReaderFor(file));
    
    if (reader != nullptr)
    {
        // Load the audio data
        loadedSample.setSize(static_cast<int>(reader->numChannels), 
                           static_cast<int>(reader->lengthInSamples));
        reader->read(&loadedSample, 0, static_cast<int>(reader->lengthInSamples), 0, true, true);
        
        // Update the sample name
        sampleName = file.getFileNameWithoutExtension();
        
        // Update the grain engine with the new sample
        if (grainEngine != nullptr)
        {
            grainEngine->setSample(loadedSample);
        }
        
        // Update all GranularSound objects in the synthesizer with the new sample
        for (int i = 0; i < synthesiser.getNumSounds(); ++i)
        {
            if (auto* granularSound = dynamic_cast<GranularSound*>(synthesiser.getSound(i).get()))
            {
                granularSound->setSampleData(loadedSample);
            }
        }
        
        // Store sample data for persistence
        currentSampleData.audioData.reset();
        currentSampleData.numSamples = loadedSample.getNumSamples();
        currentSampleData.numChannels = loadedSample.getNumChannels();
        currentSampleData.sampleRate = reader->sampleRate;
        currentSampleData.originalPath = file.getFullPathName();
        
        // Convert audio data to memory block for state saving
        size_t dataSize = sizeof(float) * loadedSample.getNumSamples() * loadedSample.getNumChannels();
        currentSampleData.audioData.setSize(dataSize);
        
        float* dest = static_cast<float*>(currentSampleData.audioData.getData());
        for (int ch = 0; ch < loadedSample.getNumChannels(); ++ch)
        {
            const float* src = loadedSample.getReadPointer(ch);
            for (int i = 0; i < loadedSample.getNumSamples(); ++i)
            {
                *dest++ = src[i];
            }
        }
        
        DBG("Sample loaded: " + sampleName + " (" + juce::String(loadedSample.getNumSamples()) + " samples)");
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EchoGrainSynthAudioProcessor();
}
