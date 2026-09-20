#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class MusicGoldLookAndFeel : public juce::LookAndFeel_V4
{
public:
    MusicGoldLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class MusicEngeenAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit MusicEngeenAudioProcessorEditor(MusicEngeenAudioProcessor&);
    ~MusicEngeenAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureKnob(juce::Slider&, juce::Label&, const juce::String& name, bool hero = false);
    void applyGoldenSettings();
    void updateSpectrum();
    void drawSpectrumAndMeters(juce::Graphics&);

    MusicEngeenAudioProcessor& processor;
    MusicGoldLookAndFeel lookAndFeel;

    std::array<juce::Slider, 7> knobs;
    std::array<juce::Label, 7> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> attachments;

    juce::Slider subKnob, tiltKnob, mixKnob, outputKnob;
    juce::Label subLabel, tiltLabel, mixLabel, outputLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        subAttachment, tiltAttachment, mixAttachment, outputAttachment;

    juce::ToggleButton autoButton { "AUTO" };
    juce::TextButton goldenButton { "GOLDEN" };
    juce::ToggleButton safeButton { "SAFE" };
    juce::ToggleButton bypassButton { "BYPASS" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        autoAttachment, safeAttachment, bypassAttachment;

    // WEIGHT sits in the hero centre position.
    const std::array<juce::String, 7> ids {
        "clean", "punch", "glue", "weight", "shine", "width", "loud"
    };

    const std::array<juce::String, 7> names {
        "CLEAN", "PUNCH", "GLUE", "WEIGHT", "SHINE", "WIDTH", "LOUD"
    };

    static constexpr int fftOrder = 10;
    static constexpr int fftSize = 1 << fftOrder;
    static constexpr int spectrumBands = 52;

    juce::dsp::FFT forwardFFT { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow {
        fftSize, juce::dsp::WindowingFunction<float>::hann, true
    };

    std::array<float, fftSize * 2> fftData {};
    std::array<float, fftSize> fftInput {};
    std::array<float, 4096> visualPullBuffer {};
    std::array<float, spectrumBands> spectrumValues {};

    int fftInputPos = 0;
    float smoothInL = 0.0f, smoothInR = 0.0f;
    float smoothOutL = 0.0f, smoothOutR = 0.0f;
    float animationPhase = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicEngeenAudioProcessorEditor)
};
