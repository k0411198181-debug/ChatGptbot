#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float dbToGain(float db)
    {
        return juce::Decibels::decibelsToGain(db);
    }

    inline float smoothEnv(float x, float current, float attackCoeff, float releaseCoeff)
    {
        return x > current
            ? attackCoeff * current + (1.0f - attackCoeff) * x
            : releaseCoeff * current + (1.0f - releaseCoeff) * x;
    }
}

KGHumanEngineAudioProcessor::KGHumanEngineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout KGHumanEngineAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout p;

    p.add(std::make_unique<APF>(juce::ParameterID{"human",1},   "Human",   0.0f, 100.0f, 55.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"feel",1},    "Feel",    0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"attack",1},  "Attack",  0.0f, 100.0f, 46.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"flow",1},    "Flow",    0.0f, 100.0f, 52.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"drift",1},   "Drift",   0.0f, 100.0f, 34.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"color",1},   "Color",   0.0f, 100.0f, 45.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"motion",1},  "Motion",  0.0f, 100.0f, 38.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"width",1},   "Width",   0.0f, 100.0f, 48.0f));

    p.add(std::make_unique<APF>(juce::ParameterID{"mix",1},     "Mix",     0.0f, 100.0f, 100.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"output",1},  "Output", -12.0f, 6.0f, 0.0f));

    p.add(std::make_unique<APC>(juce::ParameterID{"mode",1}, "Mode",
                                juce::StringArray{"MELODIC", "RHYTHMIC", "ATMOSPHERE"}, 0));

    p.add(std::make_unique<APB>(juce::ParameterID{"auto",1},   "Auto", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"max",1},    "Max", false));
    p.add(std::make_unique<APB>(juce::ParameterID{"delta",1},  "Delta", false));
    p.add(std::make_unique<APB>(juce::ParameterID{"bypass",1}, "Bypass", false));

    return p;
}

bool KGHumanEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    return in == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void KGHumanEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    lp250.prepare(sampleRate, 250.0f);
    lp2500.prepare(sampleRate, 2500.0f);

    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32)samplesPerBlock,
        (juce::uint32)getTotalNumOutputChannels()
    };

    limiter.prepare(spec);
    limiter.setThreshold(-0.8f);
    limiter.setRelease(85.0f);

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, true);

    envFast = envSlow = 0.0f;
    lfoPhaseA = 0.0f;
    lfoPhaseB = 0.37f;
    driftPhase = 0.0f;
    rngState = 0x31415926u;
    visualFifo.reset();
}

float KGHumanEngineAudioProcessor::nextNoise() noexcept
{
    rngState = 1664525u * rngState + 1013904223u;
    const float n = (float)((rngState >> 8) & 0x00ffffffu) / (float)0x00ffffffu;
    return n * 2.0f - 1.0f;
}

float KGHumanEngineAudioProcessor::getInputMeter(int channel) const noexcept
{
    return (channel == 0 ? inputMeterL : inputMeterR).load(std::memory_order_relaxed);
}

float KGHumanEngineAudioProcessor::getOutputMeter(int channel) const noexcept
{
    return (channel == 0 ? outputMeterL : outputMeterR).load(std::memory_order_relaxed);
}

void KGHumanEngineAudioProcessor::updateMeters(const juce::AudioBuffer<float>& buffer,
                                               std::atomic<float>& left,
                                               std::atomic<float>& right) noexcept
{
    const int n = buffer.getNumSamples();
    const int chs = buffer.getNumChannels();
    const float l = (chs > 0 && n > 0) ? buffer.getMagnitude(0, 0, n) : 0.0f;
    const float r = (chs > 1 && n > 0) ? buffer.getMagnitude(1, 0, n) : l;
    left.store(l, std::memory_order_relaxed);
    right.store(r, std::memory_order_relaxed);
}

void KGHumanEngineAudioProcessor::pushVisualizationSamples(const juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int chs = buffer.getNumChannels();
    if (n <= 0 || chs <= 0) return;

    int start1=0,size1=0,start2=0,size2=0;
    visualFifo.prepareToWrite(n,start1,size1,start2,size2);

    const auto writeRange = [&](int start, int count, int sourceOffset)
    {
        for (int i=0;i<count;++i)
        {
            const int src=sourceOffset+i;
            const float l=buffer.getSample(0,src);
            const float r=chs>1?buffer.getSample(1,src):l;
            visualSamples[(size_t)(start+i)] = 0.5f*(l+r);
        }
    };

    writeRange(start1,size1,0);
    writeRange(start2,size2,size1);
    visualFifo.finishedWrite(size1+size2);
}

