#include "WaveformView.h"
#include "PluginProcessor.h"
#include <algorithm>

/*
  ==============================================================================
    WaveformView.cpp
    Waveform + grain-visualizer + Simpler-style sample markers
  ==============================================================================
*/

WaveformView::WaveformView()
{
    formatManager.registerBasicFormats();
    thumbnail = std::make_unique<juce::AudioThumbnail>(512, formatManager, thumbnailCache);
    thumbnail->addChangeListener(this);
    setMouseCursor(juce::MouseCursor::NormalCursor);
    startTimerHz(60);
}

WaveformView::~WaveformView()
{
    thumbnail->removeChangeListener(this);
    stopTimer();
}

//==============================================================================
void WaveformView::setProcessor(EchoGrainSynthAudioProcessor* p) { processor = p; }

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

void WaveformView::setGrainSeedPosition(float normPos)
{
    grainSeedPosition = juce::jlimit(0.0f, 1.0f, normPos);
}

void WaveformView::zoomIn()  { zoomFactor = juce::jmin(100.0, zoomFactor * 1.5); repaint(); }
void WaveformView::zoomOut() { zoomFactor = juce::jmax(1.0,   zoomFactor / 1.5); repaint(); }
void WaveformView::fitToView() { zoomFactor = 1.0; viewStart = 0.0; repaint(); }

//==============================================================================
// Coordinate helpers

float WaveformView::normToX(float norm, juce::Rectangle<int> b) const
{
    return b.getX() + (float)((norm - viewStart) * zoomFactor * b.getWidth());
}

float WaveformView::xToNorm(float x, juce::Rectangle<int> b) const
{
    return (float)viewStart + (x - b.getX()) / (float)(zoomFactor * b.getWidth());
}

//==============================================================================
// Timer / ChangeListener

void WaveformView::timerCallback()
{
    animationPhase += 0.033f;
    if (animationPhase > juce::MathConstants<float>::twoPi)
        animationPhase -= juce::MathConstants<float>::twoPi;

    impactFlash = juce::jmax(0.0f, impactFlash - 0.06f);

    pollGrainData();
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

void WaveformView::pollGrainData()
{
    if (processor == nullptr) return;
    auto* engine = processor->getGrainEngine();
    if (engine == nullptr) return;

    currentPoints = engine->getVisualizationPoints();
    const int count = static_cast<int>(currentPoints.size());

    displayedGrainCount = static_cast<int>(
        displayedGrainCount * GRAIN_SMOOTHING + count * (1.0f - GRAIN_SMOOTHING));

    const float normAct = juce::jlimit(0.0f, 1.0f,
        static_cast<float>(displayedGrainCount) / static_cast<float>(MAX_GRAIN_SLOTS));
    const float attack = juce::jmax(0.0f, normAct - previousNormActivity);
    previousNormActivity = normAct;

    if (attack > 0.04f)
        impactFlash = juce::jlimit(0.0f, 1.0f, impactFlash + attack * 4.0f);

    std::rotate(activityHistory.begin(), activityHistory.begin() + 1, activityHistory.end());
    activityHistory.back() = normAct;
}

//==============================================================================
// paint

void WaveformView::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds();

    // Background
    juce::ColourGradient bg(
        juce::Colour::fromString("#1A0A2E").withAlpha(0.94f), bounds.getTopLeft().toFloat(),
        juce::Colour::fromString("#0C1B2A").withAlpha(0.97f), bounds.getBottomRight().toFloat(), false);
    g.setGradientFill(bg);
    g.fillRoundedRectangle(bounds.toFloat(), 4.0f);

    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.5f));
    g.drawRoundedRectangle(bounds.toFloat(), 4.0f, 1.0f);

    auto inner = bounds.reduced(2);

    // Activity bar (bottom strip)
    const int actBarH = 20;
    auto actBar = inner.removeFromBottom(actBarH);
    inner.removeFromBottom(2);
    drawActivityBar(g, actBar);

    // Waveform region
    drawGrid(g, inner);
    drawSampleZone(g, inner);
    if (hasValidSource && thumbnail->getNumSamplesFinished() > 0)
        drawWaveform(g, inner);

    drawGrainDots(g, inner);
    drawSampleMarkers(g, inner);
    drawGrainSeedCursor(g, inner);
    drawPlayhead(g, inner);
}

void WaveformView::resized() {}

//==============================================================================
// Drawing implementations

void WaveformView::drawGrid(juce::Graphics& g, juce::Rectangle<int> b)
{
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.08f));
    for (int i = 1; i < 8; ++i)
    {
        const float x = b.getX() + (float)b.getWidth() / 8.0f * (float)i;
        g.drawLine(x, (float)b.getY(), x, (float)b.getBottom(), 1.0f);
    }
    const float cy = (float)b.getCentreY();
    g.drawLine((float)b.getX(), cy, (float)b.getRight(), cy, 1.0f);
}

