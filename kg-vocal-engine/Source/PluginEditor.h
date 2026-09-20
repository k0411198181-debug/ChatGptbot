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

    KGVocalEngineAudioProcessor& processor;
    GalaxyLookAndFeel galaxyLnf;

    std::array<juce::Slider, 7> knobs;
    std::array<juce::Label, 7> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>, 7> attachments;

    juce::Slider inputKnob, mixKnob, outputKnob, throwKnob;
    juce::Label inputLabel, mixLabel, outputLabel, throwLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputAttachment, mixAttachment, outputAttachment, throwAttachment;

    juce::ToggleButton autoButton { "AUTO" }, liveButton { "LIVE" }, syncButton { "SYNC" }, bypassButton { "BYPASS" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> autoAttachment, liveAttachment, syncAttachment, bypassAttachment;

    const std::array<juce::String, 7> ids { "clean","body","air","size","width","delay","space" };
    const std::array<juce::String, 7> names { "CLEAN","BODY","AIR","SIZE","WIDTH","DELAY","SPACE" };

    float sparklePhase = 0.0f;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGVocalEngineAudioProcessorEditor)
};
