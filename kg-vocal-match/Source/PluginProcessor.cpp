#include "PluginProcessor.h"
#include "PluginEditor.h"

void KGVocalMatchAudioProcessor::PitchDetector::prepare(double sr){ sampleRate=sr; reset(); }
void KGVocalMatchAudioProcessor::PitchDetector::reset(){ ring.fill(0.0f); wp=downCounter=hopCounter=0; }
void KGVocalMatchAudioProcessor::PitchDetector::push(float x)
{
    if (++downCounter>=down){ downCounter=0; ring[(size_t)wp]=x; wp=(wp+1)%N; }
    ++hopCounter;
}
bool KGVocalMatchAudioProcessor::PitchDetector::shouldAnalyse() noexcept
{
    const int hop=juce::jmax(64,(int)(sampleRate/100.0));
    if(hopCounter>=hop){ hopCounter=0; return true; }
    return false;
}
std::pair<float,float> KGVocalMatchAudioProcessor::PitchDetector::analyse()
{
    std::array<float,N> x{};
    float mean=0.0f;
    for(int i=0;i<N;++i){ x[(size_t)i]=ring[(size_t)((wp+i)%N)]; mean+=x[(size_t)i]; }
    mean/=N;
    float e=0.0f;
    for(int i=0;i<N;++i){ x[(size_t)i]-=mean; const float w=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*(float)i/(float)(N-1)); x[(size_t)i]*=w; e+=x[(size_t)i]*x[(size_t)i]; }
    if(e<1.0e-5f) return {0.0f,0.0f};

    const double dsr=sampleRate/(double)down;
    const int minLag=juce::jmax(2,(int)(dsr/900.0));
    const int maxLag=juce::jmin(N/2,(int)(dsr/70.0));
    float best=-1.0f; int bestLag=0;
    std::array<float,N/2+1> corr{};
    for(int lag=minLag;lag<=maxLag;++lag)
    {
        double num=0,e1=0,e2=0;
        const int count=N-lag;
        for(int i=0;i<count;++i){ const float a=x[(size_t)i],b=x[(size_t)(i+lag)]; num+=a*b; e1+=a*a; e2+=b*b; }
        const float c=(float)(num/std::sqrt(e1*e2+1.0e-12)); corr[(size_t)lag]=c;
        if(c>best){ best=c; bestLag=lag; }
    }
    if(bestLag<=0 || best<0.28f) return {0.0f,juce::jmax(0.0f,best)};
    float lagF=(float)bestLag;
    if(bestLag>minLag && bestLag<maxLag){ const float a=corr[(size_t)(bestLag-1)],b=corr[(size_t)bestLag],c=corr[(size_t)(bestLag+1)]; const float den=a-2*b+c; if(std::abs(den)>1.0e-6f) lagF += 0.5f*(a-c)/den; }
    const float hz=(float)(dsr/(double)lagF);
    return {hz,juce::jlimit(0.0f,1.0f,best)};
}

void KGVocalMatchAudioProcessor::GrainShifter::prepare(double)
{
    size=16384; buf.assign((size_t)size,0.0f); reset();
}
void KGVocalMatchAudioProcessor::GrainShifter::reset(){ std::fill(buf.begin(),buf.end(),0.0f); wp=0; phase=0.0f; }
float KGVocalMatchAudioProcessor::GrainShifter::read(float delay) const noexcept
{
    float pos=(float)wp-delay;
    while(pos<0) pos+=(float)size;
    while(pos>=size) pos-=(float)size;
    const int i0=(int)pos, i1=(i0+1)%size; const float f=pos-(float)i0;
    return buf[(size_t)i0] + (buf[(size_t)i1]-buf[(size_t)i0])*f;
}
float KGVocalMatchAudioProcessor::GrainShifter::process(float x,float ratio,float dryWet,float voicedGate)
{
    buf[(size_t)wp]=x;
    ratio=juce::jlimit(0.50f,2.0f,ratio);
    const float dry=read((float)latency);
    float wet=dry;
    if(std::abs(ratio-1.0f)>0.00015f)
    {
        const float p2=std::fmod(phase+0.5f,1.0f);
        const float d1=(float)baseDelay+phase*(float)grain;
        const float d2=(float)baseDelay+p2*(float)grain;
        const float w1=0.5f-0.5f*std::cos(juce::MathConstants<float>::twoPi*phase);
        const float w2=1.0f-w1;
        wet=read(d1)*w1+read(d2)*w2;
        phase += (1.0f-ratio)/(float)grain;
        phase -= std::floor(phase);
    }
    wp=(wp+1)%size;
    const float effectiveWet=dryWet*voicedGate;
    return dry*(1.0f-effectiveWet)+wet*effectiveWet;
}

