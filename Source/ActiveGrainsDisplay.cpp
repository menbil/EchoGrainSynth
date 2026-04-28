#include "ActiveGrainsDisplay.h"

//==============================================================================
ActiveGrainsDisplay::ActiveGrainsDisplay()
    : audioProcessor(nullptr)
{
    startTimerHz(UPDATE_RATE_HZ);
}

ActiveGrainsDisplay::~ActiveGrainsDisplay()
{
    stopTimer();
}

void ActiveGrainsDisplay::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced(6.0f, 4.0f);

    auto headerRow = bounds.removeFromTop(16.0f);
    g.setColour(cyanColor.withAlpha(0.9f));
    g.setFont(juce::FontOptions(12.0f).withStyle("Bold"));
    g.drawText("ACTIVE GRAINS", headerRow.removeFromLeft(120.0f), juce::Justification::centredLeft);

    g.setColour(cyanColor);
    g.setFont(juce::FontOptions(14.0f).withStyle("Bold"));
    g.drawText(juce::String(displayedGrainCount), headerRow, juce::Justification::centredRight);

    bounds.removeFromTop(4.0f);

    auto vizBounds = bounds.reduced(0.0f, 1.0f);
    juce::ColourGradient panelFill(juce::Colour::fromString("#07111D").withAlpha(0.96f), vizBounds.getTopLeft(),
                                   juce::Colour::fromString("#0C1B2A").withAlpha(0.96f), vizBounds.getBottomRight(), false);
    g.setGradientFill(panelFill);
    g.fillRoundedRectangle(vizBounds, 3.0f);
    g.setColour(cyanColor.withAlpha(0.34f));
    g.drawRoundedRectangle(vizBounds, 3.0f, 1.0f);

    auto inner = vizBounds.reduced(4.0f, 3.0f);

    // Grid similar to XY pad language.
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.18f));
    for (int i = 1; i < 6; ++i)
    {
        const float x = inner.getX() + inner.getWidth() * (static_cast<float>(i) / 6.0f);
        g.drawVerticalLine(static_cast<int>(x), inner.getY(), inner.getBottom());
    }

    g.setColour(cyanColor.withAlpha(0.15f));
    for (int i = 1; i < 3; ++i)
    {
        const float y = inner.getY() + inner.getHeight() * (static_cast<float>(i) / 3.0f);
        g.drawHorizontalLine(static_cast<int>(y), inner.getX(), inner.getRight());
    }

    if (impactFlash > 0.001f)
    {
        juce::ColourGradient impactLayer(juce::Colour::fromString("#FF006E").withAlpha(0.24f * impactFlash),
                                         inner.getCentreX(), inner.getCentreY(),
                                         juce::Colours::transparentBlack,
                                         inner.getX(), inner.getY(), true);
        g.setGradientFill(impactLayer);
        g.fillRoundedRectangle(inner, 2.0f);
    }

    // Activity history from real active-grain count.
    for (int i = 0; i < HISTORY_SIZE; ++i)
    {
        const float t = static_cast<float>(i) / static_cast<float>(HISTORY_SIZE - 1);
        const float x = inner.getX() + t * inner.getWidth();
        const float energy = activityHistory[i];
        const float halfHeight = juce::jmax(1.0f, energy * inner.getHeight() * 0.46f);
        const float centreY = inner.getCentreY();
        auto upper = juce::Rectangle<float>(x, centreY - halfHeight, 2.5f, halfHeight);
        auto lower = juce::Rectangle<float>(x, centreY, 2.5f, halfHeight);

        const float fade = 0.2f + 0.8f * t;
        const float pulse = 0.55f + 0.45f * std::sin(animationPhase * 4.2f + t * 16.0f);
        auto topColour = juce::Colour::fromString("#00E5FF").withAlpha((0.16f + energy * 0.72f) * fade * pulse);
        auto bottomColour = juce::Colour::fromString("#FF2D8F").withAlpha((0.11f + energy * 0.6f) * fade * pulse);

        g.setColour(topColour);
        g.fillRect(upper);
        g.setColour(bottomColour);
        g.fillRect(lower);
    }

    // Real grain points from GrainEngine snapshot.
    std::array<bool, MAX_GRAIN_SLOTS> seenThisFrame{};
    for (const auto& point : currentPoints)
    {
        if (point.index < 0 || point.index >= MAX_GRAIN_SLOTS)
            continue;

        const float x = inner.getX() + point.progress * inner.getWidth();
        const float y = inner.getBottom() - point.panNorm * inner.getHeight();
        const float alpha = juce::jlimit(0.12f, 1.0f, 0.25f + point.energy * 0.85f);
        const float radius = 1.8f + point.energy * 4.6f;

        const auto core = point.reverse
            ? juce::Colour::fromString("#FF2D8F")
            : juce::Colour::fromString("#00E5FF");

        if (hadPreviousPoint[static_cast<size_t>(point.index)])
        {
            const auto prev = previousPointByIndex[static_cast<size_t>(point.index)];
            const float px = inner.getX() + prev.x * inner.getWidth();
            const float py = inner.getBottom() - prev.y * inner.getHeight();
            g.setColour(core.withAlpha(0.30f * alpha));
            g.drawLine(px, py, x, y, juce::jmax(0.8f, radius * 0.35f));
        }

        g.setColour(core.withAlpha(0.16f * alpha));
        g.fillEllipse(x - radius * 2.4f, y - radius * 2.4f, radius * 4.8f, radius * 4.8f);
        g.setColour(core.withAlpha(alpha));
        g.fillEllipse(x - radius, y - radius, radius * 2.0f, radius * 2.0f);

        previousPointByIndex[static_cast<size_t>(point.index)] = { point.progress, point.panNorm };
        hadPreviousPoint[static_cast<size_t>(point.index)] = true;
        seenThisFrame[static_cast<size_t>(point.index)] = true;
    }

    for (size_t i = 0; i < seenThisFrame.size(); ++i)
    {
        if (!seenThisFrame[i])
            hadPreviousPoint[i] = false;
    }

}

