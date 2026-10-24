#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class KGVocalMatchAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit KGVocalMatchAudioProcessorEditor(KGVocalMatchAudioProcessor&);
    ~KGVocalMatchAudioProcessorEditor() override;
    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    static juce::String noteName(float hz);

    KGVocalMatchAudioProcessor& p;

    std::array<juce::Slider,4> knobs;
    std::array<juce::Label,4> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,4> at;

    juce::TextButton octaveSafe{"OCTAVE SAFE"};
    juce::TextButton exactUnison{"EXACT UNISON"};
    juce::ToggleButton bypass{"BYPASS"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAt;

    const std::array<juce::String,4> ids{"match","natural","mix","output"};
    const std::array<juce::String,4> names{"MATCH","NATURAL","MIX","OUTPUT"};

    float live=0,ref=0,lc=0,rc=0,cents=0;
    bool exactMode=false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGVocalMatchAudioProcessorEditor)
};
