#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
class PianoSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class PianoVoice : public juce::SynthesiserVoice
{
public:
    explicit PianoVoice(juce::AudioProcessorValueTreeState& s) : state(s) {}
    bool canPlaySound(juce::SynthesiserSound* s) override { return dynamic_cast<PianoSound*>(s) != nullptr; }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int pitchWheel) override
    {
        note = midiNoteNumber;
        vel = juce::jlimit(0.0f, 1.0f, velocity);
        baseHz = (float)juce::MidiMessage::getMidiNoteInHertz(note);
        phase.fill(0.0);
        age = 0.0;
        releaseAge = 0.0;
        released = false;
        pendingRelease = false;
        hammer = 1.0f;
        pitchWheelMoved(pitchWheel);
        const float w = get("width") * 0.01f;
        const float n = ((note * 37) % 23) / 22.0f * 2.0f - 1.0f;
        pan = n * 0.45f * w;
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (sustain && allowTailOff) { pendingRelease = true; return; }
        released = true;
        releaseAge = 0.0;
        if (!allowTailOff) clearCurrentNote();
    }

    void pitchWheelMoved(int value) override
    {
        const float semis = ((float)value - 8192.0f) / 8192.0f * 2.0f;
        bend = std::pow(2.0f, semis / 12.0f);
    }

    void controllerMoved(int cc, int value) override
    {
        if (cc == 64)
        {
            const bool was = sustain;
            sustain = value >= 64;
            if (was && !sustain && pendingRelease)
            {
                released = true;
                releaseAge = 0.0;
                pendingRelease = false;
            }
        }
        else if (cc == 1)
            mod = (float)value / 127.0f;
    }

    void renderNextBlock(juce::AudioBuffer<float>& out, int start, int num) override
    {
        if (!isVoiceActive()) return;
        const double sr = getSampleRate();
        if (sr <= 0.0) return;

        const int mode = (int)get("mode");
        const float character = get("character") * 0.01f;
        const float bright = get("bright") * 0.01f;
        const float body = get("body") * 0.01f;
        const float felt = get("felt") * 0.01f;
        const float damp = get("damp") * 0.01f;
        const float att = 0.0015f + get("attack") * 0.00045f;
        const float rel = 0.08f + get("release") * 0.018f;
        const float velSens = get("vel") * 0.01f;
        const float velocityGain = juce::jmap(velSens, 1.0f, 0.18f + 0.82f * vel);

        float modeBright=1.0f, modeDecay=1.0f, modeBody=1.0f;
        if (mode==1) { modeBright=0.72f; modeDecay=0.92f; modeBody=1.08f; }
        else if (mode==2) { modeBright=0.58f; modeDecay=0.68f; modeBody=0.92f; }
        else if (mode==3) { modeBright=0.64f; modeDecay=1.75f; modeBody=1.18f; }

        const float partialBase[6] {1.0f, 0.36f, 0.22f, 0.13f, 0.075f, 0.045f};
        const float ratios[6] {1.0f, 2.01f, 3.015f, 4.02f, 5.035f, 6.05f};
        const float detune = 1.0f + (character - 0.5f) * 0.0016f;
        const float freq = baseHz * bend;
        const float leftGain = std::sqrt(0.5f * (1.0f - pan));
        const float rightGain = std::sqrt(0.5f * (1.0f + pan));

        for (int i=0;i<num;++i)
        {
            const double t = age / sr;
            const float attackEnv = (float)juce::jlimit(0.0,1.0,t/(double)att);
            const double decayTime = (2.4 + (1.0-damp)*5.5) * modeDecay;
            float env = attackEnv * (float)std::exp(-t/decayTime);
            if (released)
            {
                const float r = (float)std::exp(-releaseAge/(sr*(double)rel));
                env *= r;
                releaseAge += 1.0;
                if (r < 0.00025f) { clearCurrentNote(); break; }
            }

            float s=0.0f;
            for (int p=0;p<6;++p)
            {
                float amp=partialBase[p];
                amp *= (p==0 ? 0.65f+0.75f*body*modeBody : 0.48f+0.95f*bright*modeBright);
                amp *= std::pow(0.88f, (float)p * damp * 0.8f);
                phase[(size_t)p] += juce::MathConstants<double>::twoPi * (double)(freq*ratios[p]*std::pow(detune,(float)p)) / sr;
                if (phase[(size_t)p] > juce::MathConstants<double>::twoPi) phase[(size_t)p] -= juce::MathConstants<double>::twoPi;
                s += (float)std::sin(phase[(size_t)p]) * amp;
            }

            hammer *= 0.9915f - felt*0.0015f;
            const float noise = std::sin((float)(phase[5]*3.17 + note)) * hammer * felt * 0.11f;
            s = std::tanh((s*0.52f + noise) * (1.0f + character*0.35f));
            if (mode==2) s = std::round(s*96.0f)/96.0f;
            if (mode==3) s = 0.82f*s + 0.18f*(float)std::sin(phase[0]*0.5);
            s *= env * velocityGain * 0.42f * (0.95f + mod*0.08f);

            if (out.getNumChannels()>0) out.addSample(0,start+i,s*leftGain);
            if (out.getNumChannels()>1) out.addSample(1,start+i,s*rightGain);
            age += 1.0;
        }
    }

