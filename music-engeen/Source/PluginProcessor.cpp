#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
    inline float dbToGain(float db)
    {
        return juce::Decibels::decibelsToGain(db);
    }

    inline float smoothEnvelope(float input, float current, float attackCoeff, float releaseCoeff)
    {
        return input > current
            ? attackCoeff * current + (1.0f - attackCoeff) * input
            : releaseCoeff * current + (1.0f - releaseCoeff) * input;
    }
}

MusicEngeenAudioProcessor::MusicEngeenAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout MusicEngeenAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;

    juce::AudioProcessorValueTreeState::ParameterLayout p;

    p.add(std::make_unique<APF>(juce::ParameterID{"clean", 1},  "Clean",  0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"weight", 1}, "Weight", 0.0f, 100.0f, 58.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"punch", 1},  "Punch",  0.0f, 100.0f, 46.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"glue", 1},   "Glue",   0.0f, 100.0f, 50.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"shine", 1},  "Shine",  0.0f, 100.0f, 38.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"width", 1},  "Width",  0.0f, 100.0f, 46.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"loud", 1},   "Loud",   0.0f, 100.0f, 34.0f));

    p.add(std::make_unique<APF>(juce::ParameterID{"sub", 1},    "Sub",    0.0f, 100.0f, 42.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"tilt", 1},   "Tilt",   0.0f, 100.0f, 50.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"mix", 1},    "Mix",    0.0f, 100.0f, 100.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"output", 1}, "Output", -12.0f, 6.0f, 0.0f));

    p.add(std::make_unique<APB>(juce::ParameterID{"auto", 1},   "Auto", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"safe", 1},   "Safe", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"bypass", 1}, "Bypass", false));

    return p;
}

bool MusicEngeenAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    return in == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void MusicEngeenAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    rumbleHP.prepare(sampleRate);
    low90.prepare(sampleRate, 90.0f);
    low180.prepare(sampleRate, 180.0f);
    low420.prepare(sampleRate, 420.0f);
    low4k5.prepare(sampleRate, 4500.0f);
    low9k5.prepare(sampleRate, 9500.0f);
    sideHP.prepare(sampleRate);

    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32)samplesPerBlock,
        (juce::uint32)getTotalNumOutputChannels()
    };

    glueCompressor.prepare(spec);
    safetyLimiter.prepare(spec);

    glueCompressor.setAttack(16.0f);
    glueCompressor.setRelease(140.0f);
    safetyLimiter.setThreshold(-1.0f);
    safetyLimiter.setRelease(80.0f);

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, true);

    transientFast = 0.0f;
    transientSlow = 0.0f;
    adaptiveLow = 0.0f;
    adaptiveHigh = 0.0f;

    inputMeterL.store(0.0f, std::memory_order_relaxed);
    inputMeterR.store(0.0f, std::memory_order_relaxed);
    outputMeterL.store(0.0f, std::memory_order_relaxed);
    outputMeterR.store(0.0f, std::memory_order_relaxed);
    visualFifo.reset();
}

float MusicEngeenAudioProcessor::getInputMeter(int channel) const noexcept
{
    return (channel == 0 ? inputMeterL : inputMeterR).load(std::memory_order_relaxed);
}

float MusicEngeenAudioProcessor::getOutputMeter(int channel) const noexcept
{
    return (channel == 0 ? outputMeterL : outputMeterR).load(std::memory_order_relaxed);
}