void ActiveGrainsDisplay::resized()
{
    // Rien de spécial pour le layout
}

void ActiveGrainsDisplay::timerCallback()
{
    animationPhase += 0.33f;
    if (animationPhase > juce::MathConstants<float>::twoPi)
        animationPhase -= juce::MathConstants<float>::twoPi;

    impactFlash = juce::jmax(0.0f, impactFlash - 0.08f);

    updateGrainCount();
    repaint();
}

void ActiveGrainsDisplay::updateGrainCount()
{
    currentPoints.clear();

    // Read active grains from the real engine state.
    if (audioProcessor != nullptr)
    {
        if (auto* engine = audioProcessor->getGrainEngine())
            currentPoints = engine->getVisualizationPoints();

        currentGrainCount = static_cast<int>(currentPoints.size());
    }
    else
    {
        currentGrainCount = 0;
    }
    
    // Lissage pour éviter les variations trop brusques
    displayedGrainCount = static_cast<int>(
        displayedGrainCount * SMOOTHING_FACTOR + 
        currentGrainCount * (1.0f - SMOOTHING_FACTOR)
    );

    const float normalizedActivity = juce::jlimit(0.0f, 1.0f, static_cast<float>(displayedGrainCount) / static_cast<float>(MAX_GRAIN_SLOTS));
    const float attackAmount = juce::jmax(0.0f, normalizedActivity - previousNormalizedActivity);
    previousNormalizedActivity = normalizedActivity;

    if (attackAmount > 0.04f)
        impactFlash = juce::jlimit(0.0f, 1.0f, impactFlash + attackAmount * 4.2f);

    std::rotate(activityHistory.begin(), activityHistory.begin() + 1, activityHistory.end());
    activityHistory.back() = normalizedActivity;
}
