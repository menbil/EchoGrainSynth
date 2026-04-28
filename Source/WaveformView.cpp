#include "WaveformView.h"
#include <algorithm>

/*
  ==============================================================================
    WaveformView.cpp
    Waveform display with playhead and zoom controls
  ==============================================================================
*/

WaveformView::WaveformView()
{
    activeGrainRegions.clear();
    formatManager.registerBasicFormats();
    
    // Initialisation du thumbnail avec le cache
    thumbnail = std::make_unique<juce::AudioThumbnail>(512, formatManager, thumbnailCache);
    thumbnail->addChangeListener(this);
    
    startTimerHz(60); // 60 FPS pour une animation fluide
}

WaveformView::~WaveformView()
{
    thumbnail->removeChangeListener(this);
    stopTimer();
}

void WaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    
    // Background avec thème néon
    juce::ColourGradient bgGradient(
        juce::Colour::fromString("#1A0A2E").withAlpha(0.8f), bounds.getTopLeft().toFloat(),
        juce::Colour::fromString("#16213E").withAlpha(0.6f), bounds.getBottomRight().toFloat(), false
    );
    g.setGradientFill(bgGradient);
    g.fillRoundedRectangle(bounds.toFloat(), 3.0f);
    
    // Bordure
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.6f));
    g.drawRoundedRectangle(bounds.toFloat(), 3.0f, 1.0f);
    
    auto waveformArea = bounds.reduced(2);
    
    // Dessin de la grille
    drawGrid(g, waveformArea);
    
    // Dessin du waveform si valide
    if (hasValidSource && thumbnail->getNumSamplesFinished() > 0) 
    {
        drawWaveform(g, waveformArea);
        
        // --- SECTION GRAINS CORRIGÉE ---
        if (!activeGrainRegions.empty())
        {
            for (const auto& region : activeGrainRegions)
            {
                // CORRECTION : Calcul de position prenant en compte viewStart et zoomFactor
                float x1 = waveformArea.getX() + (float)((region.startNorm - viewStart) * zoomFactor * waveformArea.getWidth());
                float x2 = waveformArea.getX() + (float)((region.endNorm - viewStart) * zoomFactor * waveformArea.getWidth());
                
                float width = std::max(2.0f, x2 - x1);
                
                // On ne dessine que si le grain est dans la zone visible
                if (x2 > waveformArea.getX() && x1 < waveformArea.getRight())
                {
                    juce::Colour c = region.reverse
                        ? juce::Colours::orange.withAlpha(0.25f + 0.30f * region.energy)
                        : juce::Colours::cyan.withAlpha(0.25f + 0.30f * region.energy);
                        
                    g.setColour(c);
                    g.fillRect(x1, (float)waveformArea.getY(), width, (float)waveformArea.getHeight());
                    
                    // Optionnel : un petit contour pour mieux les voir
                    g.setColour(c.withAlpha(0.8f));
                    g.drawRect(x1, (float)waveformArea.getY(), width, (float)waveformArea.getHeight(), 1.0f);
                }
            }
        }

        drawPlayhead(g, waveformArea);
    }
}

void WaveformView::setActiveGrainRegions(const std::vector<GrainRegion>& regions)
{
    activeGrainRegions = regions;
    // Pas besoin de repaint() ici car le timerCallback s'en occupe 60 fois par seconde
}

void WaveformView::resized()
{
}

void WaveformView::timerCallback()
{
    repaint();
}

void WaveformView::changeListenerCallback(juce::ChangeBroadcaster* source)
{
    if (source == thumbnail.get())
    {
        hasValidSource = thumbnail->getNumSamplesFinished() > 0;
        repaint();
    }
}

void WaveformView::setSource(juce::InputSource* newSource)
{
    if (newSource != nullptr)
    {
        thumbnail->setSource(newSource);
        hasValidSource = true;
        fitToView();
    }
    else
    {
        thumbnail->clear();
        hasValidSource = false;
    }
    repaint();
}

