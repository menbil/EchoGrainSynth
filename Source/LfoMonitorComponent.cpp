#include "LfoMonitorComponent.h"

//==============================================================================
LfoMonitorComponent::LfoMonitorComponent(EchoGrainSynthAudioProcessor& processor)
    : audioProcessor(processor)
{
    setOpaque(false);
    setInterceptsMouseClicks(false, false); // Non-interactive overlay
    startMonitoring();
}

LfoMonitorComponent::~LfoMonitorComponent()
{
    stopTimer();
}

void LfoMonitorComponent::paint(juce::Graphics& g)
{
    // Update parameter values
    auto& apvts = audioProcessor.getValueTreeState();
    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("positionLfoFreq")))
        posLfoFreq = p->get();
    else
        posLfoFreq = 0.0f;

    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("positionLfoDepth")))
        posLfoDepth = p->get();
    else
        posLfoDepth = 0.0f;

    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("pitchLfoFreq")))
        pitchLfoFreq = p->get();
    else
        pitchLfoFreq = 0.0f;

    if (auto* p = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("pitchLfoDepth")))
        pitchLfoDepth = p->get();
    else
        pitchLfoDepth = 0.0f;
    
    auto drawLfoLane = [&g](juce::Rectangle<float> laneBounds,
                            float phase,
                            float depth,
                            float freq,
                            juce::Colour colour,
                            const juce::String& label)
    {
        if (laneBounds.isEmpty())
            return;

        g.setColour(juce::Colour::fromString("#0E101A").withAlpha(0.72f));
        g.fillRoundedRectangle(laneBounds, 4.0f);
        g.setColour(colour.withAlpha(0.28f));
        g.drawRoundedRectangle(laneBounds, 4.0f, 1.0f);

        const auto drawArea = laneBounds.reduced(6.0f, 8.0f);
        g.setColour(colour.withAlpha(0.18f));
        g.drawHorizontalLine(juce::roundToInt(drawArea.getCentreY()), drawArea.getX(), drawArea.getRight());

        juce::Path path;
        const int numPoints = 96;
        const float cyclesAcrossWidth = 2.0f;
        const float amplitude = drawArea.getHeight() * (0.08f + depth * 0.38f);

        for (int i = 0; i < numPoints; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(numPoints - 1);
            const float x = drawArea.getX() + t * drawArea.getWidth();
            const float value = std::sin(phase + t * juce::MathConstants<float>::twoPi * cyclesAcrossWidth);
            const float y = drawArea.getCentreY() - value * amplitude;

            if (i == 0)
                path.startNewSubPath(x, y);
            else
                path.lineTo(x, y);
        }

        g.setColour(colour.withAlpha(0.18f));
        g.strokePath(path, juce::PathStrokeType(4.0f));
        g.setColour(colour.withAlpha(0.92f));
        g.strokePath(path, juce::PathStrokeType(1.6f));

        const float markerY = drawArea.getCentreY() - std::sin(phase) * amplitude;
        g.setColour(colour.withAlpha(0.95f));
        g.fillEllipse(drawArea.getRight() - 3.0f, markerY - 3.0f, 6.0f, 6.0f);

        g.setColour(colour.withAlpha(0.86f));
        g.setFont(juce::Font(juce::FontOptions(9.5f, juce::Font::bold)));
        const auto text = label + " " + juce::String(freq, 1) + "Hz";
        g.drawText(text, laneBounds.removeFromTop(12.0f), juce::Justification::centredLeft, false);
    };

    drawLfoLane(posLfoBounds, posLfoPhase, juce::jlimit(0.0f, 1.0f, posLfoDepth), posLfoFreq,
                juce::Colour::fromString("#FF9B1A"), "POS");
    drawLfoLane(pitchLfoBounds, pitchLfoPhase, juce::jlimit(0.0f, 1.0f, pitchLfoDepth), pitchLfoFreq,
                juce::Colour::fromString("#FFD088"), "PITCH");
}

void LfoMonitorComponent::timerCallback()
{
    // Update LFO phases for smooth animation
    const float deltaTime = 1.0f / 60.0f; // 60 FPS
    
    posLfoPhase += juce::MathConstants<float>::twoPi * posLfoFreq * deltaTime;
    pitchLfoPhase += juce::MathConstants<float>::twoPi * pitchLfoFreq * deltaTime;
    
    // Keep phases in range
    while (posLfoPhase > juce::MathConstants<float>::twoPi)
        posLfoPhase -= juce::MathConstants<float>::twoPi;
    while (pitchLfoPhase > juce::MathConstants<float>::twoPi)
        pitchLfoPhase -= juce::MathConstants<float>::twoPi;
    
    repaint();
}

void LfoMonitorComponent::resized()
{
    auto bounds = getLocalBounds().toFloat();
    
    // Position LFO bounds (left half)
    posLfoBounds = bounds.withWidth(bounds.getWidth() * 0.5f).reduced(10.0f);
    
    // Pitch LFO bounds (right half)
    pitchLfoBounds = bounds.withX(bounds.getWidth() * 0.5f).withWidth(bounds.getWidth() * 0.5f).reduced(10.0f);
}

void LfoMonitorComponent::startMonitoring()
{
    startTimerHz(60); // 60 FPS for smooth animation
}

void LfoMonitorComponent::stopMonitoring()
{
    stopTimer();
}
