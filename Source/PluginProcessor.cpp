#include "PluginProcessor.h"
#include "NativePluginEditor.h"  // Changed from React-JUCE to Native JUCE UI

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
    
    // Configure MPE: lower zone occupies member channels 2-16 (15 channels)
    {
        juce::MPEZoneLayout layout;
        layout.setLowerZone (15);
        mpeInstrument.setZoneLayout (layout);
    }
    
    // Initialize recording buffer (10 seconds max at 44.1kHz)
    recordingBuffer.setSize(2, static_cast<int>(44100 * 10));
    recordingSamplePosition = 0;
    
    // Initialize sidechain buffer
    sidechainBuffer.setSize(2, 4096);
}

EchoGrainSynthAudioProcessor::~EchoGrainSynthAudioProcessor()
{
    stopWavRecording();
    wavRecordThread.stopThread (2000);
}

//==============================================================================
void EchoGrainSynthAudioProcessor::startWavRecording (const juce::File& destFile)
{
    stopWavRecording(); // close any previous session

    const double sr  = getSampleRate();
    const int    ch  = getTotalNumOutputChannels();
    if (sr <= 0.0 || ch <= 0) return;

    destFile.deleteFile();
    auto* stream = destFile.createOutputStream().release();
    if (stream == nullptr) return;

    juce::WavAudioFormat wavFmt;
    auto* writer = wavFmt.createWriterFor (stream, sr, (unsigned int) ch, 24, {}, 0);
    if (writer == nullptr) { delete stream; return; }

    if (!wavRecordThread.isThreadRunning())
        wavRecordThread.startThread();

    // ThreadedWriter owns the writer (and the stream through it)
    wavWriter = std::make_unique<juce::AudioFormatWriter::ThreadedWriter> (writer, wavRecordThread, 32768);
    isWavRecording.store (true);
}

void EchoGrainSynthAudioProcessor::stopWavRecording()
{
    isWavRecording.store (false);
    wavWriter.reset(); // flushes and closes the file cleanly
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
    // Report the maximum possible tail: delay (up to 2 s) + reverb decay (~2 s).
    // This prevents hosts from cutting audio too early when notes stop.
    return 4.0;
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
    reverbEffect.prepare(sampleRate, samplesPerBlock);
    formantFilter.prepare(sampleRate, samplesPerBlock);
    delayEffect.prepare(sampleRate, samplesPerBlock);

    smoothedReverbWet.reset(sampleRate, 0.03);
    smoothedFormantMix.reset(sampleRate, 0.02);
    smoothedDelayWet.reset(sampleRate, 0.025);
    smoothedMasterGain.reset(sampleRate, 0.02);

    const float initialReverbWet = (apvts.getRawParameterValue("reverbWet") != nullptr)
        ? apvts.getRawParameterValue("reverbWet")->load()
        : 0.0f;
    smoothedReverbWet.setCurrentAndTargetValue(initialReverbWet);

    const float initialFormantMix = (apvts.getRawParameterValue("formantMix") != nullptr)
        ? apvts.getRawParameterValue("formantMix")->load()
        : 0.0f;
    smoothedFormantMix.setCurrentAndTargetValue(initialFormantMix);

    const float initialDelayWet = (apvts.getRawParameterValue("delayWet") != nullptr)
        ? apvts.getRawParameterValue("delayWet")->load()
        : 0.0f;
    smoothedDelayWet.setCurrentAndTargetValue(initialDelayWet);

    const float initialMasterGain = (apvts.getRawParameterValue("masterGain") != nullptr)
        ? apvts.getRawParameterValue("masterGain")->load()
        : 1.0f;
    smoothedMasterGain.setCurrentAndTargetValue(initialMasterGain);
}

