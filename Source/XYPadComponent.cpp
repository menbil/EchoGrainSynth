/*
  ==============================================================================

    XYPadComponent.cpp
    XY Pad control with drag easing and trail effects

  ==============================================================================
*/

#include "XYPadComponent.h"

//==============================================================================
XYPadComponent::XYPadComponent()
{
    setOpaque(false);
}

XYPadComponent::~XYPadComponent() = default;

void XYPadComponent::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    auto padBounds = bounds.reduced(20.0f); // Space for indicators
    
    // Background with neon glow
    juce::ColourGradient bgGradient(
        juce::Colour::fromString("#1A0A2E").withAlpha(0.8f), padBounds.getTopLeft(),
        juce::Colour::fromString("#16213E").withAlpha(0.6f), padBounds.getBottomRight(), false
    );
    g.setGradientFill(bgGradient);
    g.fillRoundedRectangle(padBounds, 5.0f);
    
    // Border with glow
    g.setColour(juce::Colour::fromString("#00FFFF"));
    g.drawRoundedRectangle(padBounds, 5.0f, 1.5f);
    
    // Neon glow effect
    for (int i = 0; i < 3; ++i)
    {
        g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(0.1f - i * 0.03f));
        g.drawRoundedRectangle(padBounds.expanded(i * 2.0f), 5.0f + i, 1.0f);
    }
    
    // Grid lines
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.3f));
    int gridLines = 4;
    for (int i = 1; i < gridLines; ++i)
    {
        float x = padBounds.getX() + (padBounds.getWidth() / gridLines) * i;
        float y = padBounds.getY() + (padBounds.getHeight() / gridLines) * i;
        g.drawLine(x, padBounds.getY(), x, padBounds.getBottom(), 0.5f);
        g.drawLine(padBounds.getX(), y, padBounds.getRight(), y, 0.5f);
    }
    
    // Center crosshair
    auto center = padBounds.getCentre();
    g.setColour(juce::Colour::fromString("#FF006E").withAlpha(0.4f));
    g.drawLine(center.x - 10, center.y, center.x + 10, center.y, 1.0f);
    g.drawLine(center.x, center.y - 10, center.x, center.y + 10, 1.0f);
    
    // Draw trail
    for (const auto& point : trailPoints)
    {
        if (point.alpha > 0.0f)
        {
            auto pos = juce::Point<float>(
                padBounds.getX() + point.position.x * padBounds.getWidth(),
                padBounds.getY() + (1.0f - point.position.y) * padBounds.getHeight()
            );
            
            g.setColour(juce::Colour::fromString("#00FFFF").withAlpha(point.alpha * 0.5f));
            g.fillEllipse(pos.x - 2, pos.y - 2, 4, 4);
        }
    }
    
    // Main position indicator
    auto displayPos = juce::Point<float>(
        padBounds.getX() + displayPosition.x * padBounds.getWidth(),
        padBounds.getY() + (1.0f - displayPosition.y) * padBounds.getHeight()
    );
    
    // Glow ring around indicator
    for (int i = 3; i >= 0; --i)
    {
        float radius = 8.0f + i * 3.0f;
        float alpha = 0.8f - i * 0.2f;
        g.setColour(juce::Colour::fromString("#FF006E").withAlpha(alpha));
        g.fillEllipse(displayPos.x - radius, displayPos.y - radius, radius * 2, radius * 2);
    }
    
    // Main indicator
    g.setColour(juce::Colours::white);
    g.fillEllipse(displayPos.x - 6, displayPos.y - 6, 12, 12);
    
    // Position labels
    g.setColour(juce::Colour::fromString("#00FFFF"));
    g.setFont(juce::Font(juce::FontOptions(10.0f, juce::Font::bold)));

    const auto xLabelArea = juce::Rectangle<float>(bounds.getX(), bounds.getBottom() - 15.0f, 15.0f, 12.0f);
    const auto xValueArea = juce::Rectangle<float>(bounds.getRight() - 30.0f, bounds.getBottom() - 15.0f, 30.0f, 12.0f);
    const auto yLabelArea = juce::Rectangle<float>(bounds.getX(), bounds.getY(), 15.0f, 12.0f);
    const auto yValueArea = juce::Rectangle<float>(bounds.getX(), bounds.getY() + 15.0f, 15.0f, 12.0f);
    
    // X axis indicators
    g.drawText("X", xLabelArea, juce::Justification::centred);
    auto xValueText = juce::String(normalizedPosition.x, 2);
    g.drawText(xValueText, xValueArea, juce::Justification::centred);
    
    // Y axis indicators
    g.drawText("Y", yLabelArea, juce::Justification::centred);
    auto yValueText = juce::String(normalizedPosition.y, 2);
    g.drawText(yValueText, yValueArea, juce::Justification::centred);
}

