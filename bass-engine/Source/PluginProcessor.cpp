#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float dbToGain(float db)
    {
        return juce::Decibels::decibelsToGain(db);
    }

    inline float smoothEnv(float input, float current, float attackCoeff, float releaseCoeff)
    {
        return input > current
            ? attackCoeff * current + (1.0f - attackCoeff) * input
            : releaseCoeff * current + (1.0f - releaseCoeff) * input;
    }
}

BassEngineAudioProcessor::BassEngineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout BassEngineAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout p;

    p.add(std::make_unique<APF>(juce::ParameterID{"sub",1},       "Sub",       0.0f, 100.0f, 44.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"weight",1},    "Weight",    0.0f, 100.0f, 66.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"drive",1},     "Drive",     0.0f, 100.0f, 44.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"punch",1},     "Punch",     0.0f, 100.0f, 42.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"tight",1},     "Tight",     0.0f, 100.0f, 62.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"harmonics",1}, "Harmonics", 0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"loud",1},      "Loud",      0.0f, 100.0f, 28.0f));

    p.add(std::make_unique<APF>(juce::ParameterID{"clarity",1}, "Clarity", 0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"mix",1},     "Mix",     0.0f, 100.0f, 100.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"output",1},  "Output",  -12.0f, 6.0f, 0.0f));

    p.add(std::make_unique<APC>(juce::ParameterID{"mode",1}, "Mode",
                                juce::StringArray{"BASS", "DRUM / KICK"}, 0));

    p.add(std::make_unique<APB>(juce::ParameterID{"auto",1},   "Auto", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"max",1},    "Max", false));
    p.add(std::make_unique<APB>(juce::ParameterID{"bypass",1}, "Bypass", false));

    return p;
}

bool BassEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    return in == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void BassEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    rumbleHP.prepare(sampleRate);
    lp70.prepare(sampleRate, 70.0f);
    lp160.prepare(sampleRate, 160.0f);
    lp320.prepare(sampleRate, 320.0f);
    lp1800.prepare(sampleRate, 1800.0f);
    sideLow.prepare(sampleRate, 130.0f);

    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32)samplesPerBlock,
        (juce::uint32)getTotalNumOutputChannels()
    };

    controlCompressor.prepare(spec);
    controlCompressor.setAttack(12.0f);
    controlCompressor.setRelease(120.0f);

    finalLimiter.prepare(spec);
    finalLimiter.setThreshold(-0.8f);
    finalLimiter.setRelease(75.0f);

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, true);

    transientFast = transientSlow = lowEnvelope = 0.0f;
    inputMeterL.store(0.0f, std::memory_order_relaxed);
    inputMeterR.store(0.0f, std::memory_order_relaxed);
    outputMeterL.store(0.0f, std::memory_order_relaxed);
    outputMeterR.store(0.0f, std::memory_order_relaxed);
    visualFifo.reset();
}

float BassEngineAudioProcessor::getInputMeter(int channel) const noexcept
{
    return (channel == 0 ? inputMeterL : inputMeterR).load(std::memory_order_relaxed);
}

float BassEngineAudioProcessor::getOutputMeter(int channel) const noexcept
{
    return (channel == 0 ? outputMeterL : outputMeterR).load(std::memory_order_relaxed);
}

