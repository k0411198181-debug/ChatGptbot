#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class GalaxyLookAndFeel : public juce::LookAndFeel_V4
{
public:
    GalaxyLookAndFeel();
    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class KGVocalEngineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit KGVocalEngineAudioProcessorEditor(KGVocalEngineAudioProcessor&);
    ~KGVocalEngineAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureKnob(juce::Slider&, juce::Label&, const juce::String& name, bool hero = false);
    void applyGoldenSettings();
    void setMainKnobsTo50();
    void showTopMenu();
    void updateSpectrum();
    void drawSpectrumAndMeters(juce::Graphics& g);

    KGVocalEngineAudioProcessor& processor;
    GalaxyLookAndFeel galaxyLnf;

    std::array<juce::Slider, 7> knobs;
    std::array<juce::Label, 7> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> attachments;

    juce::Slider inputKnob, mixKnob, outputKnob, throwKnob;
    juce::Label inputLabel, mixLabel, outputLabel, throwLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment, mixAttachment, outputAttachment, throwAttachment;

    juce::ToggleButton autoButton { "AUTO" }, liveButton { "LIVE FX" }, syncButton { "BPM SYNC" }, bypassButton { "BYPASS" };
    juce::TextButton goldenButton { "GOLDEN" }, infoButton { "INFO" }, menuButton { "MENU" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAttachment, liveAttachment, syncAttachment, bypassAttachment;
    bool showHints = true;

    const std::array<juce::String, 7> ids { "clean","body","air","size","width","delay","space" };
    const std::array<juce::String, 7> names { "CLEAN","BODY","AIR","SIZE","WIDTH","DELAY","SPACE" };

    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int spectrumBands = 56;

    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow { fftSize, juce::dsp::WindowingFunction<float>::hann, true };
    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> fftInput {};
    std::array<float, 4096> visualPullBuffer {};
    std::array<float, spectrumBands> spectrumValues {};
    int fftInputPos = 0;

    float smoothInL = 0.0f, smoothInR = 0.0f;
    float smoothOutL = 0.0f, smoothOutR = 0.0f;
    float sparklePhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGVocalEngineAudioProcessorEditor)
};