int KGHumanEngineAudioProcessor::pullVisualizationSamples(float* dest, int maxSamples) noexcept
{
    if (dest==nullptr || maxSamples<=0) return 0;

    int start1=0,size1=0,start2=0,size2=0;
    visualFifo.prepareToRead(maxSamples,start1,size1,start2,size2);

    if (size1>0) std::copy_n(visualSamples.data()+start1,size1,dest);
    if (size2>0) std::copy_n(visualSamples.data()+start2,size2,dest+size1);

    visualFifo.finishedRead(size1+size2);
    return size1+size2;
}

void KGHumanEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (nCh==0 || n==0) return;

    updateMeters(buffer,inputMeterL,inputMeterR);

    if (*apvts.getRawParameterValue("bypass") > 0.5f)
    {
        updateMeters(buffer,outputMeterL,outputMeterR);
        pushVisualizationSamples(buffer);
        return;
    }

    if (dryBuffer.getNumSamples() < n || dryBuffer.getNumChannels() < nCh)
        dryBuffer.setSize(nCh,n,false,false,true);

    for (int ch=0; ch<nCh; ++ch)
        dryBuffer.copyFrom(ch,0,buffer,ch,0,n);

    float human  = apvts.getRawParameterValue("human")->load()*0.01f;
    float feel   = apvts.getRawParameterValue("feel")->load()*0.01f;
    float attack = apvts.getRawParameterValue("attack")->load()*0.01f;
    float flow   = apvts.getRawParameterValue("flow")->load()*0.01f;
    float drift  = apvts.getRawParameterValue("drift")->load()*0.01f;
    float color  = apvts.getRawParameterValue("color")->load()*0.01f;
    float motion = apvts.getRawParameterValue("motion")->load()*0.01f;
    float width  = apvts.getRawParameterValue("width")->load()*0.01f;

    const float mix = apvts.getRawParameterValue("mix")->load()*0.01f;
    const float outputGain = dbToGain(apvts.getRawParameterValue("output")->load());

    const int mode = (int)apvts.getRawParameterValue("mode")->load();
    const bool autoMode = apvts.getRawParameterValue("auto")->load() > 0.5f;
    const bool maxMode = apvts.getRawParameterValue("max")->load() > 0.5f;
    const bool deltaMode = apvts.getRawParameterValue("delta")->load() > 0.5f;

    if (!maxMode)
    {
        human  = juce::jmin(human, 0.82f);
        feel   = juce::jmin(feel, 0.78f);
        attack = juce::jmin(attack, 0.78f);
        flow   = juce::jmin(flow, 0.80f);
        drift  = juce::jmin(drift, 0.68f);
        color  = juce::jmin(color, 0.78f);
        motion = juce::jmin(motion, 0.72f);
        width  = juce::jmin(width, 0.78f);
    }

    const float modeAttackScale = mode==1 ? 1.25f : (mode==2 ? 0.65f : 0.95f);
    const float modeFlowScale   = mode==2 ? 1.30f : (mode==1 ? 0.80f : 1.0f);
    const float modeMotionScale = mode==2 ? 1.35f : (mode==1 ? 0.75f : 1.0f);

    const float fastA = std::exp(-1.0f / ((mode==1 ? 0.0018f : 0.0032f) * (float)currentSampleRate));
    const float fastR = std::exp(-1.0f / ((mode==1 ? 0.035f : 0.055f) * (float)currentSampleRate));
    const float slowA = std::exp(-1.0f / ((mode==2 ? 0.055f : 0.030f) * (float)currentSampleRate));
    const float slowR = std::exp(-1.0f / ((mode==2 ? 0.360f : 0.220f) * (float)currentSampleRate));

    const float phaseIncA = (0.065f + flow*0.085f) / (float)currentSampleRate;
    const float phaseIncB = (0.043f + motion*0.070f) / (float)currentSampleRate;
    const float driftInc  = (0.010f + drift*0.028f) / (float)currentSampleRate;

    for (int i=0; i<n; ++i)
    {
        float monoAbs=0.0f;

        for (int ch=0; ch<nCh; ++ch)
        {
            auto* d=buffer.getWritePointer(ch);
            const float x=d[i];

            const float low=lp250.process(x,ch);
            const float lowMid=lp2500.process(x,ch)-low;
            const float high=x-(low+lowMid);

            // COLOR: asymmetric soft saturation, strongest in mids/highs.
            const float satDrive=1.0f+color*(maxMode?5.2f:3.6f);
            const float satSource=0.22f*low + 0.62f*lowMid + 0.78f*high;
            const float sat=std::tanh(satSource*satDrive)/std::tanh(satDrive);
            float y=x+(sat-satSource)*color*(maxMode?0.50f:0.34f);

            monoAbs=juce::jmax(monoAbs,std::abs(y));
            d[i]=y;
        }

        envFast=smoothEnv(monoAbs,envFast,fastA,fastR);
        envSlow=smoothEnv(monoAbs,envSlow,slowA,slowR);
        const float transient=juce::jmax(0.0f,envFast-envSlow);

        // FEEL: very small deterministic micro-dynamic variation.
        const float randomish = 0.64f*std::sin(juce::MathConstants<float>::twoPi*lfoPhaseA)
                              + 0.36f*std::sin(juce::MathConstants<float>::twoPi*lfoPhaseB);
        const float noise = nextNoise();
        const float micro = (randomish*0.72f + noise*0.28f)
                          * feel * human * (maxMode?0.020f:0.012f);

        // ATTACK: musical transient shaping, stronger in RHYTHMIC mode.
        const float transientBoost = juce::jlimit(0.0f,
            maxMode?0.22f:0.13f,
            transient*attack*modeAttackScale*(maxMode?3.0f:2.1f));

        // FLOW: slow phrase-scale gain movement.
        const float phrase = std::sin(juce::MathConstants<float>::twoPi*driftPhase);
        const float flowGain = 1.0f + phrase*flow*human*modeFlowScale*(maxMode?0.020f:0.012f);

        for (int ch=0; ch<nCh; ++ch)
        {
            auto* d=buffer.getWritePointer(ch);
            d[i] *= (1.0f + micro + transientBoost) * flowGain;
        }

        lfoPhaseA += phaseIncA;
        lfoPhaseB += phaseIncB;
        driftPhase += driftInc;

        if (lfoPhaseA>=1.0f) lfoPhaseA-=1.0f;
        if (lfoPhaseB>=1.0f) lfoPhaseB-=1.0f;
        if (driftPhase>=1.0f) driftPhase-=1.0f;
    }

    // MOTION/WIDTH: subtle stereo breathing above low frequencies.
    if (nCh>=2)
    {
        auto* l=buffer.getWritePointer(0);
        auto* r=buffer.getWritePointer(1);

        float phase=lfoPhaseB;

        for (int i=0; i<n; ++i)
        {
            const float mid=0.5f*(l[i]+r[i]);
            const float side=0.5f*(l[i]-r[i]);

            const float breathe=std::sin(juce::MathConstants<float>::twoPi*phase)
                              * motion*human*modeMotionScale*(maxMode?0.13f:0.075f);

            const float widthGain=1.0f + (width-0.5f)*(maxMode?0.90f:0.60f) + breathe;
            const float lowSafe=0.90f + 0.10f*(1.0f-width);
            const float shapedSide=side*widthGain*lowSafe;

            l[i]=mid+shapedSide;
            r[i]=mid-shapedSide;

            phase += phaseIncB;
            if (phase>=1.0f) phase-=1.0f;
        }
    }

    // AUTO gently protects extremes.
    if (autoMode)
    {
        const float mag=juce::jmax(buffer.getMagnitude(0,0,n),
                                   nCh>1?buffer.getMagnitude(1,0,n):0.0f);
        if (mag>1.0f)
            buffer.applyGain(1.0f/(mag+0.01f));
    }

    for (int ch=0; ch<nCh; ++ch)
    {
        auto* wet=buffer.getWritePointer(ch);
        const auto* dry=dryBuffer.getReadPointer(ch);

        for (int i=0;i<n;++i)
        {
            const float processed=wet[i];
            const float mixed=dry[i]*(1.0f-mix)+processed*mix;
            wet[i]=(deltaMode ? (processed-dry[i]) : mixed)*outputGain;
        }
    }

    if (!deltaMode)
    {
        limiter.setThreshold(maxMode?-0.35f:-0.8f);
        limiter.setRelease(mode==1?62.0f:88.0f);
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        limiter.process(ctx);
    }

    updateMeters(buffer,outputMeterL,outputMeterR);
    pushVisualizationSamples(buffer);
}

juce::AudioProcessorEditor* KGHumanEngineAudioProcessor::createEditor()
{
    return new KGHumanEngineAudioProcessorEditor(*this);
}

void KGHumanEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state=apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml,destData);
}

void KGHumanEngineAudioProcessor::setStateInformation(const void* data,int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data,sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KGHumanEngineAudioProcessor();
}