private:
    float get(const char* id) const { return state.getRawParameterValue(id)->load(); }
    juce::AudioProcessorValueTreeState& state;
    std::array<double,6> phase{};
    int note=60;
    float vel=0.8f, baseHz=261.6f, bend=1.0f, mod=0.0f, pan=0.0f, hammer=0.0f;
    double age=0.0, releaseAge=0.0;
    bool sustain=false, pendingRelease=false, released=false;
};
}

KGSketchPianoAudioProcessor::KGSketchPianoAudioProcessor()
 : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
   apvts(*this,nullptr,"PARAMETERS",createParameterLayout())
{
    for (int i=0;i<32;++i) synth.addVoice(new PianoVoice(apvts));
    synth.addSound(new PianoSound());
}

juce::AudioProcessorValueTreeState::ParameterLayout KGSketchPianoAudioProcessor::createParameterLayout()
{
    using F=juce::AudioParameterFloat; using C=juce::AudioParameterChoice;
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<C>(juce::ParameterID{"mode",1},"Mode",juce::StringArray{"GRAND","SOFT","LOFI","PAD"},0));
    const std::array<std::pair<const char*,float>,11> defs {{
        {"character",50.f},{"bright",56.f},{"body",58.f},{"felt",24.f},{"width",58.f},{"space",28.f},
        {"damp",46.f},{"attack",10.f},{"release",48.f},{"vel",78.f},{"mix",100.f}
    }};
    for (auto spec : defs)
        p.add(std::make_unique<F>(juce::ParameterID{spec.first,1},juce::String(spec.first).toUpperCase(),0.f,100.f,spec.second));
    p.add(std::make_unique<F>(juce::ParameterID{"output",1},"OUTPUT",-18.f,6.f,0.f));
    return p;
}

bool KGSketchPianoAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    auto o=l.getMainOutputChannelSet(); return o==juce::AudioChannelSet::mono() || o==juce::AudioChannelSet::stereo();
}

void KGSketchPianoAudioProcessor::prepareToPlay(double sr,int block)
{
    synth.setCurrentPlaybackSampleRate(sr);
    juce::dsp::ProcessSpec spec{sr,(juce::uint32)block,(juce::uint32)getTotalNumOutputChannels()};
    reverb.prepare(spec);
}

void KGSketchPianoAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals n;
    buffer.clear();
    keyboardState.processNextMidiBuffer(midi,0,buffer.getNumSamples(),true);
    synth.renderNextBlock(buffer,midi,0,buffer.getNumSamples());

    juce::dsp::Reverb::Parameters rp;
    const float space=apvts.getRawParameterValue("space")->load()*0.01f;
    rp.roomSize=0.18f+space*0.62f; rp.damping=0.45f; rp.wetLevel=space*0.26f; rp.dryLevel=1.0f-rp.wetLevel*0.22f; rp.width=0.72f;
    reverb.setParameters(rp);
    juce::dsp::AudioBlock<float> block(buffer); juce::dsp::ProcessContextReplacing<float> ctx(block); reverb.process(ctx);
    const float mix=apvts.getRawParameterValue("mix")->load()*0.01f;
    buffer.applyGain(mix);
    buffer.applyGain(juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load()));
}

juce::AudioProcessorEditor* KGSketchPianoAudioProcessor::createEditor(){ return new KGSketchPianoAudioProcessorEditor(*this); }
void KGSketchPianoAudioProcessor::getStateInformation(juce::MemoryBlock& d){ auto s=apvts.copyState(); auto x=s.createXml(); copyXmlToBinary(*x,d); }
void KGSketchPianoAudioProcessor::setStateInformation(const void* d,int z){ auto x=getXmlFromBinary(d,z); if(x&&x->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*x)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new KGSketchPianoAudioProcessor(); }
