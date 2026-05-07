/*
  ==============================================================================

    WaveformView.h
    Waveform display with grain visualization overlay and Simpler-style
    sample markers (start / end, draggable).

  ==============================================================================
*/

#pragma once

#include "../JuceLibraryCode/JuceHeader.h"
#include "GrainEngine.h"   // GrainVisualizationPoint, Grain

// Forward declaration — full type available in .cpp via PluginProcessor.h
class EchoGrainSynthAudioProcessor;

//==============================================================================
class WaveformView : public juce::Component,
                     public juce::Timer,
                     public juce::ChangeListener
{
public:
    WaveformView();
    ~WaveformView() override;

    //==========================================================================
    // Processor connection — enables internal grain-data polling
    void setProcessor(EchoGrainSynthAudioProcessor* p);

    // Waveform source
    void setSource(juce::InputSource* newSource);

    // Positions (0..1 normalised)
    void setPlayheadPosition(double position);
    void setGrainSeedPosition(float normPos);   // yellow dashed cursor

    // Zoom
    void zoomIn();
    void zoomOut();
    void fitToView();

    // Accessors
    double getPlayheadPosition() const  { return playheadPosition; }
    float  getSampleStartNorm()  const  { return sampleStartNorm; }
    float  getSampleEndNorm()    const  { return sampleEndNorm; }

    // Programmatic update of both markers (e.g. from preset load)
    void setSampleRange(float startNorm, float endNorm)
    {
        sampleStartNorm = juce::jlimit(0.0f, 1.0f, startNorm);
        sampleEndNorm   = juce::jlimit(sampleStartNorm + 0.01f, 1.0f, endNorm);
        repaint();
    }

    // Callbacks fired when the user drags the start / end markers
    std::function<void(float)> onStartMarkerChanged;
    std::function<void(float)> onEndMarkerChanged;
    std::function<void(float)> onPositionChanged;   // drag de la barre jaune

    //==========================================================================
    void paint   (juce::Graphics& g)  override;
    void resized ()                    override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseWheelMove  (const juce::MouseEvent& e,
                          const juce::MouseWheelDetails& wheel) override;

    void timerCallback()                                              override;
    void changeListenerCallback(juce::ChangeBroadcaster* source)      override;

private:
    //── Waveform ──────────────────────────────────────────────────────────────
    juce::AudioFormatManager     formatManager;
    juce::AudioThumbnailCache    thumbnailCache { 10 };
    std::unique_ptr<juce::AudioThumbnail> thumbnail;

    double playheadPosition  = 0.0;
    float  grainSeedPosition = 0.0f;
    double zoomFactor        = 1.0;
    double viewStart         = 0.0;
    bool   hasValidSource    = false;

    //── Grain visualization ───────────────────────────────────────────────────
    EchoGrainSynthAudioProcessor* processor = nullptr;

    static constexpr int   HISTORY_SIZE    = 120;
    static constexpr int   MAX_GRAIN_SLOTS = 64;
    static constexpr float GRAIN_SMOOTHING = 0.85f;

    std::array<float, HISTORY_SIZE>                  activityHistory  {};
    std::vector<GrainVisualizationPoint>             currentPoints;
    std::array<juce::Point<float>, MAX_GRAIN_SLOTS>  prevPointByIndex {};
    std::array<bool,               MAX_GRAIN_SLOTS>  hadPrevPoint     {};

    int   displayedGrainCount  = 0;
    float animationPhase       = 0.0f;
    float impactFlash          = 0.0f;
    float previousNormActivity = 0.0f;

    //── Sample markers (Simpler-style) ────────────────────────────────────────
    float sampleStartNorm = 0.0f;
    float sampleEndNorm   = 1.0f;

    enum class DragMode { None, StartMarker, EndMarker, Position };
    DragMode dragMode = DragMode::None;

    static constexpr float MARKER_HIT_PX = 10.0f;

    //── Drawing helpers ───────────────────────────────────────────────────────
    void drawGrid            (juce::Graphics& g, juce::Rectangle<int> b);
    void drawSampleZone      (juce::Graphics& g, juce::Rectangle<int> b);
    void drawWaveform        (juce::Graphics& g, juce::Rectangle<int> b);
    void drawGrainDots       (juce::Graphics& g, juce::Rectangle<int> b);
    void drawActivityBar     (juce::Graphics& g, juce::Rectangle<int> b);
    void drawSampleMarkers   (juce::Graphics& g, juce::Rectangle<int> b);
    void drawGrainSeedCursor (juce::Graphics& g, juce::Rectangle<int> b);
    void drawPlayhead        (juce::Graphics& g, juce::Rectangle<int> b);

    // Coordinate helpers (respects viewStart + zoomFactor)
    float normToX (float norm, juce::Rectangle<int> b) const;
    float xToNorm (float x,    juce::Rectangle<int> b) const;

    // Internal grain-data update (called from timerCallback)
    void pollGrainData();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(WaveformView)
};
