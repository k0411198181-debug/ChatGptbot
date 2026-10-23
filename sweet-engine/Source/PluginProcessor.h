#pragma once
#include <JuceHeader.h>

class SweetEngineAudioProcessor : public juce::AudioProcessor
{
public:
    SweetEngineAudioProcessor();
    ~SweetEngineAudioProcessor() override = default;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "GOLDEN SYNTH"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;

    float getInputMeter(int channel) const noexcept;
    float getOutputMeter(int channel) const noexcept;
    int pullVisualizationSamples(float* dest, int maxSamples) noexcept;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    struct OnePoleLP
    {
        void prepare(double sr, float cutoff)
        {
            sampleRate = sr;
            zL = zR = 0.0f;
            setCutoff(cutoff);
        }

        void setCutoff(float cutoff)
        {
            a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        }

        float process(float x, int ch)
        {
            float& z = (ch == 0 ? zL : zR);
            z = (1.0f - a) * x + a * z;
            return z;
        }

        double sampleRate = 44100.0;
        float a = 0.0f, zL = 0.0f, zR = 0.0f;
    };

    struct OnePoleHP
    {
        void prepare(double sr)
        {
            sampleRate = sr;
            zL = zR = 0.0f;
        }

        float process(float x, int ch, float cutoff)
        {
            const float a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
            float& z = (ch == 0 ? zL : zR);
            const float low = (1.0f - a) * x + a * z;
            z = low;
            return x - low;
        }

        double sampleRate = 44100.0;
        float zL = 0.0f, zR = 0.0f;
    };

    struct MonoSideLP
    {
        void prepare(double sr, float cutoff)
        {
            sampleRate = sr;
            z = 0.0f;
            setCutoff(cutoff);
        }

        void setCutoff(float cutoff)
        {
            a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float) sampleRate);
        }

        float process(float x)
        {
            z = (1.0f - a) * x + a * z;
            return z;
        }

        double sampleRate = 44100.0;
        float a = 0.0f, z = 0.0f;
    };

    OnePoleHP rumbleHP;
    OnePoleLP lp180, lp450, lp1800, lp6500, lp11000;
    MonoSideLP sideLow;

    juce::dsp::Compressor<float> glueCompressor;
    juce::dsp::Limiter<float> finalLimiter;
    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;
    std::array<float, 2> harshEnvelope {};
    std::array<float, 2> brightEnvelope {};
    float transientFast = 0.0f;
    float transientSlow = 0.0f;

    std::atomic<float> inputMeterL { 0.0f }, inputMeterR { 0.0f };
    std::atomic<float> outputMeterL { 0.0f }, outputMeterR { 0.0f };

    static constexpr int visualFifoSize = 8192;
    juce::AbstractFifo visualFifo { visualFifoSize };
    std::array<float, visualFifoSize> visualSamples {};

    void updateMeters(const juce::AudioBuffer<float>&,
                      std::atomic<float>& left,
                      std::atomic<float>& right) noexcept;
    void pushVisualizationSamples(const juce::AudioBuffer<float>&) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SweetEngineAudioProcessor)
};
