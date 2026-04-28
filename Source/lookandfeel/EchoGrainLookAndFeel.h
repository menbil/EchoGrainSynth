#pragma once

#include <JuceHeader.h>

class EchoGrainLookAndFeel : public juce::LookAndFeel_V4
{
public:
    EchoGrainLookAndFeel();
    ~EchoGrainLookAndFeel() override = default;

    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                          float sliderPosProportional, float rotaryStartAngle,
                          float rotaryEndAngle, juce::Slider& slider) override;
    void drawButtonBackground(juce::Graphics& g, juce::Button& button,
                              const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted,
                              bool shouldDrawButtonAsDown) override;
    void drawButtonText(juce::Graphics& g, juce::TextButton& button,
                        bool shouldDrawButtonAsHighlighted,
                        bool shouldDrawButtonAsDown) override;
    void drawComboBox(juce::Graphics& g, int width, int height, bool,
                      int, int, int, int, juce::ComboBox& box) override;
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
    juce::Font getLabelFont(juce::Label& label) override;

private:
    juce::Colour findAccentForSlider(const juce::Slider& slider) const;
};