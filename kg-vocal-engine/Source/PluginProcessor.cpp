#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float dbToGain(float db) { return juce::Decibels::decibelsToGain(db); }
}

KGVocalEngineAudioProcessor::KGVocalEngineAudioProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                      .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout KGVocalEngineAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout p;

    p.add(std::make_unique<APF>(juce::ParameterID{"clean",1},  "Clean",  0.0f, 100.0f, 55.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"body",1},   "Body",   0.0f, 100.0f, 46.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"air",1},    "Air",    0.0f, 100.0f, 38.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"size",1},   "Size",   0.0f, 100.0f, 46.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"width",1},  "Width",  0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"delay",1},  "Delay",  0.0f, 100.0f, 45.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"space",1},  "Space",  0.0f, 100.0f, 28.0f));

    p.add(std::make_unique<APF>(juce::ParameterID{"throw",1},  "Delay Throw", 0.0f, 100.0f, 0.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"input",1},  "Input", -18.0f, 18.0f, 0.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"output",1}, "Output", -18.0f, 18.0f, 0.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"mix",1},    "Mix", 0.0f, 100.0f, 100.0f));

    p.add(std::make_unique<APB>(juce::ParameterID{"auto",1},   "Auto", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"live",1},   "Live", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"sync",1},   "Sync", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"bypass",1}, "Bypass", false));
    return p;
}

bool KGVocalEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();
    return in == out && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void KGVocalEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    hpf.prepare(sampleRate);
    widthSideHpf.prepare(sampleRate);

    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumOutputChannels() };
    densityCompressor.prepare(spec);
    peakCompressor.prepare(spec);

    densityCompressor.setAttack(14.0f);
    densityCompressor.setRelease(135.0f);
    peakCompressor.setAttack(2.5f);
    peakCompressor.setRelease(58.0f);

    split180.prepare(sampleRate, 180.0f);
    split430.prepare(sampleRate, 430.0f);
    split5k2.prepare(sampleRate, 5200.0f);
    split9k2.prepare(sampleRate, 9200.0f);
    delayToneL.prepare(sampleRate, 5400.0f);
    delayToneR.prepare(sampleRate, 5000.0f);

    delayL.prepare(spec); delayR.prepare(spec);
    widthDelayA.prepare(spec); widthDelayB.prepare(spec);
    doubleDelayL.prepare(spec); doubleDelayR.prepare(spec);
    verbPreL.prepare(spec); verbPreR.prepare(spec);

    delayL.reset(); delayR.reset(); widthDelayA.reset(); widthDelayB.reset();
    doubleDelayL.reset(); doubleDelayR.reset(); verbPreL.reset(); verbPreR.reset();

    reverb.setSampleRate(sampleRate);
    reverb.reset();

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, true);
    verbBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, true);

    autoGain.reset(sampleRate, 0.30);
    autoGain.setCurrentAndTargetValue(1.0f);

    envL = envR = broadEnvL = broadEnvR = fizzEnvL = fizzEnvR = 0.0f;
    mudEnvL = mudEnvR = mudBroadEnvL = mudBroadEnvR = 0.0f;
    phraseEnv = 0.0f;
    doublePhase = 0.0;
}

void KGVocalEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int nCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();
    if (nCh == 0 || n == 0) return;
    if (*apvts.getRawParameterValue("bypass") > 0.5f) return;

    if (dryBuffer.getNumSamples() < n || dryBuffer.getNumChannels() < nCh)
    {
        dryBuffer.setSize(nCh, n, false, false, true);
        verbBuffer.setSize(nCh, n, false, false, true);
    }

    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

    const float inputGain = dbToGain(apvts.getRawParameterValue("input")->load());
    const float outputGain = dbToGain(apvts.getRawParameterValue("output")->load());
    const float mix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    const float clean = apvts.getRawParameterValue("clean")->load() * 0.01f;
    const float body = apvts.getRawParameterValue("body")->load() * 0.01f;
    const float air = apvts.getRawParameterValue("air")->load() * 0.01f;
    const float size = apvts.getRawParameterValue("size")->load() * 0.01f;
    const float width = apvts.getRawParameterValue("width")->load() * 0.01f;
    const float delayAmt = apvts.getRawParameterValue("delay")->load() * 0.01f;
    const float space = apvts.getRawParameterValue("space")->load() * 0.01f;
    const float throwAmt = apvts.getRawParameterValue("throw")->load() * 0.01f;
    const bool autoMode = apvts.getRawParameterValue("auto")->load() > 0.5f;
    const bool liveMode = apvts.getRawParameterValue("live")->load() > 0.5f;
    const bool syncMode = apvts.getRawParameterValue("sync")->load() > 0.5f;

    buffer.applyGain(inputGain);

    if (autoMode)
    {
        float rms = 0.0f;
        for (int ch = 0; ch < nCh; ++ch)
            rms = std::max(rms, buffer.getRMSLevel(ch, 0, n));
        const float target = 0.105f;
        const float desired = juce::jlimit(0.60f, 2.35f, target / (rms + 1.0e-5f));
        autoGain.setTargetValue(desired);
        for (int i = 0; i < n; ++i)
        {
            const float g = autoGain.getNextValue();
            for (int ch = 0; ch < nCh; ++ch)
                buffer.getWritePointer(ch)[i] *= g;
        }
    }

    const float hp = juce::jmap(clean, 38.0f, 82.0f);
    const float mudAttack = std::exp(-1.0f / (0.012f * (float)currentSampleRate));
    const float mudRelease = std::exp(-1.0f / (0.140f * (float)currentSampleRate));
    const float mudBroadAttack = std::exp(-1.0f / (0.010f * (float)currentSampleRate));
    const float mudBroadRelease = std::exp(-1.0f / (0.180f * (float)currentSampleRate));

    for (int ch=0; ch<nCh; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float& mudEnv = (ch == 0 ? mudEnvL : mudEnvR);
        float& mudBroad = (ch == 0 ? mudBroadEnvL : mudBroadEnvR);
        for (int i=0; i<n; ++i)
        {
            const float x = hpf.process(d[i], ch, hp);
            const float lp180 = split180.process(x, ch);
            const float lp430 = split430.process(x, ch);
            const float mudBand = lp430 - lp180;
            const float upper = x - lp430;

            const float absX = std::abs(x);
            const float absMud = std::abs(mudBand);
            mudBroad = absX > mudBroad ? mudBroadAttack * mudBroad + (1.0f-mudBroadAttack) * absX
                                      : mudBroadRelease * mudBroad + (1.0f-mudBroadRelease) * absX;
            mudEnv = absMud > mudEnv ? mudAttack * mudEnv + (1.0f-mudAttack) * absMud
                                     : mudRelease * mudEnv + (1.0f-mudRelease) * absMud;

            const float ratio = mudEnv / (mudBroad + 1.0e-4f);
            const float mudGR = juce::jlimit(0.0f, 0.34f, (ratio - 0.40f) * (0.32f + 1.00f * clean));

            const float noiseThreshold = juce::jmap(clean, 0.010f, 0.030f);
            const float noiseFloor = autoMode ? juce::jmap(clean, 0.72f, 0.42f) : juce::jmap(clean, 0.86f, 0.62f);
            const float t = juce::jlimit(0.0f, 1.0f, mudBroad / (noiseThreshold + 1.0e-5f));
            const float expanderGain = noiseFloor + (1.0f - noiseFloor) * std::sqrt(t);

            d[i] = (lp180 + mudBand * (1.0f - mudGR) + upper) * expanderGain;
        }
    }

    densityCompressor.setThreshold(juce::jmap(body, -16.0f, -30.0f));
    densityCompressor.setRatio(juce::jmap(body, 1.25f, 3.1f));
    peakCompressor.setThreshold(juce::jmap(body, -5.0f, -12.0f));
    peakCompressor.setRatio(juce::jmap(body, 2.0f, 5.0f));

    juce::dsp::AudioBlock<float> block(buffer);
    juce::dsp::ProcessContextReplacing<float> ctx(block);
    densityCompressor.process(ctx);
    peakCompressor.process(ctx);

    const float fastAttack = std::exp(-1.0f / (0.0018f * (float)currentSampleRate));
    const float essRelease = std::exp(-1.0f / (0.060f * (float)currentSampleRate));
    const float fizzRelease = std::exp(-1.0f / (0.095f * (float)currentSampleRate));
    const float broadAttack = std::exp(-1.0f / (0.0040f * (float)currentSampleRate));
    const float broadRelease = std::exp(-1.0f / (0.110f * (float)currentSampleRate));

    for (int ch=0; ch<nCh; ++ch)
    {
        auto* d = buffer.getWritePointer(ch);
        float& essEnv = (ch == 0 ? envL : envR);
        float& broadEnv = (ch == 0 ? broadEnvL : broadEnvR);
        float& fizzEnv = (ch == 0 ? fizzEnvL : fizzEnvR);

        for (int i=0; i<n; ++i)
        {
            const float x = d[i];
            const float lp5 = split5k2.process(x, ch);
            const float lp9 = split9k2.process(x, ch);
            const float essBand = lp9 - lp5;
            const float fizzBand = x - lp9;

            const float absX = std::abs(x);
            const float absEss = std::abs(essBand);
            const float absFizz = std::abs(fizzBand);

            broadEnv = absX > broadEnv ? broadAttack * broadEnv + (1.0f-broadAttack) * absX
                                       : broadRelease * broadEnv + (1.0f-broadRelease) * absX;
            essEnv = absEss > essEnv ? fastAttack * essEnv + (1.0f-fastAttack) * absEss
                                     : essRelease * essEnv + (1.0f-essRelease) * absEss;
            fizzEnv = absFizz > fizzEnv ? fastAttack * fizzEnv + (1.0f-fastAttack) * absFizz
                                        : fizzRelease * fizzEnv + (1.0f-fizzRelease) * absFizz;

            const float ref = broadEnv + 1.0e-4f;
            const float essRatio = essEnv / ref;
            const float fizzRatio = fizzEnv / ref;
            const float autoProtect = autoMode ? 1.18f : 1.0f;
            const float essGR = juce::jlimit(0.0f, 0.56f, (essRatio - 0.19f) * (0.95f + 0.75f * air) * autoProtect);
            const float fizzGR = juce::jlimit(0.0f, 0.68f, (fizzRatio - 0.095f) * (1.18f + 1.00f * air) * autoProtect);

            const float breathGR = (autoMode && broadEnv < 0.075f && essRatio > 0.18f)
                                 ? juce::jlimit(0.0f, 0.24f, clean * (essRatio - 0.15f)) : 0.0f;

            const float airEssGain = 1.0f + 0.095f * air;
            const float airFizzGain = 1.0f + 0.145f * air;
            d[i] = lp5
                 + essBand * airEssGain * (1.0f - essGR - breathGR)
                 + fizzBand * airFizzGain * (1.0f - fizzGR - 0.45f * breathGR);
        }
    }

    const float lfoRate = 0.17f;
    const double phaseInc = juce::MathConstants<double>::twoPi * lfoRate / currentSampleRate;
    doubleDelayL.setDelay((float)(currentSampleRate * 0.0115));
    doubleDelayR.setDelay((float)(currentSampleRate * 0.0175));

    for (int i=0; i<n; ++i)
    {
        const float mod = 0.5f + 0.5f * std::sin((float)doublePhase);
        doublePhase += phaseInc;
        if (doublePhase >= juce::MathConstants<double>::twoPi) doublePhase -= juce::MathConstants<double>::twoPi;

        for (int ch=0; ch<nCh; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            const float x = d[i];
            const float driven = std::tanh(x * (1.0f + 2.05f * size));
            float y = x * (1.0f - 0.31f * size) + driven * (0.31f * size);

            if (nCh >= 2)
            {
                if (ch == 0)
                {
                    const float dbl = doubleDelayL.popSample(0);
                    doubleDelayL.pushSample(0, x);
                    y += dbl * (0.055f + 0.055f * mod) * size;
                }
                else
                {
                    const float dbl = doubleDelayR.popSample(0);
                    doubleDelayR.pushSample(0, x);
                    y += dbl * (0.075f + 0.045f * (1.0f - mod)) * size;
                }
            }
            d[i] = y;
        }
    }

    if (nCh >= 2)
    {
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);
        widthDelayA.setDelay((float)(currentSampleRate * 0.0075));
        widthDelayB.setDelay((float)(currentSampleRate * 0.0145));
        const float sideGain = 1.0f + 0.46f * width;
        for (int i=0; i<n; ++i)
        {
            const float inL = l[i], inR = r[i];
            const float m = 0.5f * (inL + inR);
            float s = 0.5f * (inL - inR) * sideGain;

            const float da = widthDelayA.popSample(0);
            const float db = widthDelayB.popSample(0);
            widthDelayA.pushSample(0, m);
            widthDelayB.pushSample(0, m);

            float syntheticSide = (da - db) * (0.19f * width);
            syntheticSide = widthSideHpf.process(syntheticSide, 0, 165.0f);
            s += syntheticSide;

            l[i] = m + s;
            r[i] = m - s;
        }
    }

    const float phraseAttack = std::exp(-1.0f / (0.006f * (float)currentSampleRate));
    const float phraseRelease = std::exp(-1.0f / (0.180f * (float)currentSampleRate));

    double bpm = 90.0;
    if (syncMode)
        if (auto* ph = getPlayHead())
            if (auto pos = ph->getPosition())
                if (auto bpmValue = pos->getBpm())
                    bpm = juce::jlimit(45.0, 220.0, *bpmValue);

    const double beatMs = 60000.0 / bpm;
    const float leftMs = syncMode ? (float)(beatMs * 0.75) : 315.0f;
    const float rightMs = syncMode ? (float)(beatMs * 1.00) : 420.0f;
    const float liveScale = liveMode ? 0.92f : 1.0f;
    delayL.setDelay((float)(currentSampleRate * leftMs * 0.001 * liveScale));
    delayR.setDelay((float)(currentSampleRate * rightMs * 0.001 * liveScale));

    if (nCh >= 1)
    {
        auto* l = buffer.getWritePointer(0);
        auto* r = nCh > 1 ? buffer.getWritePointer(1) : nullptr;
        for (int i=0; i<n; ++i)
        {
            const float inL = l[i];
            const float inR = r ? r[i] : inL;
            const float inst = std::max(std::abs(inL), std::abs(inR));
            phraseEnv = inst > phraseEnv ? phraseAttack * phraseEnv + (1.0f-phraseAttack) * inst
                                         : phraseRelease * phraseEnv + (1.0f-phraseRelease) * inst;

            float dl = delayL.popSample(0);
            float dr = delayR.popSample(0);
            dl = delayToneL.process(dl, 0);
            dr = delayToneR.process(dr, 1);

            const float voice = juce::jlimit(0.0f, 1.0f, phraseEnv * 3.0f);
            const float duck = 0.40f + 0.60f * (1.0f - voice);
            const float baseWet = 0.030f + 0.255f * delayAmt;
            const float throwWet = 0.52f * throwAmt;
            const float wet = juce::jlimit(0.0f, 0.72f, (baseWet * duck) + throwWet);
            const float feedback = juce::jlimit(0.08f, 0.72f, 0.16f + 0.36f * delayAmt + 0.22f * throwAmt);

            l[i] = inL + dl * wet;
            if (r) r[i] = inR + dr * wet;

            delayL.pushSample(0, inL + dr * feedback);
            delayR.pushSample(0, inR + dl * feedback);
        }
    }

    for (int ch=0; ch<nCh; ++ch)
        verbBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

    const float preMs = juce::jmap(space, 24.0f, 72.0f);
    verbPreL.setDelay((float)(currentSampleRate * preMs * 0.001));
    verbPreR.setDelay((float)(currentSampleRate * (preMs + 7.0f) * 0.001));

    for (int i=0; i<n; ++i)
    {
        const float vL = verbBuffer.getSample(0, i);
        const float pdL = verbPreL.popSample(0);
        verbPreL.pushSample(0, vL);
        verbBuffer.setSample(0, i, pdL);
        if (nCh > 1)
        {
            const float vR = verbBuffer.getSample(1, i);
            const float pdR = verbPreR.popSample(0);
            verbPreR.pushSample(0, vR);
            verbBuffer.setSample(1, i, pdR);
        }
    }

    juce::Reverb::Parameters rp;
    rp.roomSize = juce::jlimit(0.05f, 0.92f, 0.22f + 0.68f * space);
    rp.damping = juce::jlimit(0.15f, 0.88f, 0.72f - 0.34f * space);
    rp.wetLevel = 1.0f;
    rp.dryLevel = 0.0f;
    rp.width = juce::jlimit(0.25f, 1.0f, 0.58f + 0.42f * width);
    rp.freezeMode = 0.0f;
    reverb.setParameters(rp);

    if (nCh > 1)
        reverb.processStereo(verbBuffer.getWritePointer(0), verbBuffer.getWritePointer(1), n);
    else
        reverb.processMono(verbBuffer.getWritePointer(0), n);

    const float spaceWetBase = 0.012f + 0.255f * space;
    for (int i=0; i<n; ++i)
    {
        const float voice = juce::jlimit(0.0f, 1.0f, phraseEnv * 2.8f);
        const float duck = 0.34f + 0.66f * (1.0f - voice);
        const float verbWet = spaceWetBase * duck;
        for (int ch=0; ch<nCh; ++ch)
            buffer.getWritePointer(ch)[i] += verbBuffer.getReadPointer(ch)[i] * verbWet;
    }

    for (int ch=0; ch<nCh; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        const auto* dry = dryBuffer.getReadPointer(ch);
        for (int i=0; i<n; ++i)
        {
            float y = dry[i] * (1.0f-mix) + wet[i] * mix;
            y *= outputGain;
            wet[i] = std::tanh(y * 1.045f) / std::tanh(1.045f);
        }
    }
}

juce::AudioProcessorEditor* KGVocalEngineAudioProcessor::createEditor()
{
    return new KGVocalEngineAudioProcessorEditor(*this);
}

void KGVocalEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void KGVocalEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));
    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KGVocalEngineAudioProcessor();
}
