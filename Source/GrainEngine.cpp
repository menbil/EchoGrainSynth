#include "GrainEngine.h"

GrainEngine::GrainEngine() 
    : gen(rd())
    , uniformDist(0.0f, 1.0f)
{
    for (auto& grain : grains)
        grain.reset();
}

GrainEngine::~GrainEngine()
{
}

void GrainEngine::reset()
{
    for (auto& grain : grains)
        grain.reset();
    activeNotes.clear();
    grainTimer = 0.0f;
    smoothedDensity = grainDensity;
    midiPitchRatio  = 1.0f;
    midiNoteVelocity = 1.0f;
}

void GrainEngine::prepareToPlay(double newSampleRate, int /*samplesPerBlock*/)
{
    sampleRate = newSampleRate;
    // One-pole smoothing coefficient for grainDensity (20 ms time constant)
    densitySmoothCoeff = 1.0f - std::exp(-1.0f / (static_cast<float>(sampleRate) * 0.020f));
    smoothedDensity = grainDensity; // initialise at current value to avoid startup ramp
    samplesPerGrain = static_cast<float>(sampleRate) / smoothedDensity;
    grainTimer = 0.0f;
}

void GrainEngine::processBlock(juce::AudioBuffer<float>& buffer, int numSamples)
{
    if (!hasSample())
    {
        buffer.clear();
        return;
    }
    
    buffer.clear();
    
    // Only auto-generate grains if not in MIDI-triggered mode
    if (!midiTriggered)
    {
        updateGrainTimer(numSamples);
    }
    
    const int numSrcCh  = sampleBuffer.getNumChannels();
    const int maxSrcIdx = sampleBuffer.getNumSamples() - 1;

    // Hermite 4-point cubic interpolation (per channel)
    auto hermiteInterp = [&](int ch, float frac, int i0, int i1, int i2, int i3) -> float
    {
        const int c = juce::jmin(ch, numSrcCh - 1);
        const float y0 = sampleBuffer.getSample(c, i0);
        const float y1 = sampleBuffer.getSample(c, i1);
        const float y2 = sampleBuffer.getSample(c, i2);
        const float y3 = sampleBuffer.getSample(c, i3);
        const float c1 = 0.5f * (y2 - y0);
        const float c2 = y0 - 2.5f * y1 + 2.0f * y2 - 0.5f * y3;
        const float c3 = 0.5f * (y3 - y0) + 1.5f * (y1 - y2);
        return ((c3 * frac + c2) * frac + c1) * frac + y1;
    };

    // Process all active grains
    for (auto& grain : grains)
    {
        if (!grain.isActive)
            continue;
            
        for (int sample = 0; sample < numSamples; ++sample)
        {
            // Fractional read position with pitch ratio applied
            const float floatPos = static_cast<float>(grain.position) * grain.pitch;
            if (floatPos >= static_cast<float>(grain.grainSize - 1))
            {
                grain.reset();
                break;
            }

            const float frac = floatPos - std::floor(floatPos);
            const int iBase = static_cast<int>(floatPos);

            // Build 4 sample indices, respecting forward/reverse direction
            int i1, i0, i2, i3;
            if (grain.reverse)
            {
                i1 = juce::jlimit(0, maxSrcIdx, grain.startPos + (grain.grainSize - 1) - iBase);
                i0 = juce::jlimit(0, maxSrcIdx, i1 + 1);
                i2 = juce::jlimit(0, maxSrcIdx, i1 - 1);
                i3 = juce::jlimit(0, maxSrcIdx, i1 - 2);
            }
            else
            {
                i1 = juce::jlimit(0, maxSrcIdx, grain.startPos + iBase);
                i0 = juce::jlimit(0, maxSrcIdx, i1 - 1);
                i2 = juce::jlimit(0, maxSrcIdx, i1 + 1);
                i3 = juce::jlimit(0, maxSrcIdx, i1 + 2);
            }

            // Hermite interpolation - separate left and right for stereo grains
            const float sampleL = hermiteInterp(0, frac, i0, i1, i2, i3);
            const float sampleR = hermiteInterp(1, frac, i0, i1, i2, i3);  // falls back to ch0 if mono

            // Apply envelope - use ADSR for MIDI-triggered grains, window for others
            float envelopeValue;
            if (grain.midiNote >= 0)
            {
                // MIDI-triggered grain - use ADSR
                envelopeValue = calculateADSREnvelope(grain);
                if (envelopeValue <= 0.0f)
                {
                    // ADSR finished, deactivate grain
                    grain.reset();
                    break;
                }
            }
            else
            {
                // Regular grain - use window envelope
                envelopeValue = calculateEnvelope(grain);
            }

            const float scaledEnv = envelopeValue * grain.amplitude;

            // Apply stereo panning (equal-power)
            const float panAngle = (grain.pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi; // 0..pi/2
            const float leftGain  = std::cos(panAngle);
            const float rightGain = std::sin(panAngle);

            if (buffer.getNumChannels() >= 1)
                buffer.addSample(0, sample, sampleL * scaledEnv * leftGain);
            if (buffer.getNumChannels() >= 2)
                buffer.addSample(1, sample, sampleR * scaledEnv * rightGain);
            
            grain.position++;
            grain.age++;
        }
    }
}

void GrainEngine::setSample(const juce::AudioBuffer<float>& newSample)
{
    sampleBuffer.makeCopyOf(newSample);
    
    // Reset all grains when loading a new sample
    for (auto& grain : grains)
        grain.reset();
}

std::vector<GrainVisualizationPoint> GrainEngine::getVisualizationPoints() const
{
    std::vector<GrainVisualizationPoint> points;
    points.reserve(static_cast<size_t>(maxActiveGrains));

    for (size_t i = 0; i < grains.size(); ++i)
    {
        const auto& grain = grains[i];
        if (!grain.isActive)
            continue;

        GrainVisualizationPoint p;
        p.index = static_cast<int>(i);
        p.progress = (grain.grainSize > 0)
            ? juce::jlimit(0.0f, 1.0f, static_cast<float>(grain.position) / static_cast<float>(grain.grainSize))
            : 0.0f;
        p.panNorm = juce::jlimit(0.0f, 1.0f, (grain.pan + 1.0f) * 0.5f);

        const float env = (grain.midiNote >= 0)
            ? grain.adsrValue
            : calculateEnvelope(grain);
        p.energy = juce::jlimit(0.0f, 1.0f, env * juce::jmax(0.0f, grain.amplitude));
        p.reverse = grain.reverse;

        points.push_back(p);
    }

    return points;
}

void GrainEngine::triggerGrain()
{
    if (!hasSample())
        return;
        
    spawnGrain();
}

void GrainEngine::spawnGrain()
{
    int activeCount = 0;
    for (const auto& g : grains) if (g.isActive) ++activeCount;
    if (activeCount >= maxActiveGrains || !hasSample())
        return;
        
    int grainIndex = findInactiveGrain();
    if (grainIndex == -1)
        return;
        
    Grain& grain = grains[static_cast<size_t>(grainIndex)];
    grain.reset();
    grain.isActive = true;
    
    // Calculate grain size in samples with optional random spread
    const float spreadMs = (uniformDist(gen) * 2.0f - 1.0f) * grainSizeSpread;
    const float actualGrainSizeMs = juce::jlimit(10.0f, 600.0f, grainSize + spreadMs);
    grain.grainSize = static_cast<int>((actualGrainSizeMs / 1000.0f) * static_cast<float>(sampleRate));
    grain.grainSize = juce::jlimit(10, sampleBuffer.getNumSamples(), grain.grainSize);
    
    // Calculate start position with spread, clamped to user-defined sample range
    const float posBase   = freeze ? frozenPosition : playbackPosition;
    const float rangeLen  = sampleRangeEnd - sampleRangeStart;
    float posSpread = freeze ? 0.0f : (uniformDist(gen) - 0.5f) * positionSpread * rangeLen * 2.0f;
    float posInRange      = sampleRangeStart + posBase * rangeLen;
    float actualPosition  = juce::jlimit(sampleRangeStart, sampleRangeEnd, posInRange + posSpread);
    const int totalUsable = sampleBuffer.getNumSamples() - grain.grainSize;
    grain.startPos = static_cast<int>(actualPosition * static_cast<float>(totalUsable));
    grain.startPos = juce::jlimit(0, juce::jmax(0, totalUsable), grain.startPos);
    
    // Set pitch with spread AND MIDI pitch ratio
    float pitchSpreadAmount = (uniformDist(gen) - 0.5f) * pitchSpread;
    float basePitchRatio = pitch * midiPitchRatio;  // Apply MIDI pitch ratio to ALL grains
    grain.pitch = basePitchRatio * std::pow(2.0f, (pitchSpreadAmount + pitchBendSemitones) / 12.0f);
    
    // Set reverse
    grain.reverse = uniformDist(gen) < reverseChance;
    
    // Set pan with spread
    float panSpreadAmount = (uniformDist(gen) - 0.5f) * panSpread * 2.0f;
    grain.pan = juce::jlimit(-1.0f, 1.0f, pan + panSpreadAmount);
    
    // Set amplitude: scale by MIDI velocity so harder keystrokes produce louder grains
    grain.amplitude = (ecoMode ? 0.42f : 0.5f) * midiNoteVelocity;
}

float GrainEngine::calculateEnvelope(const Grain& grain) const
{
    if (grain.grainSize <= 0) return 0.0f;
    const float t = juce::jlimit(0.0f, 1.0f,
        static_cast<float>(grain.position) / static_cast<float>(grain.grainSize));

    switch (windowType)
    {
        case WindowType::Gaussian:
        {
            // Gaussian window: centred at 0.5, sigma = 0.18
            const float sigma = 0.18f;
            const float x = (t - 0.5f) / sigma;
            return std::exp(-0.5f * x * x);
        }
        case WindowType::Rectangular:
        {
            // Rectangular — no tapering, add tiny fade-in/out to avoid clicks
            if (t < 0.01f) return t / 0.01f;
            if (t > 0.99f) return (1.0f - t) / 0.01f;
            return 1.0f;
        }
        case WindowType::Tukey:
        {
            // Tukey (tapered cosine): 20% cosine ramps, 60% flat top
            const float alpha = 0.2f;
            if (t < alpha * 0.5f)
                return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * t / (alpha * 0.5f)));
            if (t > 1.0f - alpha * 0.5f)
                return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::pi * (1.0f - t) / (alpha * 0.5f)));
            return 1.0f;
        }
        case WindowType::Hanning:
        default:
            // Hann window (default): smooth, click-free
            return 0.5f * (1.0f - std::cos(juce::MathConstants<float>::twoPi * t));
    }
}