void BassEngineAudioProcessor::updateMeters(const juce::AudioBuffer<float>& buffer,
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

void BassEngineAudioProcessor::pushVisualizationSamples(const juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int chs = buffer.getNumChannels();

    if (n <= 0 || chs <= 0)
        return;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    visualFifo.prepareToWrite(n, start1, size1, start2, size2);

    const auto writeRange = [&](int start, int count, int sourceOffset)
    {
        for (int i = 0; i < count; ++i)
        {
            const int src = sourceOffset + i;
            const float l = buffer.getSample(0, src);
            const float r = chs > 1 ? buffer.getSample(1, src) : l;
            visualSamples[(size_t)(start + i)] = 0.5f * (l + r);
        }
    };

    writeRange(start1, size1, 0);
    writeRange(start2, size2, size1);
    visualFifo.finishedWrite(size1 + size2);
}

int BassEngineAudioProcessor::pullVisualizationSamples(float* dest, int maxSamples) noexcept
{
    if (dest == nullptr || maxSamples <= 0)
        return 0;

    int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
    visualFifo.prepareToRead(maxSamples, start1, size1, start2, size2);

    if (size1 > 0)
        std::copy_n(visualSamples.data() + start1, size1, dest);

    if (size2 > 0)
        std::copy_n(visualSamples.data() + start2, size2, dest + size1);

    visualFifo.finishedRead(size1 + size2);
    return size1 + size2;
}

void BassEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int nCh = buffer.getNumChannels();
    const int n = buffer.getNumSamples();

    if (nCh == 0 || n == 0)
        return;

    updateMeters(buffer, inputMeterL, inputMeterR);

    if (*apvts.getRawParameterValue("bypass") > 0.5f)
    {
        updateMeters(buffer, outputMeterL, outputMeterR);
        pushVisualizationSamples(buffer);
        return;
    }

    if (dryBuffer.getNumSamples() < n || dryBuffer.getNumChannels() < nCh)
        dryBuffer.setSize(nCh, n, false, false, true);

    for (int ch = 0; ch < nCh; ++ch)
        dryBuffer.copyFrom(ch, 0, buffer, ch, 0, n);

    float sub       = apvts.getRawParameterValue("sub")->load() * 0.01f;
    float weight    = apvts.getRawParameterValue("weight")->load() * 0.01f;
    float drive     = apvts.getRawParameterValue("drive")->load() * 0.01f;
    float punch     = apvts.getRawParameterValue("punch")->load() * 0.01f;
    float tight     = apvts.getRawParameterValue("tight")->load() * 0.01f;
    float harmonics = apvts.getRawParameterValue("harmonics")->load() * 0.01f;
    float loud      = apvts.getRawParameterValue("loud")->load() * 0.01f;

    const float clarity = apvts.getRawParameterValue("clarity")->load() * 0.01f;
    const float mix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    const float outputGain = dbToGain(apvts.getRawParameterValue("output")->load());

    const bool autoMode = apvts.getRawParameterValue("auto")->load() > 0.5f;
    const bool maxMode = apvts.getRawParameterValue("max")->load() > 0.5f;
    const bool drumMode = apvts.getRawParameterValue("mode")->load() > 0.5f;

    if (!maxMode)
    {
        sub = juce::jmin(sub, drumMode ? 0.64f : 0.78f);
        weight = juce::jmin(weight, drumMode ? 0.88f : 0.86f);
        drive = juce::jmin(drive, 0.80f);
        punch = juce::jmin(punch, drumMode ? 0.90f : 0.82f);
        harmonics = juce::jmin(harmonics, 0.82f);
        loud = juce::jmin(loud, 0.72f);
    }

    // BASS mode removes more rumble/boom. DRUM/KICK preserves impact but restrains true sub.
    const float hpMin = drumMode ? (maxMode ? 22.0f : 27.0f) : (maxMode ? 24.0f : 28.0f);
    const float hpMax = drumMode ? 46.0f : 50.0f;
    const float hpCut = juce::jmap(tight, hpMin, hpMax);

    const float fastA = std::exp(-1.0f / ((drumMode ? 0.0015f : 0.0025f) * (float)currentSampleRate));
    const float fastR = std::exp(-1.0f / ((drumMode ? 0.036f : 0.045f) * (float)currentSampleRate));
    const float slowA = std::exp(-1.0f / ((drumMode ? 0.020f : 0.026f) * (float)currentSampleRate));
    const float slowR = std::exp(-1.0f / ((drumMode ? 0.170f : 0.220f) * (float)currentSampleRate));
    const float lowA  = std::exp(-1.0f / (0.016f * (float)currentSampleRate));
    const float lowR  = std::exp(-1.0f / (0.210f * (float)currentSampleRate));

    for (int i = 0; i < n; ++i)
    {
        float monoAbs = 0.0f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);

            const float x0 = rumbleHP.process(d[i], ch, hpCut);
            const float subBand = lp70.process(x0, ch);
            const float low160 = lp160.process(x0, ch);
            const float low320 = lp320.process(x0, ch);
            const float low1800 = lp1800.process(x0, ch);

            const float bodyBand = low160 - subBand;
            const float mudBand = low320 - low160;
            const float upperBass = low1800 - low320;
            const float presence = x0 - low1800;

            lowEnvelope = smoothEnv(std::abs(low160), lowEnvelope, lowA, lowR);

            float autoProtect = 0.0f;
            if (autoMode)
            {
                const float threshold = drumMode ? 0.20f : 0.15f;
                const float strength = drumMode ? 1.8f : 2.7f;
                autoProtect = juce::jlimit(0.0f, drumMode ? 0.42f : 0.58f,
                                           (lowEnvelope - threshold) * strength);
            }

            const float effectiveSub = sub * (1.0f - (drumMode ? 0.92f : 0.88f) * autoProtect);
            const float effectiveWeight = weight * (1.0f - (drumMode ? 0.48f : 0.68f) * autoProtect);

            const float mudMax = maxMode
                               ? (drumMode ? 0.42f : 0.48f)
                               : (drumMode ? 0.36f : 0.42f);
            const float mudReduction = juce::jlimit(
                0.0f,
                mudMax,
                tight * ((drumMode ? 0.12f : 0.18f)
                       + (drumMode ? 0.42f : 0.52f)
                       * std::abs(mudBand) / (std::abs(x0) + 1.0e-4f)));

            float y = x0 - mudBand * mudReduction;

            // SUB: Bass keeps foundation; Drum/Kick intentionally keeps it tighter.
            const float subBase = drumMode ? 0.06f : 0.10f;
            const float subDepth = drumMode
                                 ? (maxMode ? 0.22f : 0.15f)
                                 : (maxMode ? 0.34f : 0.24f);
            y += subBand * (subBase + subDepth * effectiveSub);

            // WEIGHT: Drum/Kick puts more energy into body instead of infra-sub.
            const float bodyDepth = drumMode
                                  ? (maxMode ? 0.62f : 0.45f)
                                  : (maxMode ? 0.52f : 0.38f);
            y += bodyBand * (bodyDepth * effectiveWeight);

            // Low-band saturation adds perceived size without just EQ boosting.
            const float lowDrive = 1.0f + drive * (drumMode
                                      ? (maxMode ? 6.8f : 4.6f)
                                      : (maxMode ? 6.0f : 4.0f));
            const float lowSource = subBand + bodyBand;
            const float lowSat = std::tanh(lowSource * lowDrive) / std::tanh(lowDrive);
            y += (lowSat - lowSource) * (drive * (drumMode
                                      ? (maxMode ? 0.78f : 0.52f)
                                      : (maxMode ? 0.72f : 0.48f)));

            // Upper harmonics make the bass/kick audible on smaller speakers.
            const float harmonicDrive = 1.0f + harmonics * (drumMode
                                           ? (maxMode ? 7.6f : 5.3f)
                                           : (maxMode ? 7.0f : 4.8f));
            const float harmonicSource = low160 + (drumMode ? 0.62f : 0.45f) * upperBass;
            const float harmonicSat = std::tanh(harmonicSource * harmonicDrive) / std::tanh(harmonicDrive);
            y += (harmonicSat - harmonicSource) * (harmonics * (drumMode
                                            ? (maxMode ? 0.80f : 0.52f)
                                            : (maxMode ? 0.76f : 0.48f)));

            y += upperBass * ((drumMode ? 0.20f : 0.16f) * clarity);
            y += presence * ((drumMode ? 0.085f : 0.07f) * clarity);

            monoAbs = juce::jmax(monoAbs, std::abs(y));
            d[i] = y;
        }

        transientFast = smoothEnv(monoAbs, transientFast, fastA, fastR);
        transientSlow = smoothEnv(monoAbs, transientSlow, slowA, slowR);

        const float transient = juce::jmax(0.0f, transientFast - transientSlow);
        const float punchCeil = drumMode
                              ? (maxMode ? 0.48f : 0.30f)
                              : (maxMode ? 0.34f : 0.20f);
        const float punchDrive = drumMode
                               ? (2.8f + (maxMode ? 5.2f : 3.8f) * punch)
                               : (2.0f + (maxMode ? 4.4f : 3.0f) * punch);
        const float punchGain = 1.0f + juce::jlimit(0.0f, punchCeil, transient * punchDrive);

        for (int ch = 0; ch < nCh; ++ch)
            buffer.getWritePointer(ch)[i] *= punchGain;
    }

    // Mode-dependent glue: drums get faster, punchier control; bass gets smoother leveling.
    controlCompressor.setThreshold(juce::jmap(weight,
        drumMode ? -3.0f : -4.0f,
        drumMode ? (maxMode ? -13.0f : -10.0f) : (maxMode ? -15.0f : -11.0f)));
    controlCompressor.setRatio(juce::jmap(weight,
        drumMode ? 1.08f : 1.05f,
        drumMode ? (maxMode ? 2.7f : 2.0f) : (maxMode ? 3.0f : 2.2f)));
    controlCompressor.setAttack(juce::jmap(punch,
        drumMode ? 18.0f : 24.0f,
        drumMode ? 5.5f : 8.0f));
    controlCompressor.setRelease(juce::jmap(tight,
        drumMode ? 130.0f : 180.0f,
        drumMode ? 62.0f : 85.0f));

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        controlCompressor.process(ctx);
    }

    if (nCh >= 2)
    {
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);

        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);

            const float sideLowPart = sideLow.process(side);
            const float sideHighPart = side - sideLowPart;

            const float lowSideGain = juce::jmap(tight,
                drumMode ? 0.58f : 0.46f,
                drumMode ? 0.16f : 0.08f);
            const float newSide = sideLowPart * lowSideGain + sideHighPart;

            l[i] = mid + newSide;
            r[i] = mid - newSide;
        }
    }

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        const auto* dry = dryBuffer.getReadPointer(ch);

        const float loudDrive = 1.0f + loud * (drumMode
                                   ? (maxMode ? 4.0f : 2.5f)
                                   : (maxMode ? 3.7f : 2.2f));
        const float loudNorm = std::tanh(loudDrive);

        for (int i = 0; i < n; ++i)
        {
            float y = dry[i] * (1.0f - mix) + wet[i] * mix;

            const float clipped = std::tanh(y * loudDrive) / loudNorm;
            const float clipMix = (drumMode ? (maxMode ? 0.58f : 0.42f)
                                            : (maxMode ? 0.56f : 0.40f)) * loud;
            y = y * (1.0f - clipMix) + clipped * clipMix;

            wet[i] = y * outputGain;
        }
    }

    finalLimiter.setThreshold(maxMode ? -0.4f : -0.8f);
    finalLimiter.setRelease(drumMode ? (maxMode ? 44.0f : 62.0f)
                                     : (maxMode ? 58.0f : 82.0f));

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        finalLimiter.process(ctx);
    }

    updateMeters(buffer, outputMeterL, outputMeterR);
    pushVisualizationSamples(buffer);
}

juce::AudioProcessorEditor* BassEngineAudioProcessor::createEditor()
{
    return new BassEngineAudioProcessorEditor(*this);
}

void BassEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void BassEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new BassEngineAudioProcessor();
}
