#include "PluginEditor.h"
KGSketchPianoAudioProcessorEditor::KGSketchPianoAudioProcessorEditor(KGSketchPianoAudioProcessor& pr)
 : AudioProcessorEditor(&pr),p(pr),keyboard(pr.keyboardState,juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setSize(1000,610); setResizable(true,true); setResizeLimits(800,488,1500,915); if(auto*c=getConstrainer()) c->setFixedAspectRatio(1000.0/610.0);
    mode.addItemList({"GRAND","SOFT","LOFI","PAD"},1); addAndMakeVisible(mode); modeAt=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts,"mode",mode);
    for(size_t i=0;i<knobs.size();++i){ auto&k=knobs[i]; k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag); k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,62,18); k.setColour(juce::Slider::rotarySliderFillColourId,juce::Colour(0xffffc85b)); k.setColour(juce::Slider::thumbColourId,juce::Colour(0xff77d8ff)); if(ids[i]=="output") k.setTextValueSuffix(" dB"); else k.setTextValueSuffix(" %"); addAndMakeVisible(k); labels[i].setText(names[i],juce::dontSendNotification); labels[i].setJustificationType(juce::Justification::centred); labels[i].setColour(juce::Label::textColourId,juce::Colour(0xffe8eef8)); labels[i].setFont(juce::FontOptions(11.5f,juce::Font::bold)); addAndMakeVisible(labels[i]); at[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.apvts,ids[i],k); }
    keyboard.setAvailableRange(24,96); keyboard.setKeyWidth(18.0f); keyboard.setScrollButtonsVisible(false); addAndMakeVisible(keyboard);
}
void KGSketchPianoAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff050813));
    juce::ColourGradient gr(juce::Colour(0xff081126),0,0,juce::Colour(0xff25112b),1000,610,false); gr.addColour(.58,juce::Colour(0xff0a1830)); g.setGradientFill(gr); g.fillRect(getLocalBounds().toFloat());
    for(int i=0;i<90;++i){ float x=(float)((i*137+31)%1000), y=(float)((i*83+17)%430); g.setColour((i%3)?juce::Colour(0x557dc8ff):juce::Colour(0x66ffd36f)); float rr=(i%9==0)?2.2f:1.1f; g.fillEllipse(x,y,rr,rr); }
    g.setColour(juce::Colour(0xffffecbd)); g.setFont(juce::FontOptions(28.0f,juce::Font::bold)); g.drawText("KG SKETCH PIANO",28,18,340,38,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff8fb6d8)); g.setFont(juce::FontOptions(11.0f)); g.drawText("32 VOICES  •  ARTURIA / MIDI  •  CC64 SUSTAIN  •  PITCH BEND",31,55,460,18,juce::Justification::centredLeft);
    g.setColour(juce::Colour(0x70141c31)); g.fillRoundedRectangle(24,88,952,364,18); g.setColour(juce::Colour(0x806c5b9a)); g.drawRoundedRectangle(24,88,952,364,18,1);
    g.setColour(juce::Colour(0xffa2acc4)); g.setFont(juce::FontOptions(9.5f)); g.drawText("KG Music Records  |  KG Sketch Piano VST3 v0.3",30,584,300,14,juce::Justification::centredLeft);
}
void KGSketchPianoAudioProcessorEditor::resized()
{
    float s=(float)getWidth()/1000.0f; auto R=[s](int x,int y,int w,int h){return juce::Rectangle<int>((int)(x*s),(int)(y*s),(int)(w*s),(int)(h*s));};
    mode.setBounds(R(790,25,170,28));
    const int xs[6]={45,195,345,495,645,795};
    for(int i=0;i<12;++i){ int row=i/6,col=i%6; int y=row?285:115; labels[(size_t)i].setBounds(R(xs[col],y-18,120,18)); knobs[(size_t)i].setBounds(R(xs[col],y,120,140)); }
    keyboard.setBounds(R(30,470,940,100));
}
