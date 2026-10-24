#include "PluginEditor.h"

KGVocalMatchAudioProcessorEditor::KGVocalMatchAudioProcessorEditor(KGVocalMatchAudioProcessor& pr)
 : AudioProcessorEditor(&pr), p(pr)
{
    setSize(1000,560);
    setResizable(true,true);
    setResizeLimits(800,448,1500,840);
    if(auto* c=getConstrainer()) c->setFixedAspectRatio(1000.0/560.0);

    for(size_t i=0;i<knobs.size();++i)
    {
        auto& k=knobs[i];
        k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,72,19);
        k.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff78dcff));
        k.setColour(juce::Slider::thumbColourId,juce::Colour(0xffffc86d));
        if(ids[i]=="output") k.setTextValueSuffix(" dB"); else k.setTextValueSuffix(" %");
        addAndMakeVisible(k);

        labels[i].setText(names[i],juce::dontSendNotification);
        labels[i].setJustificationType(juce::Justification::centred);
        labels[i].setColour(juce::Label::textColourId,juce::Colour(0xffeef5ff));
        labels[i].setFont(juce::FontOptions(12.0f,juce::Font::bold));
        addAndMakeVisible(labels[i]);

        at[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts,ids[i],k);
    }

    auto setMode=[this](bool exact)
    {
        if(auto* param=p.apvts.getParameter("exact"))
        {
            param->beginChangeGesture();
            param->setValueNotifyingHost(exact?1.0f:0.0f);
            param->endChangeGesture();
        }
    };

    octaveSafe.onClick=[setMode]{ setMode(false); };
    exactUnison.onClick=[setMode]{ setMode(true); };

    for(auto* b:{&octaveSafe,&exactUnison})
    {
        b->setClickingTogglesState(false);
        b->setColour(juce::TextButton::textColourOffId,juce::Colours::white);
        addAndMakeVisible(*b);
    }

    bypass.setColour(juce::ToggleButton::textColourId,juce::Colours::white);
    addAndMakeVisible(bypass);
    bypassAt=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,"bypass",bypass);

    startTimerHz(25);
}

KGVocalMatchAudioProcessorEditor::~KGVocalMatchAudioProcessorEditor(){ stopTimer(); }

