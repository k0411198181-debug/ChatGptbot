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

SweetEngineAudioProcessor::SweetEngineAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SweetEngineAudioProcessor::createParameterLayout()
{
    using APF = juce::AudioParameterFloat;
    using APB = juce::AudioParameterBool;
    using APC = juce::AudioParameterChoice;

    juce::AudioProcessorValueTreeState::ParameterLayout p;

    p.add(std::make_unique<APF>(juce::ParameterID{"clean",1},  "Clean",  0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"tame",1},   "Tame",   0.0f, 100.0f, 42.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"detail",1}, "Detail", 0.0f, 100.0f, 52.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"sweet",1},  "Sweet",  0.0f, 100.0f, 58.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"air",1},    "Air",    0.0f, 100.0f, 48.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"body",1},   "Body",   0.0f, 100.0f, 50.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"width",1},  "Width",  0.0f, 100.0f, 52.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"impact",1}, "Impact", 0.0f, 100.0f, 45.0f));

    p.add(std::make_unique<APF>(juce::ParameterID{"mix",1},    "Mix",    0.0f, 100.0f, 100.0f));
    p.add(std::make_unique<APF>(juce::ParameterID{"output",1}, "Output", -12.0f, 6.0f, 0.0f));

    p.add(std::make_unique<APC>(juce::ParameterID{"mode",1}, "Mode",
                                juce::StringArray{"SYNTH", "STRINGS"}, 0));

    p.add(std::make_unique<APB>(juce::ParameterID{"auto",1},   "Auto", true));
    p.add(std::make_unique<APB>(juce::ParameterID{"max",1},    "Max", false));
    p.add(std::make_unique<APB>(juce::ParameterID{"delta",1},  "Delta", false));
    p.add(std::make_unique<APB>(juce::ParameterID{"bypass",1}, "Bypass", false));

    return p;
}

bool SweetEngineAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet();
    const auto out = layouts.getMainOutputChannelSet();

    return in == out
        && (out == juce::AudioChannelSet::mono() || out == juce::AudioChannelSet::stereo());
}

void SweetEngineAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;

    rumbleHP.prepare(sampleRate);
    lp180.prepare(sampleRate, 180.0f);
    lp450.prepare(sampleRate, 450.0f);
    lp1800.prepare(sampleRate, 1800.0f);
    lp6500.prepare(sampleRate, 6500.0f);
    lp11000.prepare(sampleRate, 11000.0f);
    sideLow.prepare(sampleRate, 260.0f);

    juce::dsp::ProcessSpec spec {
        sampleRate,
        (juce::uint32) samplesPerBlock,
        (juce::uint32) getTotalNumOutputChannels()
    };

    glueCompressor.prepare(spec);
    glueCompressor.setAttack(18.0f);
    glueCompressor.setRelease(140.0f);

    finalLimiter.prepare(spec);
    finalLimiter.setThreshold(-0.8f);
    finalLimiter.setRelease(75.0f);

    dryBuffer.setSize(getTotalNumOutputChannels(), samplesPerBlock, false, true, true);

    harshEnvelope.fill(0.0f);
    brightEnvelope.fill(0.0f);
    transientFast = transientSlow = 0.0f;

    inputMeterL.store(0.0f, std::memory_order_relaxed);
    inputMeterR.store(0.0f, std::memory_order_relaxed);
    outputMeterL.store(0.0f, std::memory_order_relaxed);
    outputMeterR.store(0.0f, std::memory_order_relaxed);

    visualFifo.reset();
}

float SweetEngineAudioProcessor::getInputMeter(int channel) const noexcept
{
    return (channel == 0 ? inputMeterL : inputMeterR).load(std::memory_order_relaxed);
}

float SweetEngineAudioProcessor::getOutputMeter(int channel) const noexcept
{
    return (channel == 0 ? outputMeterL : outputMeterR).load(std::memory_order_relaxed);
}

