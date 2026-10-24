#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"
class KGSketchPianoAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit KGSketchPianoAudioProcessorEditor(KGSketchPianoAudioProcessor&);
    ~KGSketchPianoAudioProcessorEditor() override = default;
    void paint(juce::Graphics&) override;
    void resized() override;
private:
    KGSketchPianoAudioProcessor& p;
    juce::MidiKeyboardComponent keyboard;
    juce::ComboBox mode;
    std::array<juce::Slider,12> knobs;
    std::array<juce::Label,12> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,12> at;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAt;
    const std::array<juce::String,12> ids{"character","bright","body","felt","width","space","damp","attack","release","vel","mix","output"};
    const std::array<juce::String,12> names{"CHARACTER","BRIGHT","BODY","FELT","WIDTH","SPACE","DAMP","ATTACK","RELEASE","VEL","MIX","OUTPUT"};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGSketchPianoAudioProcessorEditor)
};
