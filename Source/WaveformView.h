/*
  ==============================================================================

    WaveformView.h
    Waveform display with playhead and zoom controls

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"

//==============================================================================
// Structure pour visualiser les grains sur la waveform
struct GrainRegion {
  float startNorm = 0.0f; // Position de départ normalisée (0..1)
  float endNorm = 0.0f;   // Position de fin normalisée (0..1)
  float energy = 1.0f;    // Pour alpha/couleur
  bool reverse = false;
};

class WaveformView : public juce::Component,
                     public juce::Timer,
                     public juce::ChangeListener
{
public:
    WaveformView();
    ~WaveformView() override;
  // Permet d'injecter les grains actifs à afficher
  void setActiveGrainRegions(const std::vector<GrainRegion>& regions);

    void paint(juce::Graphics& g) override;
    void resized() override;
    
    void timerCallback() override;
    void changeListenerCallback(juce::ChangeBroadcaster* source) override;
    
    void setSource(juce::InputSource* newSource);
    void setPlayheadPosition(double position);
    
    void zoomIn();
    void zoomOut();
    void fitToView();
    
    double getPlayheadPosition() const { return playheadPosition; }

private:
    juce::AudioFormatManager formatManager;
    juce::AudioThumbnailCache thumbnailCache { 10 };
    std::unique_ptr<juce::AudioThumbnail> thumbnail;
      std::vector<GrainRegion> activeGrainRegions;
    
    double playheadPosition = 0.0;
    double zoomFactor = 1.0;
    double viewStart = 0.0;
    
    bool hasValidSource = false;
    
    void drawWaveform(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawPlayhead(juce::Graphics& g, juce::Rectangle<int> bounds);
    void drawGrid(juce::Graphics& g, juce::Rectangle<int> bounds);
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