void XYPadComponent::resized()
{
    // Component resizing doesn't require special handling
}

void XYPadComponent::mouseDown(const juce::MouseEvent& e)
{
    auto bounds = getLocalBounds().toFloat().reduced(20.0f);
    
    if (bounds.contains(e.position))
    {
        isDragging = true;
        
        // Handle Shift for X-only and Alt for Y-only
        auto newPos = juce::Point<float>(
            (e.position.x - bounds.getX()) / bounds.getWidth(),
            1.0f - (e.position.y - bounds.getY()) / bounds.getHeight()
        );
        
        if (e.mods.isShiftDown())
        {
            newPos.y = normalizedPosition.y; // Lock Y
        }
        else if (e.mods.isAltDown())
        {
            newPos.x = normalizedPosition.x; // Lock X
        }
        
        setNormalizedPosition(constrainToBounds(newPos));
    }
}

void XYPadComponent::mouseDrag(const juce::MouseEvent& e)
{
    if (isDragging)
    {
        auto bounds = getLocalBounds().toFloat().reduced(20.0f);
        
        auto newPos = juce::Point<float>(
            (e.position.x - bounds.getX()) / bounds.getWidth(),
            1.0f - (e.position.y - bounds.getY()) / bounds.getHeight()
        );
        
        // Handle modifier keys
        if (e.mods.isShiftDown())
        {
            newPos.y = normalizedPosition.y; // Lock Y
        }
        else if (e.mods.isAltDown())
        {
            newPos.x = normalizedPosition.x; // Lock X
        }
        
        setNormalizedPosition(constrainToBounds(newPos));
        updateTrail();
    }
}

void XYPadComponent::mouseDoubleClick(const juce::MouseEvent&)
{
    // Reset to center
    setNormalizedPosition({0.5f, 0.5f});
}

void XYPadComponent::setNormalizedPosition(juce::Point<float> newPosition, bool sendNotification)
{
    normalizedPosition = constrainToBounds(newPosition);
    displayPosition = normalizedPosition;
    
    if (sendNotification && onPositionChanged)
    {
        onPositionChanged(normalizedPosition);
    }
    
    repaint();
}

void XYPadComponent::resetToCenter(bool sendNotification)
{
    trailPoints.clear();
    setNormalizedPosition({ 0.5f, 0.5f }, sendNotification);
}

juce::Point<float> XYPadComponent::constrainToBounds(juce::Point<float> pos) const
{
    return {
        juce::jlimit(0.0f, 1.0f, pos.x),
        juce::jlimit(0.0f, 1.0f, pos.y)
    };
}

void XYPadComponent::updateTrail()
{
    auto currentTime = juce::Time::getMillisecondCounter();
    
    // Add new trail point
    trailPoints.push_back({normalizedPosition, 1.0f, currentTime});
    
    // Update existing trail points
    for (auto it = trailPoints.begin(); it != trailPoints.end();)
    {
        auto age = currentTime - it->timestamp;
        it->alpha = juce::jmax(0.0f, 1.0f - (age / 500.0f)); // 500ms fade
        
        if (it->alpha <= 0.0f)
        {
            it = trailPoints.erase(it);
        }
        else
        {
            ++it;
        }
    }
    
    // Limit trail points
    if (trailPoints.size() > 20)
    {
        trailPoints.erase(trailPoints.begin());
    }
}
