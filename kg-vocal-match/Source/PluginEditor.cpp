#include "PluginEditor.h"
KGVocalMatchAudioProcessorEditor::KGVocalMatchAudioProcessorEditor(KGVocalMatchAudioProcessor& pr):AudioProcessorEditor(&pr),p(pr)
{
    setSize(1000,580); setResizable(true,true); setResizeLimits(800,464,1500,870); if(auto*c=getConstrainer()) c->setFixedAspectRatio(1000.0/580.0);
    for(size_t i=0;i<knobs.size();++i){ auto&k=knobs[i]; k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,70,19); k.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xff77dcff)); k.setColour(juce::Slider::thumbColourId,juce::Colour(0xffffc96e)); if(ids[i]=="speed")k.setTextValueSuffix(" ms"); else if(ids[i]=="range")k.setTextValueSuffix(" st"); else if(ids[i]=="tolerance")k.setTextValueSuffix(" ct"); else if(ids[i]=="output")k.setTextValueSuffix(" dB"); else k.setTextValueSuffix(" %"); addAndMakeVisible(k); labels[i].setText(names[i],juce::dontSendNotification); labels[i].setJustificationType(juce::Justification::centred); labels[i].setColour(juce::Label::textColourId,juce::Colour(0xffe8efff)); labels[i].setFont(juce::FontOptions(12.0f,juce::Font::bold)); addAndMakeVisible(labels[i]); at[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts,ids[i],k); }
    for(auto*b:{&exact,&bypass}){ b->setClickingTogglesState(true); b->setColour(juce::ToggleButton::textColourId,juce::Colours::white); addAndMakeVisible(*b); }
    exactAt=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,"exact",exact); bypassAt=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.apvts,"bypass",bypass);
    startTimerHz(25);
}
KGVocalMatchAudioProcessorEditor::~KGVocalMatchAudioProcessorEditor(){ stopTimer(); }
juce::String KGVocalMatchAudioProcessorEditor::noteName(float hz)
{
    if(hz<20) return "--"; const float m=69.0f+12.0f*std::log2(hz/440.0f); const int n=(int)std::round(m); static const char* nn[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"}; return juce::String(nn[(n%12+12)%12])+juce::String(n/12-1);
}
void KGVocalMatchAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff040712)); juce::ColourGradient bg(juce::Colour(0xff06142c),0,0,juce::Colour(0xff25102f),1000,580,false); bg.addColour(.55,juce::Colour(0xff0b1025)); g.setGradientFill(bg); g.fillRect(getLocalBounds().toFloat());
    for(int i=0;i<95;++i){ float x=(float)((i*151+29)%1000),y=(float)((i*73+19)%430); g.setColour((i%4)?juce::Colour(0x447fdfff):juce::Colour(0x55ffd06d)); float r=(i%11==0)?2.2f:1.0f; g.fillEllipse(x,y,r,r); }
    g.setColour(juce::Colour(0xffffedc2)); g.setFont(juce::FontOptions(27.0f,juce::Font::bold)); g.drawText("KG VOCAL MATCH",28,18,330,36,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff8ebbe0)); g.setFont(juce::FontOptions(10.5f)); g.drawText("REFERENCE 3/4  →  YOUR VOICE 1/2  |  PITCH MATCH + OCTAVE PROTECTION",31,54,570,17,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0x7a12182c)); g.fillRoundedRectangle(26,86,948,142,18); g.setColour(juce::Colour(0x807c5fb1)); g.drawRoundedRectangle(26,86,948,142,18,1);
    const bool liveOK=lc>.42f, refOK=rc>.40f;
    g.setFont(juce::FontOptions(13.0f,juce::Font::bold)); g.setColour(juce::Colour(0xff8bdcff)); g.drawText("YOUR VOICE",58,103,220,22,juce::Justification::centred);
    g.setColour(juce::Colour(0xffffcc70)); g.drawText("REFERENCE",722,103,220,22,juce::Justification::centred);
    g.setFont(juce::FontOptions(35.0f,juce::Font::bold)); g.setColour(liveOK?juce::Colour(0xffeaf7ff):juce::Colour(0xff6b748a)); g.drawText(noteName(live),58,127,220,52,juce::Justification::centred);
    g.setColour(refOK?juce::Colour(0xffffefc9):juce::Colour(0xff746d63)); g.drawText(noteName(ref),722,127,220,52,juce::Justification::centred);
    g.setFont(juce::FontOptions(24.0f,juce::Font::bold)); g.setColour(std::abs(cents)<15?juce::Colour(0xff79e3b0):juce::Colour(0xffff927a)); g.drawText(juce::String(cents,1)+" cents",360,132,280,40,juce::Justification::centred);
    g.setFont(juce::FontOptions(10.0f)); g.setColour(juce::Colour(0xff8e9aaf)); g.drawText("confidence "+juce::String(lc,2),85,183,170,18,juce::Justification::centred); g.drawText("confidence "+juce::String(rc,2),745,183,170,18,juce::Justification::centred);
    g.setColour(juce::Colour(0xff677895)); g.setFont(juce::FontOptions(9.5f)); g.drawText("KG Music Records  |  KG Vocal Match VST3 v0.3",30,552,310,14,juce::Justification::centredLeft);
}
void KGVocalMatchAudioProcessorEditor::resized()
{
    float s=(float)getWidth()/1000.0f; auto R=[s](int x,int y,int w,int h){return juce::Rectangle<int>((int)(x*s),(int)(y*s),(int)(w*s),(int)(h*s));};
    const int xs[4]={65,285,505,725};
    for(int i=0;i<8;++i){ int row=i/4,col=i%4,y=row?405:255; labels[(size_t)i].setBounds(R(xs[col],y-18,150,18)); knobs[(size_t)i].setBounds(R(xs[col],y,150,130)); }
    exact.setBounds(R(385,205,100,28)); bypass.setBounds(R(515,205,100,28));
}
void KGVocalMatchAudioProcessorEditor::timerCallback(){ live=p.getLiveHz(); ref=p.getRefHz(); lc=p.getLiveConfidence(); rc=p.getRefConfidence(); cents=p.getShiftCents(); repaint(); }
