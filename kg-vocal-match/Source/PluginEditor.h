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
    std::array<juce::Slider,8> knobs;
    std::array<juce::Label,8> labels;
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>,8> at;
    juce::ToggleButton exact{"EXACT"}, bypass{"BYPASS"};
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> exactAt,bypassAt;
    const std::array<juce::String,8> ids{"match","speed","range","vibrato","natural","tolerance","mix","output"};
    const std::array<juce::String,8> names{"MATCH","SPEED","RANGE","VIBRATO","NATURAL","TOLERANCE","MIX","OUTPUT"};
    float live=0,ref=0,lc=0,rc=0,cents=0;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGVocalMatchAudioProcessorEditor)
};