juce::String KGVocalMatchAudioProcessorEditor::noteName(float hz)
{
    if(hz<20.0f) return "--";
    const float m=69.0f+12.0f*std::log2(hz/440.0f);
    const int n=(int)std::round(m);
    static const char* nn[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    return juce::String(nn[(n%12+12)%12])+juce::String(n/12-1);
}

void KGVocalMatchAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff040711));
    juce::ColourGradient bg(juce::Colour(0xff07152d),0,0,juce::Colour(0xff25102f),1000,560,false);
    bg.addColour(.55,juce::Colour(0xff0a1025));
    g.setGradientFill(bg);
    g.fillRect(getLocalBounds().toFloat());

    for(int i=0;i<90;++i)
    {
        const float x=(float)((i*151+29)%1000), y=(float)((i*73+19)%410);
        g.setColour((i%4)?juce::Colour(0x447fdfff):juce::Colour(0x55ffd06d));
        const float r=(i%11==0)?2.2f:1.0f;
        g.fillEllipse(x,y,r,r);
    }

    g.setColour(juce::Colour(0xffffedc2));
    g.setFont(juce::FontOptions(27.0f,juce::Font::bold));
    g.drawText("KG VOCAL MATCH",28,18,330,36,juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8ebbe0));
    g.setFont(juce::FontOptions(10.5f));
    g.drawText("REFERENCE 3/4  →  YOUR VOICE 1/2",31,54,360,17,juce::Justification::centredLeft);

    g.setColour(juce::Colour(0x7a12182c));
    g.fillRoundedRectangle(26,86,948,146,18);
    g.setColour(juce::Colour(0x807c5fb1));
    g.drawRoundedRectangle(26,86,948,146,18,1);

    const bool liveOK=lc>.42f, refOK=rc>.40f;

    g.setFont(juce::FontOptions(13.0f,juce::Font::bold));
    g.setColour(juce::Colour(0xff8bdcff));
    g.drawText("YOUR VOICE",58,103,220,22,juce::Justification::centred);
    g.setColour(juce::Colour(0xffffcc70));
    g.drawText("REFERENCE",722,103,220,22,juce::Justification::centred);

    g.setFont(juce::FontOptions(35.0f,juce::Font::bold));
    g.setColour(liveOK?juce::Colour(0xffeaf7ff):juce::Colour(0xff6b748a));
    g.drawText(noteName(live),58,127,220,52,juce::Justification::centred);
    g.setColour(refOK?juce::Colour(0xffffefc9):juce::Colour(0xff746d63));
    g.drawText(noteName(ref),722,127,220,52,juce::Justification::centred);

    const float absC=std::abs(cents);
    const bool largeShift=absC>250.0f;
    const bool veryLargeShift=absC>500.0f;

    g.setFont(juce::FontOptions(24.0f,juce::Font::bold));
    g.setColour(absC<15?juce::Colour(0xff79e3b0):(largeShift?juce::Colour(0xffff8c70):juce::Colour(0xffffd070)));
    g.drawText(juce::String(cents,1)+" cents",350,128,300,42,juce::Justification::centred);

    g.setFont(juce::FontOptions(11.0f,juce::Font::bold));
    if(!liveOK || !refOK)
    {
        g.setColour(juce::Colour(0xffffb477));
        g.drawText("WAITING FOR CLEAN PITCH",350,174,300,22,juce::Justification::centred);
    }
    else if(veryLargeShift)
    {
        g.setColour(juce::Colour(0xffff7777));
        g.drawText("VERY LARGE SHIFT • ARTIFACT RISK",350,174,300,22,juce::Justification::centred);
    }
    else if(largeShift)
    {
        g.setColour(juce::Colour(0xffffc76d));
        g.drawText("LARGE SHIFT • USE NATURAL",350,174,300,22,juce::Justification::centred);
    }
    else
    {
        g.setColour(juce::Colour(0xff78e2b1));
        g.drawText("NATURAL CORRECTION ZONE",350,174,300,22,juce::Justification::centred);
    }

    g.setFont(juce::FontOptions(10.0f));
    g.setColour(juce::Colour(0xff8e9aaf));
    g.drawText("confidence "+juce::String(lc,2),85,188,170,18,juce::Justification::centred);
    g.drawText("confidence "+juce::String(rc,2),745,188,170,18,juce::Justification::centred);

    g.setColour(juce::Colour(0xff8fa4c4));
    g.setFont(juce::FontOptions(10.5f));
    const juce::String modeText=exactMode
        ?"EXACT UNISON: absolute reference pitch"
        :"OCTAVE SAFE: same melody, your natural octave";
    g.drawText(modeText,320,248,360,20,juce::Justification::centred);

    g.setColour(juce::Colour(0xff677895));
    g.setFont(juce::FontOptions(9.5f));
    g.drawText("KG Music Records  |  KG Vocal Match VST3 v0.4",30,532,320,14,juce::Justification::centredLeft);
}

void KGVocalMatchAudioProcessorEditor::resized()
{
    const float s=(float)getWidth()/1000.0f;
    auto R=[s](int x,int y,int w,int h)
    {
        return juce::Rectangle<int>((int)(x*s),(int)(y*s),(int)(w*s),(int)(h*s));
    };

    octaveSafe.setBounds(R(290,270,190,38));
    exactUnison.setBounds(R(520,270,190,38));
    bypass.setBounds(R(830,24,110,28));

    const int xs[4]={115,315,515,715};
    for(int i=0;i<4;++i)
    {
        labels[(size_t)i].setBounds(R(xs[i],330,150,18));
        knobs[(size_t)i].setBounds(R(xs[i],350,150,145));
    }
}

void KGVocalMatchAudioProcessorEditor::timerCallback()
{
    live=p.getLiveHz();
    ref=p.getRefHz();
    lc=p.getLiveConfidence();
    rc=p.getRefConfidence();
    cents=p.getShiftCents();
    exactMode=p.apvts.getRawParameterValue("exact")->load()>0.5f;

    octaveSafe.setColour(juce::TextButton::buttonColourId,exactMode?juce::Colour(0xff17223c):juce::Colour(0xff157067));
    exactUnison.setColour(juce::TextButton::buttonColourId,exactMode?juce::Colour(0xff7a4f18):juce::Colour(0xff17223c));

    repaint();
}