void WaveformView::drawSampleZone(juce::Graphics& g, juce::Rectangle<int> b)
{
    const float x1 = normToX(sampleStartNorm, b);
    const float x2 = normToX(sampleEndNorm,   b);

    // Darken the regions outside start/end
    if (x1 > b.getX())
    {
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillRect((float)b.getX(), (float)b.getY(), x1 - b.getX(), (float)b.getHeight());
    }
    if (x2 < b.getRight())
    {
        g.setColour(juce::Colours::black.withAlpha(0.38f));
        g.fillRect(x2, (float)b.getY(), (float)b.getRight() - x2, (float)b.getHeight());
    }

    // Tint the active zone
    if (x2 > x1)
    {
        g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.055f));
        g.fillRect(x1, (float)b.getY(), x2 - x1, (float)b.getHeight());
    }
}

void WaveformView::drawWaveform(juce::Graphics& g, juce::Rectangle<int> b)
{
    const double duration = thumbnail->getTotalLength();
    if (duration <= 0.0) return;

    const double visibleDur = duration / zoomFactor;
    const double startTime  = viewStart * duration;
    const double endTime    = std::min(duration, startTime + visibleDur);

    // Glow passes
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.07f));
    thumbnail->drawChannels(g, b, startTime, endTime, 1.0f);

    for (int i = 3; i > 0; --i)
    {
        g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.32f / (float)i));
        thumbnail->drawChannels(g, b.expanded(i), startTime, endTime, 1.0f);
    }

    g.setColour(juce::Colour::fromString("#00FFFF"));
    thumbnail->drawChannels(g, b, startTime, endTime, 1.0f);
}

void WaveformView::drawGrainDots(juce::Graphics& g, juce::Rectangle<int> b)
{
    if (processor == nullptr || currentPoints.empty()) return;
    auto* engine = processor->getGrainEngine();
    if (engine == nullptr) return;

    const auto& sample    = processor->getLoadedSample();
    const int   sampleLen = sample.getNumSamples();
    if (sampleLen == 0) return;

    const auto& grains = engine->getGrains();

    std::array<bool, MAX_GRAIN_SLOTS> seenThisFrame {};
    seenThisFrame.fill(false);

    for (const auto& pt : currentPoints)
    {
        if (pt.index < 0 || pt.index >= MAX_GRAIN_SLOTS) continue;

        const auto& grain = grains[pt.index];
        if (!grain.isActive || grain.grainSize <= 0) continue;

        // Map to waveform x-axis using the grain's current read position
        const float centerNorm = ((float)grain.startPos + (float)grain.grainSize * pt.progress)
                                  / (float)sampleLen;
        const float x = normToX(centerNorm, b);
        if (x < (float)b.getX() || x > (float)b.getRight()) continue;

        // Y driven by pan (0=left, 1=right → bottom to top)
        const float y = (float)b.getBottom() - pt.panNorm * (float)b.getHeight();

        const float alpha  = juce::jlimit(0.15f, 1.0f, 0.3f + pt.energy * 0.8f);
        const float radius = 1.8f + pt.energy * 4.2f;
        const auto  core   = pt.reverse
                             ? juce::Colour::fromString("#FF2D8F")
                             : juce::Colour::fromString("#00E5FF");

        // Trail from previous frame
        if (hadPrevPoint[pt.index])
        {
            const auto& prev = prevPointByIndex[pt.index];
            const float px = (float)b.getX() + prev.x * (float)b.getWidth();
            const float py = (float)b.getBottom() - prev.y * (float)b.getHeight();
            g.setColour(core.withAlpha(0.22f * alpha));
            g.drawLine(px, py, x, y, juce::jmax(0.6f, radius * 0.3f));
        }

        // Halo + core dot
        g.setColour(core.withAlpha(0.13f * alpha));
        g.fillEllipse(x - radius * 2.3f, y - radius * 2.3f, radius * 4.6f, radius * 4.6f);
        g.setColour(core.withAlpha(alpha));
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        prevPointByIndex[pt.index] = { centerNorm, pt.panNorm };
        hadPrevPoint[pt.index]     = true;
        seenThisFrame[pt.index]    = true;
    }

    for (size_t i = 0; i < seenThisFrame.size(); ++i)
        if (!seenThisFrame[i])
            hadPrevPoint[i] = false;
}