KGVocalMatchAudioProcessor::KGVocalMatchAudioProcessor()
 : AudioProcessor(BusesProperties()
      .withInput("Voice",juce::AudioChannelSet::stereo(),true)
      .withInput("Reference",juce::AudioChannelSet::stereo(),true)
      .withOutput("Output",juce::AudioChannelSet::stereo(),true)),
   apvts(*this,nullptr,"PARAMETERS",createParameterLayout()) {}

juce::AudioProcessorValueTreeState::ParameterLayout KGVocalMatchAudioProcessor::createParameterLayout()
{
    using F=juce::AudioParameterFloat; using B=juce::AudioParameterBool;
    juce::AudioProcessorValueTreeState::ParameterLayout p;
    p.add(std::make_unique<F>(juce::ParameterID{"match",1},"MATCH",0.f,100.f,88.f));
    p.add(std::make_unique<F>(juce::ParameterID{"speed",1},"SPEED",5.f,200.f,42.f));
    p.add(std::make_unique<F>(juce::ParameterID{"range",1},"RANGE",1.f,12.f,7.f));
    p.add(std::make_unique<F>(juce::ParameterID{"vibrato",1},"VIBRATO",0.f,100.f,82.f));
    p.add(std::make_unique<F>(juce::ParameterID{"natural",1},"NATURAL",0.f,100.f,78.f));
    p.add(std::make_unique<F>(juce::ParameterID{"tolerance",1},"TOLERANCE",0.f,50.f,8.f));
    p.add(std::make_unique<F>(juce::ParameterID{"mix",1},"MIX",0.f,100.f,100.f));
    p.add(std::make_unique<F>(juce::ParameterID{"output",1},"OUTPUT",-12.f,6.f,0.f));
    p.add(std::make_unique<B>(juce::ParameterID{"exact",1},"EXACT",false));
    p.add(std::make_unique<B>(juce::ParameterID{"bypass",1},"BYPASS",false));
    return p;
}

bool KGVocalMatchAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto in=l.getMainInputChannelSet(), out=l.getMainOutputChannelSet();
    if(in!=out || (out!=juce::AudioChannelSet::mono() && out!=juce::AudioChannelSet::stereo())) return false;
    const auto ref=l.getChannelSet(true,1);
    return ref==juce::AudioChannelSet::mono() || ref==juce::AudioChannelSet::stereo() || ref.isDisabled();
}

void KGVocalMatchAudioProcessor::prepareToPlay(double sampleRate,int)
{
    sr=sampleRate; liveDetector.prepare(sr); refDetector.prepare(sr); shifterL.prepare(sr); shifterR.prepare(sr); ratioSmoothed=1.0f; setLatencySamples(shifterL.getLatency());
}

static float nearestRefOctave(float ref,float live)
{
    if(ref<=0||live<=0) return ref;
    float best=ref,bestD=1.0e9f;
    for(int k=-2;k<=2;++k){ const float c=ref*std::pow(2.0f,(float)k); const float d=std::abs(1200.0f*std::log2(c/live)); if(d<bestD){bestD=d;best=c;} }
    return best;
}

void KGVocalMatchAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    auto voiceBus=getBusBuffer(buffer,true,0);
    auto refBus=getBusBuffer(buffer,true,1);
    auto outBus=getBusBuffer(buffer,false,0);
    const int n=outBus.getNumSamples();
    const int ch=outBus.getNumChannels();
    if(n<=0||ch<=0) return;

    float currentLive=liveHz.load(), currentRef=refHz.load(), lc=liveConf.load(), rc=refConf.load();
    const bool bypass=apvts.getRawParameterValue("bypass")->load()>0.5f;
    const bool exact=apvts.getRawParameterValue("exact")->load()>0.5f;
    const float strength=apvts.getRawParameterValue("match")->load()*0.01f;
    const float rangeCents=apvts.getRawParameterValue("range")->load()*100.0f;
    const float vibrato=exact?1.0f:apvts.getRawParameterValue("vibrato")->load()*0.01f;
    const float natural=apvts.getRawParameterValue("natural")->load()*0.01f;
    const float tol=apvts.getRawParameterValue("tolerance")->load();
    const float mix=apvts.getRawParameterValue("mix")->load()*0.01f;
    const float outGain=juce::Decibels::decibelsToGain(apvts.getRawParameterValue("output")->load());
    const float speedMs=exact?juce::jmin(22.0f,apvts.getRawParameterValue("speed")->load()):apvts.getRawParameterValue("speed")->load();
    const float smooth=std::exp(-1.0f/(juce::jmax(1.0f,speedMs*0.001f)*(float)sr));

    for(int i=0;i<n;++i)
    {
        float v=0.0f,r=0.0f;
        const int vc=voiceBus.getNumChannels(), rcnt=refBus.getNumChannels();
        if(vc>0){ v=voiceBus.getSample(0,i); if(vc>1) v=0.5f*(v+voiceBus.getSample(1,i)); }
        if(rcnt>0){ r=refBus.getSample(0,i); if(rcnt>1) r=0.5f*(r+refBus.getSample(1,i)); }
        liveDetector.push(v); refDetector.push(r);
        if(liveDetector.shouldAnalyse())
        {
            auto a=liveDetector.analyse(); auto b=refDetector.analyse();
            currentLive=a.first; lc=a.second; currentRef=b.first; rc=b.second;
            liveHz.store(currentLive); liveConf.store(lc); refHz.store(currentRef); refConf.store(rc);
        }

        float cents=0.0f;
        const bool voiced=currentLive>65.0f && currentRef>65.0f && lc>0.42f && rc>0.40f;
        if(voiced)
        {
            float target=nearestRefOctave(currentRef,currentLive);
            const float midi=69.0f+12.0f*std::log2(target/440.0f);
            const float snapped=440.0f*std::pow(2.0f,(std::round(midi)-69.0f)/12.0f);
            target=snapped*(1.0f-vibrato)+target*vibrato;
            cents=1200.0f*std::log2(target/currentLive);
            if(std::abs(cents)<tol) cents=0.0f;
            cents=juce::jlimit(-rangeCents,rangeCents,cents)*strength;
        }
        shiftCents.store(cents);
        const float desired=std::pow(2.0f,cents/1200.0f);
        ratioSmoothed=smooth*ratioSmoothed+(1.0f-smooth)*desired;
        const float conf=juce::jlimit(0.0f,1.0f,juce::jmin(lc,rc));
        const float voicedGate=voiced ? juce::jmap(natural,0.55f,0.98f)*juce::jlimit(0.0f,1.0f,(conf-0.35f)/0.45f) : 0.0f;

        for(int c=0;c<ch;++c)
        {
            const float x=voiceBus.getNumChannels()>c?voiceBus.getSample(c,i):voiceBus.getSample(0,i);
            float y=(c==0?shifterL:shifterR).process(x,ratioSmoothed,mix,voicedGate);
            if(bypass) y=x;
            outBus.setSample(c,i,y*outGain);
        }
    }
}

juce::AudioProcessorEditor* KGVocalMatchAudioProcessor::createEditor(){ return new KGVocalMatchAudioProcessorEditor(*this); }
void KGVocalMatchAudioProcessor::getStateInformation(juce::MemoryBlock& d){ auto s=apvts.copyState(); auto x=s.createXml(); copyXmlToBinary(*x,d); }
void KGVocalMatchAudioProcessor::setStateInformation(const void* d,int z){ auto x=getXmlFromBinary(d,z); if(x&&x->hasTagName(apvts.state.getType())) apvts.replaceState(juce::ValueTree::fromXml(*x)); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){ return new KGVocalMatchAudioProcessor(); }