void MusicEngeenAudioProcessor::updateMeters(const juce::AudioBuffer<float>& buffer,
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

void MusicEngeenAudioProcessor::pushVisualizationSamples(const juce::AudioBuffer<float>& buffer) noexcept
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

int MusicEngeenAudioProcessor::pullVisualizationSamples(float* dest, int maxSamples) noexcept
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

void MusicEngeenAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    float clean  = apvts.getRawParameterValue("clean")->load()  * 0.01f;
    float weight = apvts.getRawParameterValue("weight")->load() * 0.01f;
    float punch  = apvts.getRawParameterValue("punch")->load()  * 0.01f;
    float glue   = apvts.getRawParameterValue("glue")->load()   * 0.01f;
    float shine  = apvts.getRawParameterValue("shine")->load()  * 0.01f;
    float width  = apvts.getRawParameterValue("width")->load()  * 0.01f;
    float loud   = apvts.getRawParameterValue("loud")->load()   * 0.01f;

    float sub = apvts.getRawParameterValue("sub")->load() * 0.01f;
    const float tilt = (apvts.getRawParameterValue("tilt")->load() - 50.0f) / 50.0f;
    const float mix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    const float outputGain = dbToGain(apvts.getRawParameterValue("output")->load());

    const bool autoMode = apvts.getRawParameterValue("auto")->load() > 0.5f;
    const bool safeMode = apvts.getRawParameterValue("safe")->load() > 0.5f;

    if (safeMode)
    {
        weight = juce::jmin(weight, 0.78f);
        punch = juce::jmin(punch, 0.78f);
        glue = juce::jmin(glue, 0.82f);
        shine = juce::jmin(shine, 0.78f);
        width = juce::jmin(width, 0.82f);
        loud = juce::jmin(loud, 0.72f);
        sub = juce::jmin(sub, 0.72f);
    }

    const float hpCut = juce::jmap(clean, 27.0f, 48.0f);

    const float fastA = std::exp(-1.0f / (0.0035f * (float)currentSampleRate));
    const float fastR = std::exp(-1.0f / (0.055f * (float)currentSampleRate));
    const float slowA = std::exp(-1.0f / (0.035f * (float)currentSampleRate));
    const float slowR = std::exp(-1.0f / (0.240f * (float)currentSampleRate));
    const float adaptA = std::exp(-1.0f / (0.025f * (float)currentSampleRate));
    const float adaptR = std::exp(-1.0f / (0.260f * (float)currentSampleRate));

    for (int i = 0; i < n; ++i)
    {
        float monoAbs = 0.0f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);
            const float x0 = rumbleHP.process(d[i], ch, hpCut);

            const float lp90 = low90.process(x0, ch);
            const float lp180 = low180.process(x0, ch);
            const float lp420 = low420.process(x0, ch);
            const float lp4k5 = low4k5.process(x0, ch);
            const float lp9k5 = low9k5.process(x0, ch);

            const float mud = lp420 - lp180;
            const float high = x0 - lp4k5;
            const float fizz = x0 - lp9k5;

            const float broad = std::abs(x0) + 1.0e-4f;
            const float mudRatio = std::abs(mud) / broad;

            const float mudGR = juce::jlimit(0.0f, 0.30f,
                                            (mudRatio - 0.34f) * (0.32f + 0.95f * clean));

            float y = x0 - mud * mudGR;

            const float lowEnergy = std::abs(lp180);
            const float highEnergy = std::abs(high);

            adaptiveLow = smoothEnvelope(lowEnergy, adaptiveLow, adaptA, adaptR);
            adaptiveHigh = smoothEnvelope(highEnergy, adaptiveHigh, adaptA, adaptR);

            float lowProtection = 0.0f;
            float highProtection = 0.0f;

            if (autoMode)
            {
                lowProtection = juce::jlimit(0.0f, 0.36f, (adaptiveLow - 0.12f) * 2.4f);
                highProtection = juce::jlimit(0.0f, 0.34f, (adaptiveHigh - 0.075f) * 3.2f);
            }

            const float effectiveWeight = weight * (1.0f - lowProtection);
            const float effectiveSub = sub * (1.0f - 0.75f * lowProtection);
            const float effectiveShine = shine * (1.0f - highProtection);

            const float lowSat = std::tanh(lp180 * 3.0f) / std::tanh(3.0f);
            y += lp180 * (0.18f * effectiveWeight);
            y += (lowSat - lp180) * (0.24f * effectiveWeight);
            y += lp90 * (0.14f * effectiveSub);

            const float fizzRatio = std::abs(fizz) / broad;
            const float fizzGuard = juce::jlimit(0.0f, 0.55f,
                                                (fizzRatio - 0.11f)
                                                * (0.80f + 1.10f * effectiveShine));

            y += high * (0.18f * effectiveShine);
            y -= fizz * fizzGuard;

            if (tilt >= 0.0f)
            {
                y += high * (0.20f * tilt);
                y -= lp420 * (0.08f * tilt);
            }
            else
            {
                const float dark = -tilt;
                y -= high * (0.18f * dark);
                y += lp420 * (0.10f * dark);
            }

            monoAbs = juce::jmax(monoAbs, std::abs(y));
            d[i] = y;
        }

        transientFast = smoothEnvelope(monoAbs, transientFast, fastA, fastR);
        transientSlow = smoothEnvelope(monoAbs, transientSlow, slowA, slowR);

        const float transient = juce::jmax(0.0f, transientFast - transientSlow);
        const float punchGain = 1.0f + juce::jlimit(0.0f,
                                                    safeMode ? 0.18f : 0.30f,
                                                    transient * (1.8f + 3.0f * punch));

        for (int ch = 0; ch < nCh; ++ch)
            buffer.getWritePointer(ch)[i] *= punchGain;
    }

    const float glueThreshold = juce::jmap(glue, -4.0f, safeMode ? -15.0f : -20.0f);
    const float glueRatio = juce::jmap(glue, 1.10f, safeMode ? 2.8f : 3.8f);
    const float glueAttack = juce::jmap(glue, 24.0f, 9.0f);
    const float glueRelease = juce::jmap(glue, 185.0f, 95.0f);

    glueCompressor.setThreshold(glueThreshold);
    glueCompressor.setRatio(glueRatio);
    glueCompressor.setAttack(glueAttack);
    glueCompressor.setRelease(glueRelease);

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        glueCompressor.process(ctx);
    }

    if (nCh >= 2)
    {
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);

        const float highSideGain = juce::jmap(width, 0.78f, safeMode ? 1.42f : 1.62f);
        const float lowSideGain = juce::jmap(width, 0.72f, safeMode ? 0.28f : 0.40f);

        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);

            const float highSide = sideHP.process(side, 135.0f);
            const float lowSide = side - highSide;
            const float newSide = highSide * highSideGain + lowSide * lowSideGain;

            l[i] = mid + newSide;
            r[i] = mid - newSide;
        }
    }

    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        const auto* dry = dryBuffer.getReadPointer(ch);

        const float drive = 1.0f + loud * (safeMode ? 1.65f : 2.45f);
        const float norm = std::tanh(drive);

        for (int i = 0; i < n; ++i)
        {
            float y = dry[i] * (1.0f - mix) + wet[i] * mix;

            const float clipped = std::tanh(y * drive) / norm;
            y = y * (1.0f - 0.38f * loud) + clipped * (0.38f * loud);
            wet[i] = y * outputGain;
        }
    }

    safetyLimiter.setThreshold(safeMode ? -1.0f : -0.5f);
    safetyLimiter.setRelease(safeMode ? 95.0f : 70.0f);

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        safetyLimiter.process(ctx);
    }

    updateMeters(buffer, outputMeterL, outputMeterR);
    pushVisualizationSamples(buffer);
}

juce::AudioProcessorEditor* MusicEngeenAudioProcessor::createEditor()
{
    return new MusicEngeenAudioProcessorEditor(*this);
}

void MusicEngeenAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void MusicEngeenAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new MusicEngeenAudioProcessor();
}
