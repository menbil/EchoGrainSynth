/*
  ==============================================================================

    XYPadComponent.h
    XY Pad control with drag easing and trail effects

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

//==============================================================================
class XYPadComponent : public juce::Component
{
public:
    XYPadComponent();
    ~XYPadComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent& e) override;

    // Get normalized coordinates (0-1)
    juce::Point<float> getNormalizedPosition() const { return normalizedPosition; }
    
    // Set position programmatically
    void setNormalizedPosition(juce::Point<float> newPosition, bool sendNotification = true);
    void resetToCenter(bool sendNotification = false);
    
    // Callbacks
    std::function<void(juce::Point<float>)> onPositionChanged;

private:
    juce::Point<float> normalizedPosition{0.5f, 0.5f}; // Center by default
    juce::Point<float> displayPosition{0.5f, 0.5f};
    
    // Trail effect
    struct TrailPoint
    {
        juce::Point<float> position;
        float alpha;
        juce::uint32 timestamp;
    };
    
    std::vector<TrailPoint> trailPoints;
    
    // Easing
    bool isDragging = false;
    juce::Point<float> targetPosition{0.5f, 0.5f};
    
    void updateTrail();
    void updateEasing();
    juce::Point<float> constrainToBounds(juce::Point<float> pos) const;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(XYPadComponent)
};