void GrainEngine::updateGrainTimer(int numSamples)
{
    // Exponential smoothing on density prevents zipper noise on rapid automation
    smoothedDensity += (grainDensity - smoothedDensity) * densitySmoothCoeff;
    samplesPerGrain = static_cast<float>(sampleRate) / juce::jmax(0.1f, smoothedDensity);
    
    grainTimer += static_cast<float>(numSamples);
    
    while (grainTimer >= samplesPerGrain)
    {
        spawnGrain();
        grainTimer -= samplesPerGrain;
    }
}

int GrainEngine::findInactiveGrain()
{
    for (int i = 0; i < maxGrains; ++i)
    {
        if (!grains[static_cast<size_t>(i)].isActive)
            return i;
    }
    return -1;
}

//==============================================================================
// NEW IMPLEMENTATIONS: ADSR and MIDI Support
//==============================================================================

void GrainEngine::triggerGrainForNote(int noteNumber, float velocity)
{
    int activeCount = 0;
    for (const auto& g : grains) if (g.isActive) ++activeCount;
    if (activeCount >= maxActiveGrains)
        return;

    int grainIndex = findInactiveGrain();
    if (grainIndex == -1)
        return;
        
    spawnGrainForNote(noteNumber, velocity);
    
    // Track this grain with the note
    for (auto& note : activeNotes)
    {
        if (note.noteNumber == noteNumber)
        {
            note.grainIndices.push_back(grainIndex);
            return;
        }
    }
    
    // Create new active note
    ActiveNote newNote;
    newNote.noteNumber = noteNumber;
    newNote.velocity = velocity;
    newNote.grainIndices.push_back(grainIndex);
    activeNotes.push_back(newNote);
}

