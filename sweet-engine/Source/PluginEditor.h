#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class SweetEngineLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SweetEngineLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                          juce::Slider&) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
};

class SweetEngineAudioProcessorEditor : public juce::AudioProcessorEditor,
                                        private juce::Timer
{
public:
    explicit SweetEngineAudioProcessorEditor(SweetEngineAudioProcessor&);
    ~SweetEngineAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void configureKnob(juce::Slider&, juce::Label&, const juce::String& name, bool hero = false);

    void setParameter(const juce::String& id, float plainValue);
    void applyPreset(int index);
    void applyGoldenSettings();
    void stepPreset(int delta);
    void showPresetMenu();
    void showMainMenu();
    void captureState(juce::MemoryBlock& target);
    void restoreState(const juce::MemoryBlock& source);
    void updatePresetLabel(const juce::String& name);

    void updateSpectrum();
    void drawSpectrumAndMeters(juce::Graphics&);

    SweetEngineAudioProcessor& processor;
    SweetEngineLookAndFeel lnf;

    std::array<juce::Slider, 8> knobs;
    std::array<juce::Label, 8> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 8> attachments;

    juce::Slider mixKnob, outputKnob;
    juce::Label mixLabel, outputLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        mixAttachment, outputAttachment;

    juce::ComboBox modeBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAttachment;

    juce::ToggleButton autoButton { "AUTO" };
    juce::TextButton goldenButton { "GOLDEN" };
    juce::ToggleButton maxButton { "MAX" };
    juce::ToggleButton deltaButton { "DELTA" };
    juce::ToggleButton imagerButton { "IMAGER" };
    juce::ToggleButton bypassButton { "BYPASS" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        autoAttachment, maxAttachment, deltaAttachment, imagerAttachment, bypassAttachment;

    juce::TextButton prevPresetButton { "<" };
    juce::TextButton presetButton { "GOLDEN SYNTH" };
    juce::TextButton nextPresetButton { ">" };
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton abButton { "A/B : A" };
    juce::TextButton resetButton { "RESET" };
    juce::TextButton menuButton { "MENU" };

    juce::MemoryBlock stateA, stateB, userSnapshot;
    bool activeA = true;
    int currentPreset = 0;

    const juce::StringArray presetNames {
        "GOLDEN SYNTH",
        "SWEET SYNTH",
        "WIDE SYNTH",
        "GOLDEN STRINGS",
        "SILK STRINGS",
        "CINEMATIC STRINGS"
    };

    const std::array<juce::String, 8> ids {
        "clean", "tame", "detail", "sweet", "air", "body", "width", "impact"
    };

    const std::array<juce::String, 8> names {
        "CLEAN", "TAME", "DETAIL", "SWEET", "AIR", "BODY", "WIDTH", "IMPACT"
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

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SweetEngineAudioProcessorEditor)
};