void EchoGrainSynthAudioProcessor::releaseResources()
{
    if (grainEngine != nullptr)
        grainEngine->reset();
    mpeInstrument.setZoneLayout(mpeInstrument.getZoneLayout()); // clears all active notes
    std::fill(activeNotes.begin(),    activeNotes.end(),    false);
    std::fill(noteVelocities.begin(), noteVelocities.end(), 0.0f);
    mpePressure = 0.5f;
    mpeSlide    = 0.5f;
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
    // A/B bypass: loop the loaded sample at the current position param (meaningful A/B comparison)
    if (abBypassEnabled.load())
    {
        buffer.clear();
        const juce::ScopedTryReadLock tryRead(sampleLock);
        if (!tryRead.isLocked())
            return; // sample is being swapped, skip this block safely
        const int numSamplesTotal = loadedSample.getNumSamples();
        if (numSamplesTotal > 0)
        {
            float masterGain = 1.0f;
            if (auto* gainParam = apvts.getRawParameterValue("masterGain"))
                masterGain = gainParam->load();

            // Loop a 1-second window around the current position param
            float posNorm = 0.0f;
            if (auto* posParam = apvts.getRawParameterValue("position"))
                posNorm = posParam->load();

            const int windowSamples = juce::jmin(numSamplesTotal,
                static_cast<int>(sampleRateForProcessing));
            const int windowStart = juce::jlimit(0,
                juce::jmax(0, numSamplesTotal - windowSamples),
                static_cast<int>(posNorm * static_cast<float>(numSamplesTotal - windowSamples)));

            int pos = abPlaybackPosition.load();
            for (int i = 0; i < numSamples; ++i)
            {
                const int srcIdx = windowStart + (pos % windowSamples);
                for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                {
                    const int srcCh = juce::jmin(ch, loadedSample.getNumChannels() - 1);
                    buffer.getWritePointer(ch)[i] = loadedSample.getReadPointer(srcCh)[srcIdx] * masterGain;
                }
                ++pos;
            }
            abPlaybackPosition.store(pos % windowSamples);
        }
        return;
    }

    // === New granular parameters ===
    if (auto* freezeParam = apvts.getRawParameterValue("freeze"))
        grainEngine->setFreeze(freezeParam->load() > 0.5f);

    if (auto* windowParam = apvts.getRawParameterValue("windowType"))
        grainEngine->setWindowType(static_cast<GrainEngine::WindowType>(juce::roundToInt(windowParam->load())));

    if (auto* gsSpreadadParam = apvts.getRawParameterValue("grainSizeSpread"))
        grainEngine->setGrainSizeSpread(gsSpreadadParam->load());

    // LFO waveform shape (applies to all LFOs)
    int lfoWave = 0;
    if (auto* lfoWaveParam = apvts.getRawParameterValue("lfoWaveform"))
        lfoWave = juce::roundToInt(lfoWaveParam->load());
    positionLFO.waveform  = lfoWave;
    pitchLFO.waveform     = lfoWave;
    densityLFO.waveform   = lfoWave;
    grainSizeLFO.waveform = lfoWave;

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

    // GrainSize LFO
    if (auto* gsParam = apvts.getRawParameterValue("grainSize"))
    {
        if (auto* gsLfoFreqP = apvts.getRawParameterValue("grainSizeLfoFreq"))
            grainSizeLFO.frequency = gsLfoFreqP->load();
        if (auto* gsLfoDepthP = apvts.getRawParameterValue("grainSizeLfoDepth"))
            grainSizeLFO.depth = gsLfoDepthP->load();
        const float lfoMod = grainSizeLFO.getNextValue(static_cast<float>(sampleRateForProcessing));
        grainEngine->setGrainSize(juce::jlimit(10.0f, 600.0f, gsParam->load() + lfoMod));
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
    // grainSizeLFO freq/depth are read inside the grainSize LFO block above

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
    
    // Feed all MIDI events into the MPE instrument for per-note expression tracking
    for (const auto& meta : midiMessages)
        mpeInstrument.processNextMidiEvent(meta.getMessage());

    // Process MIDI messages for note-triggered granular synthesis
    for (const auto metadata : midiMessages)
    {
        auto message = metadata.getMessage();
        
        if (message.isNoteOn())
        {
            int   noteNumber = message.getNoteNumber();
            float velocity   = message.getFloatVelocity();
            activeNotes[noteNumber]    = true;
            noteVelocities[noteNumber] = velocity;
            noteOnReceived = true;

            // Forward velocity to grain engine so grains reflect key force
            grainEngine->setNoteVelocity(velocity);
            // Trigger a note-tracked grain immediately for responsiveness
            grainEngine->triggerGrainForNote(noteNumber, velocity);
        }
        else if (message.isNoteOff())
        {
            int noteNumber = message.getNoteNumber();
            activeNotes[noteNumber]    = false;
            noteVelocities[noteNumber] = 0.0f;
            grainEngine->releaseNote(noteNumber);
        }
        else if (message.isAftertouch())
        {
            // Per-note pressure (MPE member channel aftertouch)
            const float v = message.getAfterTouchValue() / 127.0f;
            mpePressure = juce::jmax(mpePressure, v);
        }
        else if (message.isChannelPressure())
        {
            // Channel-wide pressure (standard MIDI and MPE master channel)
            mpePressure = message.getChannelPressureValue() / 127.0f;
        }
        else if (message.isController())
        {
            if (message.getControllerNumber() == 74) // MPE slide / timbre
                mpeSlide = message.getControllerValue() / 127.0f;
            // All CCs (including 74) can still be MIDI-learned to any parameter
            midiLearnManager.handleCC (message.getControllerNumber(),
                                       message.getControllerValue(), apvts);
        }
        else if (message.isPitchWheel())
        {
            currentPitchWheel = (message.getPitchWheelValue() - 8192) / 8192.0f;
        }
    }

    // Find highest active note and velocity-weighted note for pitch/expression
    int highestActiveNote = -1;
    float highestNoteVelocity = 0.0f;
    for (int i = 127; i >= 0; --i)
    {
        if (activeNotes[i])
        {
            highestActiveNote = i;
            highestNoteVelocity = noteVelocities[i];
            break;
        }
    }

    if (highestActiveNote >= 0)
    {
        const float baseSemitones = (static_cast<float>(highestActiveNote) - rootNote) + (fineTuneCents / 100.0f);
        const float bendSemitones = currentPitchWheel * pitchBendRange;
        const float targetRatio = std::pow(2.0f, (baseSemitones + bendSemitones) / 12.0f);

        grainEngine->setMIDIPitchRatio(targetRatio);
        grainEngine->setNoteVelocity(highestNoteVelocity);
        grainEngine->setMIDITriggered(false);
    }
    else
    {
        // No notes held: reset velocity to full and return to free-running mode
        grainEngine->setNoteVelocity(1.0f);
        grainEngine->setMIDITriggered(true);
    }

    // === MPE per-note expression modulation ===
    // Decay pressure toward neutral when no notes are held
    if (highestActiveNote < 0)
        mpePressure = mpePressure * 0.995f + 0.5f * 0.005f; // slow decay back to 0.5

    if (highestActiveNote >= 0)
    {
        // Pressure (0..1, neutral=0.5): scale density in ±50% range
        if (auto* densityParam = apvts.getRawParameterValue("density"))
        {
            const float baseDensity = densityParam->load();
            const float mpeDensity  = baseDensity * (0.5f + mpePressure);
            grainEngine->setGrainDensity(juce::jlimit(0.1f, 30.0f, mpeDensity));
        }
        // Slide CC74 (0..1, neutral=0.5): shift read position ±0.25
        if (auto* posParam = apvts.getRawParameterValue("position"))
        {
            const float basePos = posParam->load();
            const float mpePos  = basePos + (mpeSlide - 0.5f) * 0.5f;
            grainEngine->setPosition(juce::jlimit(0.0f, 1.0f, mpePos));
        }
    }
    
    // Always process grain engine
    grainEngine->processBlock(buffer, numSamples);
    
    // Apply effects chain
    if (smoothedReverbWet.getCurrentValue() > 0.0001f || smoothedReverbWet.isSmoothing())
        reverbEffect.processBlock(buffer);

    if (smoothedFormantMix.getCurrentValue() > 0.0001f || smoothedFormantMix.isSmoothing())
        formantFilter.processBlock(buffer);

    // Delay
    if (auto* delayWetParam = apvts.getRawParameterValue("delayWet"))
    {
        smoothedDelayWet.setTargetValue(delayWetParam->load());
        delayEffect.setWetLevel(smoothedDelayWet.skip(numSamples));
    }
    if (auto* delayTimeParam = apvts.getRawParameterValue("delayTimeMs"))
        delayEffect.setDelayTimeMs(delayTimeParam->load());
    if (auto* delayFbParam = apvts.getRawParameterValue("delayFeedback"))
        delayEffect.setFeedback(delayFbParam->load());
    {
        bool bpmSync = false;
        int  subdivision = 2; // default 1/4
        if (auto* syncParam = apvts.getRawParameterValue("delayBpmSync"))
            bpmSync = syncParam->load() > 0.5f;
        if (auto* subdivParam = apvts.getRawParameterValue("delaySubdivision"))
            subdivision = juce::roundToInt(subdivParam->load());
        delayEffect.setBpmSync(bpmSync, currentBPM, subdivision);
    }
    if (smoothedDelayWet.getCurrentValue() > 0.0001f || smoothedDelayWet.isSmoothing())
        delayEffect.processBlock(buffer);

    // MASTER GAIN & SOFT CLIPPER (per-sample smoothed to eliminate zippering on automation)
    if (auto* gainParam = apvts.getRawParameterValue("masterGain"))
        smoothedMasterGain.setTargetValue(gainParam->load());

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float gain = smoothedMasterGain.getNextValue();
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            float x = buffer.getWritePointer(ch)[i] * gain;
            // Soft clipper (tanh approx): y ≈ x*(27+x²)/(27+9x²)
            float x2 = x * x;
            buffer.getWritePointer(ch)[i] = juce::jlimit(-1.0f, 1.0f, x * (27.0f + x2) / (27.0f + 9.0f * x2));
        }
    }

    // Update output peak level for the UI meter (decaying envelope follower)
    {
        float blockPeak = 0.0f;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            blockPeak = juce::jmax(blockPeak, buffer.getMagnitude(ch, 0, buffer.getNumSamples()));
        // 60 Hz decay pole: release ~= 300 ms @ 60 fps
        const float prev = outputPeakLevel.load(std::memory_order_relaxed);
        outputPeakLevel.store(blockPeak > prev ? blockPeak : prev * 0.9998f,
                              std::memory_order_relaxed);
    }

    // WAV export — push current block to background writer (lock-free FIFO)
    if (isWavRecording.load(std::memory_order_relaxed))
    {
        if (wavWriter != nullptr)
        {
            // Build a const pointer array that ThreadedWriter::write() expects
            const int numCh = buffer.getNumChannels();
            const int numSmp = buffer.getNumSamples();
            juce::HeapBlock<const float*> channelPtrs ((size_t) numCh);
            for (int ch = 0; ch < numCh; ++ch)
                channelPtrs[ch] = buffer.getReadPointer (ch);
            wavWriter->write (channelPtrs.getData(), numSmp);
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
    midiLearnManager.saveToValueTree(state);
    
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
            midiLearnManager.loadFromValueTree(newState);
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

    // Delay
    layout.add(std::make_unique<juce::AudioParameterFloat>("delayTimeMs", "Delay Time",
                                                          juce::NormalisableRange<float>(1.0f, 2000.0f, 0.0f, 0.35f), 375.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delayFeedback", "Delay Feedback",
                                                          juce::NormalisableRange<float>(0.0f, 0.95f), 0.3f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("delayWet", "Delay Wet",
                                                          juce::NormalisableRange<float>(0.0f, 1.0f), 0.0f));
    layout.add(std::make_unique<juce::AudioParameterBool>("delayBpmSync", "Delay BPM Sync", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>("delaySubdivision", "Delay Subdivision",
                                                           juce::StringArray{"1/1","1/2","1/4","1/8","1/16"}, 2)); // default 1/4

    // Granular advanced
    layout.add(std::make_unique<juce::AudioParameterBool>("freeze", "Freeze", false));
    layout.add(std::make_unique<juce::AudioParameterChoice>("windowType", "Window Type",
                                                           juce::StringArray{"Hanning", "Gaussian", "Rectangular", "Tukey"}, 0));
    layout.add(std::make_unique<juce::AudioParameterFloat>("grainSizeSpread", "Grain Size Spread",
                                                          juce::NormalisableRange<float>(0.0f, 200.0f), 0.0f));

    // LFO waveform
    layout.add(std::make_unique<juce::AudioParameterChoice>("lfoWaveform", "LFO Shape",
                                                           juce::StringArray{"Sine", "Square", "Triangle", "S&H"}, 0));

    // GrainSize LFO
    layout.add(std::make_unique<juce::AudioParameterFloat>("grainSizeLfoFreq", "GrainSize LFO Freq",
                                                          juce::NormalisableRange<float>(0.01f, 10.0f, 0.0f, 0.4f), 1.0f));
    layout.add(std::make_unique<juce::AudioParameterFloat>("grainSizeLfoDepth", "GrainSize LFO Depth",
                                                          juce::NormalisableRange<float>(0.0f, 100.0f), 0.0f));
    
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
        
        if (recordingSamplePosition > 0)
        {
            juce::AudioBuffer<float> trimmedBuffer(recordingBuffer.getNumChannels(), recordingSamplePosition);
            for (int ch = 0; ch < trimmedBuffer.getNumChannels(); ++ch)
                trimmedBuffer.copyFrom(ch, 0, recordingBuffer, ch, 0, recordingSamplePosition);

            // Write-lock: swap loadedSample safely
            {
                const juce::ScopedWriteLock writeLock(sampleLock);
                loadedSample.makeCopyOf(trimmedBuffer);
            }

            grainEngine->setSample(loadedSample);
            setSampleName("Recorded Sample");

            // Persist to project state so the recording survives save/reload
            const double sessionRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
            currentSampleData.audioData.reset();
            currentSampleData.numSamples  = loadedSample.getNumSamples();
            currentSampleData.numChannels = loadedSample.getNumChannels();
            currentSampleData.sampleRate  = sessionRate;
            currentSampleData.originalPath.clear(); // no file path for recordings

            const size_t dataSize = sizeof(float)
                * static_cast<size_t>(loadedSample.getNumSamples())
                * static_cast<size_t>(loadedSample.getNumChannels());
            currentSampleData.audioData.setSize(dataSize);

            float* dest = static_cast<float*>(currentSampleData.audioData.getData());
            for (int ch = 0; ch < loadedSample.getNumChannels(); ++ch)
            {
                const float* src = loadedSample.getReadPointer(ch);
                for (int i = 0; i < loadedSample.getNumSamples(); ++i)
                    *dest++ = src[i];
            }
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
        // Resample to the current session sample rate so pitch is always correct
        const double fileSampleRate    = reader->sampleRate;
        const double sessionSampleRate = getSampleRate() > 0.0 ? getSampleRate() : 44100.0;
        const int    fileNumSamples    = static_cast<int>(reader->lengthInSamples);
        const int    fileNumChannels   = static_cast<int>(reader->numChannels);

        // Read raw data at native file rate
        juce::AudioBuffer<float> rawBuffer(fileNumChannels, fileNumSamples);
        reader->read(&rawBuffer, 0, fileNumSamples, 0, true, true);

        juce::AudioBuffer<float> tempSample;
        if (std::abs(fileSampleRate - sessionSampleRate) < 0.5)
        {
            // Same rate — no conversion needed
            tempSample.makeCopyOf(rawBuffer);
        }
        else
        {
            // Resample with JUCE's LagrangeInterpolator
            const double ratio       = fileSampleRate / sessionSampleRate;
            const int    outSamples  = juce::roundToInt(static_cast<double>(fileNumSamples) / ratio);
            tempSample.setSize(fileNumChannels, outSamples);

            for (int ch = 0; ch < fileNumChannels; ++ch)
            {
                juce::LagrangeInterpolator resampler;
                resampler.reset();
                const float* src = rawBuffer.getReadPointer(ch);
                float*       dst = tempSample.getWritePointer(ch);
                resampler.process(ratio, src, dst, outSamples);
            }
        }

        // Write-lock: swap loadedSample atomically relative to any processBlock read
        {
            const juce::ScopedWriteLock writeLock(sampleLock);
            loadedSample.makeCopyOf(tempSample);
        }

        // Update the sample name
        sampleName = file.getFileNameWithoutExtension();

        // Push the new sample to the grain engine (it makes its own internal copy)
        if (grainEngine != nullptr)
            grainEngine->setSample(loadedSample);
        
        // Store sample data for persistence / state saving (always at session rate)
        currentSampleData.audioData.reset();
        currentSampleData.numSamples   = loadedSample.getNumSamples();
        currentSampleData.numChannels  = loadedSample.getNumChannels();
        currentSampleData.sampleRate   = sessionSampleRate;   // already resampled
        currentSampleData.originalPath = file.getFullPathName();
        
        const size_t dataSize = sizeof(float) * static_cast<size_t>(loadedSample.getNumSamples())
                                              * static_cast<size_t>(loadedSample.getNumChannels());
        currentSampleData.audioData.setSize(dataSize);
        
        float* dest = static_cast<float*>(currentSampleData.audioData.getData());
        for (int ch = 0; ch < loadedSample.getNumChannels(); ++ch)
        {
            const float* src = loadedSample.getReadPointer(ch);
            for (int i = 0; i < loadedSample.getNumSamples(); ++i)
                *dest++ = src[i];
        }
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EchoGrainSynthAudioProcessor();
}