void WaveformView::drawActivityBar(juce::Graphics& g, juce::Rectangle<int> b)
{
    // Dark panel
    g.setColour(juce::Colour::fromString("#07111D").withAlpha(0.92f));
    g.fillRoundedRectangle(b.toFloat(), 2.0f);
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.22f));
    g.drawRoundedRectangle(b.toFloat(), 2.0f, 1.0f);

    auto inner = b.reduced(2, 1);

    // Scrolling history
    for (int i = 0; i < HISTORY_SIZE; ++i)
    {
        const float t      = static_cast<float>(i) / static_cast<float>(HISTORY_SIZE - 1);
        const float x      = static_cast<float>(inner.getX()) + t * static_cast<float>(inner.getWidth());
        const float energy = activityHistory[i];
        const float halfH  = juce::jmax(1.0f, energy * static_cast<float>(inner.getHeight()) * 0.46f);
        const float cy     = static_cast<float>(inner.getCentreY());

        const float fade  = 0.2f + 0.8f * t;
        const float pulse = 0.6f + 0.4f * std::sin(animationPhase * 4.0f + t * 14.0f);

        g.setColour(juce::Colour::fromString("#00E5FF").withAlpha((0.14f + energy * 0.68f) * fade * pulse));
        g.fillRect(x, cy - halfH, 2.2f, halfH);
        g.setColour(juce::Colour::fromString("#FF2D8F").withAlpha((0.10f + energy * 0.52f) * fade * pulse));
        g.fillRect(x, cy, 2.2f, halfH);
    }

    // Grain count label
    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.75f));
    g.drawText(juce::String(displayedGrainCount) + " / " + juce::String(MAX_GRAIN_SLOTS) + " GRAINS",
               b.reduced(4, 0), juce::Justification::centredRight);
}

void WaveformView::drawSampleMarkers(juce::Graphics& g, juce::Rectangle<int> b)
{
    const float top    = (float)b.getY();
    const float bottom = (float)b.getBottom();

    // ── Start marker (green) ──────────────────────────────────────────────
    const float sx = normToX(sampleStartNorm, b);
    g.setColour(juce::Colour::fromString("#00FF88").withAlpha(0.92f));
    g.drawLine(sx, top, sx, bottom, 2.0f);

    juce::Path startHandle;
    startHandle.addTriangle(sx, top, sx, top + 14.0f, sx + 9.0f, top + 7.0f);
    g.fillPath(startHandle);

    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText("S", (int)sx + 2, (int)top, 14, 14, juce::Justification::centredLeft);

    // ── End marker (orange) ───────────────────────────────────────────────
    const float ex = normToX(sampleEndNorm, b);
    g.setColour(juce::Colour::fromString("#FF8800").withAlpha(0.92f));
    g.drawLine(ex, top, ex, bottom, 2.0f);

    juce::Path endHandle;
    endHandle.addTriangle(ex, top, ex, top + 14.0f, ex - 9.0f, top + 7.0f);
    g.fillPath(endHandle);

    g.setFont(juce::FontOptions(8.0f).withStyle("Bold"));
    g.drawText("E", (int)ex - 16, (int)top, 14, 14, juce::Justification::centredRight);
}

void WaveformView::drawGrainSeedCursor(juce::Graphics& g, juce::Rectangle<int> b)
{
    const float x = normToX(grainSeedPosition, b);
    if (x < (float)b.getX() || x > (float)b.getRight()) return;

    const float top    = (float)b.getY();
    const float bottom = (float)b.getBottom();

    // Dashed yellow line
    float y = top; bool draw = true;
    while (y < bottom)
    {
        const float segEnd = std::min(y + 5.0f, bottom);
        if (draw)
        {
            g.setColour(juce::Colour::fromString("#FFFF00").withAlpha(0.72f));
            g.drawLine(x, y, x, segEnd, 1.5f);
        }
        y = segEnd; draw = !draw;
    }

    // Diamond handle
    const float r = 5.0f, cy = top + 9.0f;
    juce::Path diamond;
    diamond.addTriangle(x, cy - r, x + r, cy, x, cy + r);
    diamond.addTriangle(x, cy - r, x - r, cy, x, cy + r);
    g.setColour(juce::Colour::fromString("#FFFF00").withAlpha(0.88f));
    g.fillPath(diamond);
}