void WaveformView::setPlayheadPosition(double position)
{
    playheadPosition = juce::jlimit(0.0, 1.0, position);
}

void WaveformView::zoomIn()
{
    zoomFactor = juce::jmin(100.0, zoomFactor * 1.5);
    repaint();
}

void WaveformView::zoomOut()
{
    zoomFactor = juce::jmax(1.0, zoomFactor / 1.5);
    repaint();
}

void WaveformView::fitToView()
{
    zoomFactor = 1.0;
    viewStart = 0.0;
    repaint();
}

void WaveformView::drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!thumbnail || thumbnail->getNumSamplesFinished() == 0)
        return;

    auto duration = thumbnail->getTotalLength();
    if (duration <= 0.0)
        return;

    auto visibleDuration = duration / zoomFactor;
    auto startTime = viewStart * duration;
    auto endTime = std::min(duration, startTime + visibleDuration);
    
    auto waveformBounds = bounds;
    
    // Effet de glow (Halo)
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.1f));
    thumbnail->drawChannels(g, waveformBounds, startTime, endTime, 1.0f);
    
    for (int i = 3; i > 0; --i)
    {
        g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.4f / (float)i));
        thumbnail->drawChannels(g, waveformBounds.expanded(i), startTime, endTime, 1.0f);
    }
    
    g.setColour(juce::Colour::fromString("#00FFFF"));
    thumbnail->drawChannels(g, waveformBounds, startTime, endTime, 1.0f);
}

void WaveformView::drawPlayhead(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    if (!hasValidSource || thumbnail->getTotalLength() <= 0.0)
        return;

    auto duration = thumbnail->getTotalLength();
    auto visibleDuration = duration / zoomFactor;
    auto startTime = viewStart * duration;
    auto playheadTime = playheadPosition * duration;

    if (playheadTime >= startTime && playheadTime <= startTime + visibleDuration)
    {
        const double relativePos = (playheadTime - startTime) / visibleDuration;
        const float x = (float)bounds.getX() + (float)(relativePos * (double)bounds.getWidth());
        
        const float top = (float)bounds.getY();
        const float bottom = (float)bounds.getBottom();

        for (int i = 4; i > 0; --i)
        {
            g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.6f / (float)i));
            g.drawLine(x, top, x, bottom, 2.0f + (float)i);
        }

        g.setColour(juce::Colour::fromString("#FF006E"));
        g.drawLine(x, top, x, bottom, 2.0f);

        juce::Path triangle;
        triangle.addTriangle(x - 4.0f, top, x + 4.0f, top, x, top + 8.0f);
        g.setColour(juce::Colour::fromString("#FF006E"));
        g.fillPath(triangle);
        g.setColour(juce::Colours::white);
        g.strokePath(triangle, juce::PathStrokeType(1.0f));
    }
}

void WaveformView::drawGrid(juce::Graphics& g, juce::Rectangle<int> bounds)
{
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.1f));
    
    const float left = (float)bounds.getX();
    const float right = (float)bounds.getRight();
    const float top = (float)bounds.getY();
    const float bottom = (float)bounds.getBottom();

    for (int i = 1; i < 8; ++i)
    {
        const float x = (float)bounds.getX() + ((float)bounds.getWidth() / 8.0f) * (float)i;
        g.drawLine(x, top, x, bottom, 1.0f);
    }

    const float centerY = (float)bounds.getCentreY();
    g.drawLine(left, centerY, right, centerY, 1.0f);
    
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.05f));
    g.drawLine(left, (float)bounds.getY() + (float)bounds.getHeight() / 4.0f, right, (float)bounds.getY() + (float)bounds.getHeight() / 4.0f, 1.0f);
    g.drawLine(left, (float)bounds.getY() + 3.0f * (float)bounds.getHeight() / 4.0f, right, (float)bounds.getY() + 3.0f * (float)bounds.getHeight() / 4.0f, 1.0f);
}