void GrainEngine::releaseNote(int noteNumber)
{
    for (auto it = activeNotes.begin(); it != activeNotes.end(); ++it)
    {
        if (it->noteNumber == noteNumber)
        {
            // Set all associated grains to release phase
            for (int grainIndex : it->grainIndices)
            {
                if (grainIndex < maxGrains && grains[grainIndex].isActive)
                {
                    grains[grainIndex].isNoteOn = false;
                    grains[grainIndex].adsrPhase = Grain::Release;
                    grains[grainIndex].adsrSamples = 0;
                }
            }
            
            activeNotes.erase(it);
            break;
        }
    }
}

void GrainEngine::spawnGrainForNote(int noteNumber, float velocity)
{
    int grainIndex = findInactiveGrain();
    if (grainIndex == -1)
        return;
        
    auto& grain = grains[grainIndex];
    
    // Basic grain setup (similar to regular spawnGrain)
    grain.isActive = true;
    grain.sampleData = sampleBuffer.getWritePointer(0);
    grain.sampleLength = sampleBuffer.getNumSamples();
    
    // Random position with spread
    float randomOffset = (uniformDist(gen) - 0.5f) * positionSpread;
    float actualPosition = juce::jlimit(0.0f, 1.0f, playbackPosition + randomOffset);
    grain.startPos = static_cast<int>(actualPosition * grain.sampleLength);
    
    // Apply MIDI pitch ratio + pitch bend + regular pitch/spread
    float pitchRandomness = (uniformDist(gen) - 0.5f) * pitchSpread / 12.0f; // semitones to ratio
    float totalPitchRatio = midiPitchRatio * pitch * std::pow(2.0f, (pitchBendSemitones + pitchRandomness) / 12.0f);
    grain.pitch = totalPitchRatio;
    
    // Grain size
    float grainSizeMs = grainSize + (uniformDist(gen) - 0.5f) * 50.0f; // ±25ms randomness
    grain.grainSize = static_cast<int>((grainSizeMs / 1000.0f) * sampleRate);
    
    // Pan with spread
    float panRandomness = (uniformDist(gen) - 0.5f) * panSpread;
    grain.pan = juce::jlimit(-1.0f, 1.0f, pan + panRandomness);
    
    // MIDI-specific properties
    grain.midiNote = noteNumber;
    grain.velocity = velocity;
    grain.isNoteOn = true;
    
    // ADSR setup
    grain.adsrPhase = Grain::Attack;
    grain.adsrValue = 0.0f;
    grain.adsrSamples = 0;
    grain.amplitude = velocity; // Use MIDI velocity
    
    // Initialize other properties
    grain.position = 0;
    grain.age = 0;
    grain.reverse = uniformDist(gen) < reverseChance;
}

