#pragma once
#include <JuceHeader.h>

class KGVocalMatchAudioProcessor : public juce::AudioProcessor
{
public:
    KGVocalMatchAudioProcessor();
    ~KGVocalMatchAudioProcessor() override = default;
    void prepareToPlay(double,int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
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
    const juce::String getProgramName(int) override { return "REFERENCE MATCH"; }
    void changeProgramName(int,const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;

    juce::AudioProcessorValueTreeState apvts;
    float getLiveHz() const noexcept { return liveHz.load(); }
    float getRefHz() const noexcept { return refHz.load(); }
    float getLiveConfidence() const noexcept { return liveConf.load(); }
    float getRefConfidence() const noexcept { return refConf.load(); }
    float getShiftCents() const noexcept { return shiftCents.load(); }

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    class PitchDetector
    {
    public:
        void prepare(double sr);
        void reset();
        void push(float x);
        bool shouldAnalyse() noexcept;
        std::pair<float,float> analyse();
    private:
        static constexpr int down=4;
        static constexpr int N=512;
        std::array<float,N> ring{};
        int wp=0, downCounter=0, hopCounter=0;
        double sampleRate=48000.0;
    } liveDetector, refDetector;

    class GrainShifter
    {
    public:
        void prepare(double sr);
        void reset();
        float process(float x,float ratio,float dryWet,float voicedGate);
        int getLatency() const noexcept { return latency; }
    private:
        float read(float delay) const noexcept;
        std::vector<float> buf;
        int wp=0, size=0, grain=2048, baseDelay=2304, latency=3328;
        float phase=0.0f;
    } shifterL, shifterR;

    double sr=48000.0;
    float ratioSmoothed=1.0f;
    std::atomic<float> liveHz{0},refHz{0},liveConf{0},refConf{0},shiftCents{0};
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KGVocalMatchAudioProcessor)
};