void SweetEngineAudioProcessor::updateMeters(const juce::AudioBuffer<float>& buffer,
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

void SweetEngineAudioProcessor::pushVisualizationSamples(const juce::AudioBuffer<float>& buffer) noexcept
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

int SweetEngineAudioProcessor::pullVisualizationSamples(float* dest, int maxSamples) noexcept
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

void SweetEngineAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
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

    float clean  = apvts.getRawParameterValue("clean")->load() * 0.01f;
    float tame   = apvts.getRawParameterValue("tame")->load() * 0.01f;
    float detail = apvts.getRawParameterValue("detail")->load() * 0.01f;
    float sweet  = apvts.getRawParameterValue("sweet")->load() * 0.01f;
    float air    = apvts.getRawParameterValue("air")->load() * 0.01f;
    float body   = apvts.getRawParameterValue("body")->load() * 0.01f;
    float width  = apvts.getRawParameterValue("width")->load() * 0.01f;
    float impact = apvts.getRawParameterValue("impact")->load() * 0.01f;

    const float mix = apvts.getRawParameterValue("mix")->load() * 0.01f;
    const float outputGain = dbToGain(apvts.getRawParameterValue("output")->load());

    const bool stringsMode = apvts.getRawParameterValue("mode")->load() > 0.5f;
    const bool autoMode = apvts.getRawParameterValue("auto")->load() > 0.5f;
    const bool maxMode = apvts.getRawParameterValue("max")->load() > 0.5f;
    const bool deltaMode = apvts.getRawParameterValue("delta")->load() > 0.5f;

    if (!maxMode)
    {
        clean  = juce::jmin(clean, 0.88f);
        tame   = juce::jmin(tame, stringsMode ? 0.88f : 0.80f);
        detail = juce::jmin(detail, 0.82f);
        sweet  = juce::jmin(sweet, 0.82f);
        air    = juce::jmin(air, stringsMode ? 0.78f : 0.82f);
        body   = juce::jmin(body, 0.82f);
        width  = juce::jmin(width, 0.82f);
        impact = juce::jmin(impact, stringsMode ? 0.66f : 0.82f);
    }

    const float hpCut = juce::jmap(clean,
                                   stringsMode ? 22.0f : 25.0f,
                                   stringsMode ? 34.0f : 42.0f);

    const float harshAttack = std::exp(-1.0f / ((stringsMode ? 0.006f : 0.0035f) * (float)currentSampleRate));
    const float harshRelease = std::exp(-1.0f / ((stringsMode ? 0.150f : 0.105f) * (float)currentSampleRate));
    const float brightAttack = std::exp(-1.0f / (0.005f * (float)currentSampleRate));
    const float brightRelease = std::exp(-1.0f / (0.125f * (float)currentSampleRate));

    const float fastA = std::exp(-1.0f / ((stringsMode ? 0.0045f : 0.0023f) * (float)currentSampleRate));
    const float fastR = std::exp(-1.0f / ((stringsMode ? 0.070f : 0.045f) * (float)currentSampleRate));
    const float slowA = std::exp(-1.0f / ((stringsMode ? 0.045f : 0.028f) * (float)currentSampleRate));
    const float slowR = std::exp(-1.0f / ((stringsMode ? 0.300f : 0.220f) * (float)currentSampleRate));

    for (int i = 0; i < n; ++i)
    {
        float monoAbs = 0.0f;

        for (int ch = 0; ch < nCh; ++ch)
        {
            auto* d = buffer.getWritePointer(ch);

            const float x = rumbleHP.process(d[i], ch, hpCut);

            const float low180 = lp180.process(x, ch);
            const float low450 = lp450.process(x, ch);
            const float low1800 = lp1800.process(x, ch);
            const float low6500 = lp6500.process(x, ch);
            const float low11000 = lp11000.process(x, ch);

            const float bodyBand = low450 - low180;
            const float mudBand = low1800 - low450;
            const float presenceBand = low6500 - low1800;
            const float sheenBand = low11000 - low6500;
            const float airBand = x - low11000;

            harshEnvelope[(size_t)ch] = smoothEnv(std::abs(presenceBand),
                                                  harshEnvelope[(size_t)ch],
                                                  harshAttack, harshRelease);
            brightEnvelope[(size_t)ch] = smoothEnv(std::abs(sheenBand + airBand),
                                                   brightEnvelope[(size_t)ch],
                                                   brightAttack, brightRelease);

            float adaptiveTame = tame;
            float adaptiveDetail = detail;

            if (autoMode)
            {
                const float harshness = juce::jlimit(0.0f, 1.0f,
                    (harshEnvelope[(size_t)ch] - (stringsMode ? 0.055f : 0.070f))
                    * (stringsMode ? 8.0f : 6.0f));

                const float brightness = juce::jlimit(0.0f, 1.0f,
                    (brightEnvelope[(size_t)ch] - 0.045f) * 7.0f);

                adaptiveTame = juce::jlimit(0.0f, 1.0f,
                    tame + harshness * (stringsMode ? 0.30f : 0.22f));
                adaptiveDetail *= 1.0f - brightness * 0.30f;
            }

            // CLEAN: remove low-mid congestion while keeping musical body.
            const float mudReduction = clean
                * (stringsMode ? 0.18f : 0.24f)
                * juce::jlimit(0.0f, 1.0f, std::abs(mudBand) / (std::abs(x) + 1.0e-4f));

            float y = x - mudBand * mudReduction;

            // TAME: adaptive presence/fizz restraint. Smooth and level-dependent.
            const float harshOver = juce::jlimit(0.0f, 1.0f,
                (harshEnvelope[(size_t)ch] - (stringsMode ? 0.045f : 0.060f))
                * (stringsMode ? 8.5f : 6.5f));

            const float highOver = juce::jlimit(0.0f, 1.0f,
                (brightEnvelope[(size_t)ch] - 0.040f) * 7.5f);

            const float presenceReduction = adaptiveTame
                * harshOver
                * (stringsMode ? (maxMode ? 0.46f : 0.36f)
                               : (maxMode ? 0.38f : 0.28f));

            const float highReduction = adaptiveTame
                * highOver
                * (stringsMode ? (maxMode ? 0.30f : 0.22f)
                               : (maxMode ? 0.34f : 0.24f));

            y -= presenceBand * presenceReduction;
            y -= (sheenBand + airBand) * highReduction;

            // BODY: parallel warm saturation in the musical low-mid region.
            const float bodyDrive = 1.0f + body * (stringsMode ? 2.7f : 3.4f);
            const float bodySat = std::tanh(bodyBand * bodyDrive) / std::tanh(bodyDrive);
            y += bodyBand * body * (stringsMode ? 0.20f : 0.16f);
            y += (bodySat - bodyBand) * body * (stringsMode ? 0.42f : 0.34f);

            // DETAIL / Recover-like macro: expose masked mid-high information without a static EQ shelf.
            const float detailSource = 0.72f * presenceBand + 0.28f * sheenBand;
            const float detailDrive = 1.0f + adaptiveDetail * (stringsMode ? 2.3f : 3.1f);
            const float detailSat = std::tanh(detailSource * detailDrive) / std::tanh(detailDrive);
            y += detailSource * adaptiveDetail * (stringsMode ? 0.10f : 0.15f);
            y += (detailSat - detailSource) * adaptiveDetail * (stringsMode ? 0.28f : 0.36f);

            // SWEET: multi-region harmonic polish rather than plain EQ boost.
            const float sweetSource = 0.30f * bodyBand + 0.52f * mudBand
                                    + 0.72f * presenceBand + 0.34f * sheenBand;
            const float sweetDrive = 1.0f + sweet * (stringsMode
                                      ? (maxMode ? 5.2f : 3.8f)
                                      : (maxMode ? 6.0f : 4.4f));
            const float sweetSat = std::tanh(sweetSource * sweetDrive) / std::tanh(sweetDrive);
            y += (sweetSat - sweetSource) * sweet * (stringsMode
                                      ? (maxMode ? 0.58f : 0.42f)
                                      : (maxMode ? 0.68f : 0.48f));

            // AIR: top-end sheen with auto protection already feeding TAME above.
            const float airSource = 0.62f * sheenBand + airBand;
            const float airDrive = 1.0f + air * (stringsMode ? 2.1f : 2.7f);
            const float airSat = std::tanh(airSource * airDrive) / std::tanh(airDrive);
            y += airSource * air * (stringsMode ? 0.10f : 0.13f);
            y += (airSat - airSource) * air * (stringsMode ? 0.24f : 0.30f);

            monoAbs = juce::jmax(monoAbs, std::abs(y));
            d[i] = y;
        }

        // IMPACT: program-dependent transient articulation.
        transientFast = smoothEnv(monoAbs, transientFast, fastA, fastR);
        transientSlow = smoothEnv(monoAbs, transientSlow, slowA, slowR);
        const float transient = juce::jmax(0.0f, transientFast - transientSlow);

        const float impactGain = 1.0f + juce::jlimit(
            0.0f,
            stringsMode ? (maxMode ? 0.16f : 0.10f) : (maxMode ? 0.28f : 0.18f),
            transient * impact * (stringsMode ? 1.8f : 3.0f));

        for (int ch = 0; ch < nCh; ++ch)
            buffer.getWritePointer(ch)[i] *= impactGain;
    }

    // Gentle glue, mode-dependent.
    glueCompressor.setThreshold(juce::jmap(sweet, -2.0f,
        stringsMode ? (maxMode ? -9.5f : -7.0f) : (maxMode ? -11.0f : -8.0f)));
    glueCompressor.setRatio(juce::jmap(sweet, 1.0f,
        stringsMode ? (maxMode ? 1.8f : 1.45f) : (maxMode ? 2.0f : 1.6f)));
    glueCompressor.setAttack(stringsMode ? 28.0f : 16.0f);
    glueCompressor.setRelease(stringsMode ? 190.0f : 125.0f);

    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        glueCompressor.process(ctx);
    }

    // WIDTH: widen upper content, keep the low region controlled.
    if (nCh >= 2)
    {
        auto* l = buffer.getWritePointer(0);
        auto* r = buffer.getWritePointer(1);

        for (int i = 0; i < n; ++i)
        {
            const float mid = 0.5f * (l[i] + r[i]);
            const float side = 0.5f * (l[i] - r[i]);

            const float lowSide = sideLow.process(side);
            const float highSide = side - lowSide;

            const float lowSideGain = juce::jmap(width, 0.82f, 0.55f);
            const float highSideGain = 1.0f + width * (stringsMode
                                     ? (maxMode ? 0.72f : 0.50f)
                                     : (maxMode ? 0.62f : 0.44f));

            const float newSide = lowSide * lowSideGain + highSide * highSideGain;

            l[i] = mid + newSide;
            r[i] = mid - newSide;
        }
    }

    // Wet/dry, DELTA, output.
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto* wet = buffer.getWritePointer(ch);
        const auto* dry = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < n; ++i)
        {
            const float processed = wet[i];
            const float mixed = dry[i] * (1.0f - mix) + processed * mix;
            wet[i] = (deltaMode ? (processed - dry[i]) : mixed) * outputGain;
        }
    }

    // Safety limiter. This is a conventional sample-peak limiter, not true-peak.
    finalLimiter.setThreshold(maxMode ? -0.35f : -0.8f);
    finalLimiter.setRelease(stringsMode ? 95.0f : 72.0f);

    if (!deltaMode)
    {
        juce::dsp::AudioBlock<float> block(buffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        finalLimiter.process(ctx);
    }

    updateMeters(buffer, outputMeterL, outputMeterR);
    pushVisualizationSamples(buffer);
}

juce::AudioProcessorEditor* SweetEngineAudioProcessor::createEditor()
{
    return new SweetEngineAudioProcessorEditor(*this);
}

void SweetEngineAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void SweetEngineAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml(getXmlFromBinary(data, sizeInBytes));

    if (xml && xml->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SweetEngineAudioProcessor();
}