void WaveformView::drawPlayhead(juce::Graphics& g, juce::Rectangle<int> b)
{
    if (!hasValidSource || thumbnail->getTotalLength() <= 0.0) return;

    const double duration   = thumbnail->getTotalLength();
    const double visibleDur = duration / zoomFactor;
    const double startTime  = viewStart * duration;
    const double playTime   = playheadPosition * duration;

    if (playTime < startTime || playTime > startTime + visibleDur) return;

    const double relPos = (playTime - startTime) / visibleDur;
    const float x   = (float)b.getX() + (float)(relPos * b.getWidth());
    const float top = (float)b.getY(), bottom = (float)b.getBottom();

    for (int i = 4; i > 0; --i)
    {
        g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.5f / (float)i));
        g.drawLine(x, top, x, bottom, 2.0f + (float)i);
    }
    g.setColour(juce::Colour::fromString("#FF006E"));
    g.drawLine(x, top, x, bottom, 2.0f);

    juce::Path tri;
    tri.addTriangle(x - 4.0f, top, x + 4.0f, top, x, top + 8.0f);
    g.setColour(juce::Colour::fromString("#FF006E"));
    g.fillPath(tri);
    g.setColour(juce::Colours::white);
    g.strokePath(tri, juce::PathStrokeType(1.0f));
}

//==============================================================================
// Mouse interaction for start / end markers

void WaveformView::mouseDown(const juce::MouseEvent& e)
{
    auto b = getLocalBounds().reduced(2);
    b.removeFromBottom(22);  // activity bar + gap

    const float mx     = (float)e.x;
    const float startX = normToX(sampleStartNorm, b);
    const float endX   = normToX(sampleEndNorm,   b);

    if      (std::abs(mx - startX) <= MARKER_HIT_PX) dragMode = DragMode::StartMarker;
    else if (std::abs(mx - endX)   <= MARKER_HIT_PX) dragMode = DragMode::EndMarker;
    else
    {
        dragMode = DragMode::Position;
        // Mise à jour immédiate au clic
        const float norm = juce::jlimit(0.0f, 1.0f, xToNorm((float)e.x, b));
        grainSeedPosition = norm;
        if (onPositionChanged) onPositionChanged(norm);
        repaint();
    }
}

void WaveformView::mouseDrag(const juce::MouseEvent& e)
{
    if (dragMode == DragMode::None) return;

    auto b = getLocalBounds().reduced(2);
    b.removeFromBottom(22);

    const float norm = juce::jlimit(0.0f, 1.0f, xToNorm((float)e.x, b));

    if (dragMode == DragMode::StartMarker)
    {
        sampleStartNorm = std::min(norm, sampleEndNorm - 0.01f);
        if (onStartMarkerChanged) onStartMarkerChanged(sampleStartNorm);
    }
    else if (dragMode == DragMode::EndMarker)
    {
        sampleEndNorm = std::max(norm, sampleStartNorm + 0.01f);
        if (onEndMarkerChanged) onEndMarkerChanged(sampleEndNorm);
    }
    else if (dragMode == DragMode::Position)
    {
        grainSeedPosition = norm;
        if (onPositionChanged) onPositionChanged(norm);
    }
    repaint();
}

void WaveformView::mouseUp(const juce::MouseEvent&) { dragMode = DragMode::None; }

void WaveformView::mouseWheelMove(const juce::MouseEvent& e,
                                    const juce::MouseWheelDetails& wheel)
{
    // Shift+scroll (or horizontal scroll) → pan le long de la forme d'onde
    // Scroll vertical → zoom centré sur la souris
    const double visibleRange = 1.0 / zoomFactor;

    if (e.mods.isShiftDown() || wheel.isReversed || std::abs(wheel.deltaX) > std::abs(wheel.deltaY))
    {
        // Pan horizontal
        const double scrollAmount = (double)(-wheel.deltaX - wheel.deltaY) * visibleRange * 0.5;
        viewStart = juce::jlimit(0.0, 1.0 - visibleRange, viewStart + scrollAmount);
    }
    else
    {
        // Zoom : centré sur la position horizontale de la souris
        const float normUnderMouse = xToNorm((float)e.x, getLocalBounds().reduced(2).withTrimmedBottom(22));
        const double zoomDelta = (double)wheel.deltaY * 0.3;
        zoomFactor = juce::jlimit(1.0, 32.0, zoomFactor * std::pow(2.0, zoomDelta));
        const double newVisibleRange = 1.0 / zoomFactor;
        // Garder normUnderMouse au même endroit à l'écran
        viewStart = juce::jlimit(0.0, 1.0 - newVisibleRange,
                                 (double)normUnderMouse - newVisibleRange * 0.5);
    }

    repaint();
}

void WaveformView::mouseMove(const juce::MouseEvent& e)
{
    auto b = getLocalBounds().reduced(2);
    b.removeFromBottom(22);

    const float mx     = (float)e.x;
    const float startX = normToX(sampleStartNorm, b);
    const float endX   = normToX(sampleEndNorm,   b);

    const bool nearMarker = std::abs(mx - startX) <= MARKER_HIT_PX
                         || std::abs(mx - endX)   <= MARKER_HIT_PX;
    setMouseCursor(nearMarker ? juce::MouseCursor::LeftRightResizeCursor
                              : juce::MouseCursor::PointingHandCursor);
}