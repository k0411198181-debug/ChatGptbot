#pragma once
#include <JuceHeader.h>

class KGHumanEngineAudioProcessor : public juce::AudioProcessor
{
public:
    KGHumanEngineAudioProcessor();
    ~KGHumanEngineAudioProcessor() override = default;

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
    const juce::String getProgramName(int) override { return "GOLDEN HUMAN"; }
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

    OnePoleLP lp250, lp2500;

    juce::dsp::Limiter<float> limiter;
    juce::AudioBuffer<float> dryBuffer;

    double currentSampleRate = 44100.0;
    float envFast = 0.0f;
    float envSlow = 0.0f;
    float lfoPhaseA = 0.0f;
    float lfoPhaseB = 0.37f;
    float driftPhase = 0.0f;
    uint32_t rngState = 0x31415926u;

    std::atomic<float> inputMeterL { 0.0f }, inputMeterR { 0.0f };
    std::atomic<float> outputMeterL { 0.0f }, outputMeterR { 0.0f };

    static constexpr int visualFifoSize = 8192;
    juce::AbstractFifo visualFifo { visualFifoSize };
    std::array<float, visualFifoSize> visualSamples {};

    float nextNoise() noexcept;
    void updateMeters(const juce::AudioBuffer<float>&,
                      std::atomic<float>& left,
                      std::atomic<float>& right) noexcept;
    void pushVisualizationSamples(const juce::AudioBuffer<float>&) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGHumanEngineAudioProcessor)
};