float GrainEngine::calculateADSREnvelope(Grain& grain) const
{
    const float sampleRate_f = static_cast<float>(sampleRate);
    
    switch (grain.adsrPhase)
    {
        case Grain::Attack:
        {
            int attackSamples = static_cast<int>((adsrAttack / 1000.0f) * sampleRate_f);
            if (attackSamples <= 0) attackSamples = 1;
            
            grain.adsrValue = static_cast<float>(grain.adsrSamples) / attackSamples;
            grain.adsrSamples++;
            
            if (grain.adsrSamples >= attackSamples)
            {
                grain.adsrPhase = Grain::Decay;
                grain.adsrSamples = 0;
                grain.adsrValue = 1.0f;
            }
            break;
        }
        
        case Grain::Decay:
        {
            int decaySamples = static_cast<int>((adsrDecay / 1000.0f) * sampleRate_f);
            if (decaySamples <= 0) decaySamples = 1;
            
            float decayProgress = static_cast<float>(grain.adsrSamples) / decaySamples;
            grain.adsrValue = 1.0f - decayProgress * (1.0f - adsrSustain);
            grain.adsrSamples++;
            
            if (grain.adsrSamples >= decaySamples || grain.adsrValue <= adsrSustain)
            {
                grain.adsrPhase = Grain::Sustain;
                grain.adsrValue = adsrSustain;
            }
            break;
        }
        
        case Grain::Sustain:
        {
            grain.adsrValue = adsrSustain;
            
            // Automatically go to release if note is off or grain is ending
            if (!grain.isNoteOn)
            {
                grain.adsrPhase = Grain::Release;
                grain.adsrSamples = 0;
            }
            break;
        }
        
        case Grain::Release:
        {
            int releaseSamples = static_cast<int>((adsrRelease / 1000.0f) * sampleRate_f);
            if (releaseSamples <= 0) releaseSamples = 1;
            
            float releaseProgress = static_cast<float>(grain.adsrSamples) / releaseSamples;
            grain.adsrValue = adsrSustain * (1.0f - releaseProgress);
            grain.adsrSamples++;
            
            if (grain.adsrSamples >= releaseSamples || grain.adsrValue <= 0.0f)
            {
                grain.adsrPhase = Grain::Inactive;
                grain.adsrValue = 0.0f;
                return 0.0f; // Signal that grain should be stopped
            }
            break;
        }
        
        case Grain::Inactive:
        default:
            grain.adsrValue = 0.0f;
            return 0.0f;
    }
    
    return juce::jlimit(0.0f, 1.0f, grain.adsrValue);
}

void GrainEngine::updateADSRPhase(Grain& grain) const
{
    // This is called during grain processing to handle phase transitions
    // Most of the work is done in calculateADSREnvelope
    juce::ignoreUnused(grain);
}
