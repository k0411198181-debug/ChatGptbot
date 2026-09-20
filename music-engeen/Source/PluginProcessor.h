#pragma once
#include <JuceHeader.h>

class MusicEngeenAudioProcessor : public juce::AudioProcessor
{
public:
    MusicEngeenAudioProcessor();
    ~MusicEngeenAudioProcessor() override = default;

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
    const juce::String getProgramName(int) override { return "GOLDEN MUSIC BUS"; }
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
            setCutoff(cutoff);
            zL = zR = 0.0f;
        }

        void setCutoff(float cutoff)
        {
            a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float)sampleRate);
        }

        float process(float x, int ch)
        {
            float& z = (ch == 0 ? zL : zR);
            z = (1.0f - a) * x + a * z;
            return z;
        }

        double sampleRate = 44100.0;
        float a = 0.0f;
        float zL = 0.0f, zR = 0.0f;
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
            const float a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float)sampleRate);
            float& z = (ch == 0 ? zL : zR);
            const float lp = (1.0f - a) * x + a * z;
            z = lp;
            return x - lp;
        }

        double sampleRate = 44100.0;
        float zL = 0.0f, zR = 0.0f;
    };

    struct SideHP
    {
        void prepare(double sr) { sampleRate = sr; z = 0.0f; }

        float process(float x, float cutoff)
        {
            const float a = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / (float)sampleRate);
            const float lp = (1.0f - a) * x + a * z;
            z = lp;
            return x - lp;
        }

        double sampleRate = 44100.0;
        float z = 0.0f;
    };

    OnePoleHP rumbleHP;
    OnePoleLP low90;
    OnePoleLP low180;
    OnePoleLP low420;
    OnePoleLP low4k5;
    OnePoleLP low9k5;
    SideHP sideHP;

    juce::dsp::Compressor<float> glueCompressor;
    juce::dsp::Limiter<float> safetyLimiter;

    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;
    float transientFast = 0.0f;
    float transientSlow = 0.0f;
    float adaptiveLow = 0.0f;
    float adaptiveHigh = 0.0f;

    std::atomic<float> inputMeterL { 0.0f }, inputMeterR { 0.0f };
    std::atomic<float> outputMeterL { 0.0f }, outputMeterR { 0.0f };

    static constexpr int visualFifoSize = 8192;
    juce::AbstractFifo visualFifo { visualFifoSize };
    std::array<float, visualFifoSize> visualSamples {};

    void pushVisualizationSamples(const juce::AudioBuffer<float>& buffer) noexcept;
    void updateMeters(const juce::AudioBuffer<float>& buffer,
                      std::atomic<float>& left,
                      std::atomic<float>& right) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MusicEngeenAudioProcessor)
};
