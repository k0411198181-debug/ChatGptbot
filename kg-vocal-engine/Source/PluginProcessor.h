#pragma once
#include <JuceHeader.h>

class KGVocalEngineAudioProcessor : public juce::AudioProcessor
{
public:
    KGVocalEngineAudioProcessor();
    ~KGVocalEngineAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 8.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "KG LIVE VOCAL"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct OnePoleHPF
    {
        void prepare(double sr) { sampleRate = sr; z1L = z1R = 0.0f; }
        float process(float x, int ch, float cutoff)
        {
            const float a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
            float& z = (ch == 0 ? z1L : z1R);
            const float lp = (1.0f - a) * x + a * z;
            z = lp;
            return x - lp;
        }
        double sampleRate = 44100.0;
        float z1L = 0.0f, z1R = 0.0f;
    } hpf, widthSideHpf;

    struct OnePoleLP
    {
        void prepare(double sr, float cutoff)
        {
            a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sr);
            zL = zR = 0.0f;
        }
        float process(float x, int ch)
        {
            float& z = (ch == 0 ? zL : zR);
            z = (1.0f - a) * x + a * z;
            return z;
        }
        float a = 0.0f, zL = 0.0f, zR = 0.0f;
    } split180, split430, split5k2, split9k2, delayToneL, delayToneR;

    juce::dsp::Compressor<float> densityCompressor;
    juce::dsp::Compressor<float> peakCompressor;

    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayL { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> delayR { 192000 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> widthDelayA { 8192 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> widthDelayB { 8192 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> doubleDelayL { 8192 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> doubleDelayR { 8192 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> verbPreL { 19200 };
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> verbPreR { 19200 };

    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuffer, verbBuffer;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> autoGain;

    double currentSampleRate = 44100.0;
    double doublePhase = 0.0;
    float envL = 0.0f, envR = 0.0f;
    float broadEnvL = 0.0f, broadEnvR = 0.0f;
    float fizzEnvL = 0.0f, fizzEnvR = 0.0f;
    float mudEnvL = 0.0f, mudEnvR = 0.0f;
    float mudBroadEnvL = 0.0f, mudBroadEnvR = 0.0f;
    float phraseEnv = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KGVocalEngineAudioProcessor)
};
