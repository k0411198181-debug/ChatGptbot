#include "PluginEditor.h"

KGHumanEngineLookAndFeel::KGHumanEngineLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff7f4ea));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff10161a));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x0037465f));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff2f5f0));

    setColour(juce::ComboBox::backgroundColourId, juce::Colour(0xff10161a));
    setColour(juce::ComboBox::textColourId, juce::Colour(0xffffdd83));
    setColour(juce::ComboBox::outlineColourId, juce::Colour(0xff4d6f59));
    setColour(juce::ComboBox::arrowColourId, juce::Colour(0xffffdd83));

    setColour(juce::PopupMenu::backgroundColourId, juce::Colour(0xff0b1110));
    setColour(juce::PopupMenu::textColourId, juce::Colour(0xffedf4ef));
    setColour(juce::PopupMenu::highlightedBackgroundColourId, juce::Colour(0xff315a49));
    setColour(juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void KGHumanEngineLookAndFeel::drawRotarySlider(juce::Graphics& g,int x,int y,int width,int height,
                                                float sliderPos,float rotaryStartAngle,float rotaryEndAngle,
                                                juce::Slider& slider)
{
    const float diameter=(float)juce::jmin(width,height)-16.0f;
    const float radius=diameter*0.5f;
    const float cx=x+width*0.5f;
    const float cy=y+height*0.5f;
    const float angle=rotaryStartAngle+sliderPos*(rotaryEndAngle-rotaryStartAngle);

    const auto name=slider.getName();
    const bool hero=name=="HUMAN";

    juce::Colour c1=juce::Colour(0xff62e6a7);
    juce::Colour c2=juce::Colour(0xffffc76a);

    if (name=="ATTACK" || name=="FEEL")
    {
        c1=juce::Colour(0xff6bd7ff);
        c2=juce::Colour(0xff6df0c4);
    }
    else if (name=="DRIFT" || name=="MOTION" || name=="WIDTH")
    {
        c1=juce::Colour(0xff7d8cff);
        c2=juce::Colour(0xffd071ff);
    }
    else if (name=="COLOR")
    {
        c1=juce::Colour(0xffffb75d);
        c2=juce::Colour(0xffff7b70);
    }

    if (hero)
    {
        juce::ColourGradient glow(juce::Colour(0x806be9a9),cx,cy,
                                  juce::Colour(0x006be9a9),cx+radius*1.8f,cy,true);
        g.setGradientFill(glow);
        g.fillEllipse(cx-radius*1.55f,cy-radius*1.55f,radius*3.10f,radius*3.10f);
    }

    g.setColour(hero?juce::Colour(0x946fe0ab):juce::Colour(0x70546b63));
    g.drawEllipse(cx-radius-5.0f,cy-radius-5.0f,diameter+10.0f,diameter+10.0f,
                  hero?2.4f:1.2f);

    juce::ColourGradient face(juce::Colour(0xff303a38),cx-radius*0.55f,cy-radius*0.75f,
                              juce::Colour(0xff090d0c),cx+radius*0.75f,cy+radius*0.85f,false);
    face.addColour(0.45,juce::Colour(0xff18211f));
    g.setGradientFill(face);
    g.fillEllipse(cx-radius,cy-radius,diameter,diameter);

    juce::Path bgArc;
    bgArc.addCentredArc(cx,cy,radius-4.0f,radius-4.0f,0.0f,
                        rotaryStartAngle,rotaryEndAngle,true);
    g.setColour(juce::Colour(0xff293932));
    g.strokePath(bgArc,juce::PathStrokeType(hero?5.8f:4.2f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(cx,cy,radius-4.0f,radius-4.0f,0.0f,
                           rotaryStartAngle,angle,true);
    juce::ColourGradient arc(c1,cx-radius,cy,c2,cx+radius,cy,false);
    g.setGradientFill(arc);
    g.strokePath(valueArc,juce::PathStrokeType(hero?6.2f:4.5f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    const float now=(float)(juce::Time::getMillisecondCounterHiRes()*0.001);
    const int ledCount=hero?35:25;
    const float ledR=radius+(hero?9.5f:7.0f);
    const float dot=hero?4.4f:3.2f;

    for (int i=0;i<ledCount;++i)
    {
        const float t=(float)i/(float)(ledCount-1);
        const float a=rotaryStartAngle+t*(rotaryEndAngle-rotaryStartAngle);
        const float lx=cx+std::sin(a)*ledR;
        const float ly=cy-std::cos(a)*ledR;
        const bool on=t<=sliderPos+0.002f;
        const float dist=std::abs(t-sliderPos);
        const float nearPointer=juce::jlimit(0.0f,1.0f,1.0f-dist*15.0f);
        const float pulse=0.66f+0.34f*std::sin(now*5.2f-(float)i*0.37f);

        if (on)
        {
            const auto led=c1.interpolatedWith(c2,t);
            g.setColour(led.withAlpha(0.16f+0.46f*nearPointer*pulse));
            g.fillEllipse(lx-dot,ly-dot,dot*2.0f,dot*2.0f);
            g.setColour(led.withAlpha(0.74f+0.26f*nearPointer*pulse));
            g.fillEllipse(lx-dot*0.48f,ly-dot*0.48f,dot*0.96f,dot*0.96f);
        }
        else
        {
            g.setColour(juce::Colour(0x44394a43));
            g.fillEllipse(lx-dot*0.34f,ly-dot*0.34f,dot*0.68f,dot*0.68f);
        }
    }

    const float midAngle=rotaryStartAngle+0.5f*(rotaryEndAngle-rotaryStartAngle);
    const float mx=cx+std::sin(midAngle)*ledR;
    const float my=cy-std::cos(midAngle)*ledR;
    g.setColour(juce::Colour(0xfff7fff9));
    g.fillEllipse(mx-2.2f,my-2.2f,4.4f,4.4f);

    juce::Path pointer;
    const float pointerLength=radius*0.66f;
    const float pointerThickness=hero?4.4f:3.2f;
    pointer.addRoundedRectangle(-pointerThickness*0.5f,-pointerLength,
                                pointerThickness,pointerLength*0.57f,2.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(cx,cy));
    g.setColour(juce::Colour(0xfff8fbff));
    g.fillPath(pointer);
}

void KGHumanEngineLookAndFeel::drawToggleButton(juce::Graphics& g,juce::ToggleButton& b,
                                                bool over,bool down)
{
    auto r=b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on=b.getToggleState();

    auto fill=juce::Colour(0xff111817);
    if (on)
    {
        if (b.getButtonText()=="AUTO") fill=juce::Colour(0xff245f4a);
        else if (b.getButtonText()=="MAX") fill=juce::Colour(0xff8a551d);
        else if (b.getButtonText()=="DELTA") fill=juce::Colour(0xff57529c);
        else if (b.getButtonText()=="BYPASS") fill=juce::Colour(0xff6b2938);
    }

    if (over) fill=fill.brighter(0.10f);
    if (down) fill=fill.darker(0.12f);

    g.setColour(fill);
    g.fillRoundedRectangle(r,7.0f);
    g.setColour(on?juce::Colour(0xffffdc86):juce::Colour(0xff3b5148));
    g.drawRoundedRectangle(r,7.0f,on?1.8f:1.0f);
    g.setColour(on?juce::Colours::white:juce::Colour(0xffafc3ba));
    g.setFont(juce::FontOptions(juce::jlimit(10.0f,15.0f,b.getHeight()*0.36f),juce::Font::bold));
    g.drawFittedText(b.getButtonText(),b.getLocalBounds(),juce::Justification::centred,1);
}

KGHumanEngineAudioProcessorEditor::KGHumanEngineAudioProcessorEditor(KGHumanEngineAudioProcessor& p)
    : AudioProcessorEditor(&p),processor(p)
{
    setLookAndFeel(&lnf);
    setSize(1024,600);
    setResizable(true,true);
    setResizeLimits(780,457,1536,900);

    if (auto* constrainer=getConstrainer())
        constrainer->setFixedAspectRatio(1024.0/600.0);

    for (size_t i=0;i<knobs.size();++i)
    {
        configureKnob(knobs[i],labels[i],names[i],i==0);
        knobs[i].setName(names[i]);
        attachments[i]=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts,ids[i],knobs[i]);
    }

    configureKnob(mixKnob,mixLabel,"MIX");
    configureKnob(outputKnob,outputLabel,"OUTPUT");
    mixKnob.setTextValueSuffix(" %");
    outputKnob.setTextValueSuffix(" dB");

    mixAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts,"mix",mixKnob);
    outputAttachment=std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts,"output",outputKnob);

    modeBox.addItem("MELODIC",1);
    modeBox.addItem("RHYTHMIC",2);
    modeBox.addItem("ATMOSPHERE",3);
    modeBox.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(modeBox);

    modeAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts,"mode",modeBox);

    for (auto* b:{&autoButton,&maxButton,&deltaButton,&bypassButton})
    {
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }

    addAndMakeVisible(goldenButton);
    goldenButton.setColour(juce::TextButton::buttonColourId,juce::Colour(0xff3f5429));
    goldenButton.setColour(juce::TextButton::textColourOffId,juce::Colour(0xffffdd86));

    autoAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts,"auto",autoButton);
    maxAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts,"max",maxButton);
    deltaAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts,"delta",deltaButton);
    bypassAttachment=std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts,"bypass",bypassButton);

    const auto styleTopButton=[this](juce::TextButton& b,bool gold)
    {
        b.setColour(juce::TextButton::buttonColourId,
                    gold?juce::Colour(0xff3d492b):juce::Colour(0xff111817));
        b.setColour(juce::TextButton::buttonOnColourId,
                    gold?juce::Colour(0xff5f6531):juce::Colour(0xff274538));
        b.setColour(juce::TextButton::textColourOffId,
                    gold?juce::Colour(0xffffdc83):juce::Colour(0xffcde0d5));
        addAndMakeVisible(b);
    };

    styleTopButton(prevPresetButton,false);
    styleTopButton(presetButton,true);
    styleTopButton(nextPresetButton,false);
    styleTopButton(saveButton,false);
    styleTopButton(abButton,false);
    styleTopButton(resetButton,false);
    styleTopButton(menuButton,false);

    autoButton.onClick=[this]
    {
        if (autoButton.getToggleState())
            applyGoldenSettings();
    };

    goldenButton.onClick=[this]{applyGoldenSettings();};
    prevPresetButton.onClick=[this]{stepPreset(-1);};
    nextPresetButton.onClick=[this]{stepPreset(1);};
    presetButton.onClick=[this]{showPresetMenu();};

    saveButton.onClick=[this]
    {
        captureState(userSnapshot);
        updatePresetLabel("USER");
    };

    abButton.onClick=[this]
    {
        if (activeA)
        {
            captureState(stateA);
            restoreState(stateB);
            activeA=false;
            abButton.setButtonText("A/B : B");
        }
        else
        {
            captureState(stateB);
            restoreState(stateA);
            activeA=true;
            abButton.setButtonText("A/B : A");
        }
    };

    resetButton.onClick=[this]{applyGoldenSettings();};
    menuButton.onClick=[this]{showMainMenu();};

    captureState(stateA);
    stateB=stateA;

    startTimerHz(30);
}

KGHumanEngineAudioProcessorEditor::~KGHumanEngineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void KGHumanEngineAudioProcessorEditor::configureKnob(juce::Slider& k,juce::Label& label,
                                                      const juce::String& name,bool hero)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow,false,hero?76:64,19);
    k.setDoubleClickReturnValue(true,50.0);
    k.setPopupDisplayEnabled(false,false,this);
    addAndMakeVisible(k);

    label.setText(name,juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId,
                    hero?juce::Colour(0xffffdf85):juce::Colour(0xffd8e8df));
    label.setFont(juce::FontOptions(hero?15.2f:13.0f,juce::Font::bold));
    addAndMakeVisible(label);
}

void KGHumanEngineAudioProcessorEditor::setParameter(const juce::String& id,float plainValue)
{
    if (auto* p=processor.apvts.getParameter(id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(plainValue));
        p->endChangeGesture();
    }
}

void KGHumanEngineAudioProcessorEditor::applyPreset(int index)
{
    currentPreset=juce::jlimit(0,presetNames.size()-1,index);

    struct Preset
    {
        float human,feel,attack,flow,drift,color,motion,width;
        float mix,output,mode;
        bool autoMode,maxMode,deltaMode;
    };

    static const std::array<Preset,6> presets{{
        {55,48,46,52,34,45,38,48,100,0,0,true,false,false},
        {62,55,42,58,38,52,42,50,100,0,0,true,false,false},
        {54,46,66,38,28,42,30,44,100,0,1,true,false,false},
        {58,50,28,72,48,48,64,66,100,0,2,true,false,false},
        {36,32,34,38,20,34,24,42,100,0,0,true,false,false},
        {100,72,58,66,54,58,56,54,100,-0.3f,0,true,true,false}
    }};

    const auto& v=presets[(size_t)currentPreset];

    setParameter("mode",v.mode);
    setParameter("human",v.human);
    setParameter("feel",v.feel);
    setParameter("attack",v.attack);
    setParameter("flow",v.flow);
    setParameter("drift",v.drift);
    setParameter("color",v.color);
    setParameter("motion",v.motion);
    setParameter("width",v.width);
    setParameter("mix",v.mix);
    setParameter("output",v.output);
    setParameter("auto",v.autoMode?1.0f:0.0f);
    setParameter("max",v.maxMode?1.0f:0.0f);
    setParameter("delta",v.deltaMode?1.0f:0.0f);

    updatePresetLabel(presetNames[currentPreset]);
}

void KGHumanEngineAudioProcessorEditor::applyGoldenSettings()
{
    const int mode=(int)processor.apvts.getRawParameterValue("mode")->load();
    applyPreset(mode==1?2:(mode==2?3:0));
}

void KGHumanEngineAudioProcessorEditor::stepPreset(int delta)
{
    int next=currentPreset+delta;
    if (next<0) next=presetNames.size()-1;
    if (next>=presetNames.size()) next=0;
    applyPreset(next);
}

void KGHumanEngineAudioProcessorEditor::showPresetMenu()
{
    juce::PopupMenu menu;

    for (int i=0;i<presetNames.size();++i)
        menu.addItem(i+1,presetNames[i],true,i==currentPreset);

    if (userSnapshot.getSize()>0)
    {
        menu.addSeparator();
        menu.addItem(100,"USER SNAPSHOT");
    }

    juce::Component::SafePointer<KGHumanEngineAudioProcessorEditor> safeThis(this);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&presetButton),
        [safeThis](int result)
        {
            if (safeThis==nullptr || result==0) return;

            if (result>=1 && result<=safeThis->presetNames.size())
                safeThis->applyPreset(result-1);
            else if (result==100)
            {
                safeThis->restoreState(safeThis->userSnapshot);
                safeThis->updatePresetLabel("USER");
            }
        });
}

void KGHumanEngineAudioProcessorEditor::showMainMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1,"Golden for current mode");
    menu.addItem(2,"Load USER snapshot",userSnapshot.getSize()>0);
    menu.addSeparator();
    menu.addItem(3,"About KG Human Engine v0.2");

    juce::Component::SafePointer<KGHumanEngineAudioProcessorEditor> safeThis(this);

    menu.showMenuAsync(
        juce::PopupMenu::Options().withTargetComponent(&menuButton),
        [safeThis](int result)
        {
            if (safeThis==nullptr) return;

            if (result==1)
                safeThis->applyGoldenSettings();
            else if (result==2 && safeThis->userSnapshot.getSize()>0)
            {
                safeThis->restoreState(safeThis->userSnapshot);
                safeThis->updatePresetLabel("USER");
            }
            else if (result==3)
            {
                juce::AlertWindow::showMessageBoxAsync(
                    juce::MessageBoxIconType::InfoIcon,
                    "KG Human Engine v0.2",
                    "Galaxy Signal Records\n\n"
                    "MELODIC = natural musical phrasing and gentle color.\n"
                    "RHYTHMIC = stronger articulation and pulse.\n"
                    "ATMOSPHERE = slow flow, width and stereo breathing.\n"
                    "FULL PERFORMANCE = HUMAN 100 with broader musical variation.\n\n"
                    "DELTA lets you hear only what Human Engine changes.");
            }
        });
}

void KGHumanEngineAudioProcessorEditor::captureState(juce::MemoryBlock& target)
{
    target.reset();
    processor.getStateInformation(target);
}

void KGHumanEngineAudioProcessorEditor::restoreState(const juce::MemoryBlock& source)
{
    if (source.getSize()>0)
        processor.setStateInformation(source.getData(),(int)source.getSize());
}

void KGHumanEngineAudioProcessorEditor::updatePresetLabel(const juce::String& name)
{
    presetButton.setButtonText(name);
}

void KGHumanEngineAudioProcessorEditor::updateSpectrum()
{
    const int pulled=processor.pullVisualizationSamples(
        visualPullBuffer.data(),(int)visualPullBuffer.size());

    bool transformed=false;

    for (int i=0;i<pulled;++i)
    {
        fftInput[(size_t)fftInputPos++]=visualPullBuffer[(size_t)i];

        if (fftInputPos>=fftSize)
        {
            std::fill(fftData.begin(),fftData.end(),0.0f);
            std::copy(fftInput.begin(),fftInput.end(),fftData.begin());

            fftWindow.multiplyWithWindowingTable(fftData.data(),fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

            const double sr=processor.getSampleRate()>1000.0?processor.getSampleRate():44100.0;
            const double lowHz=35.0;
            const double highHz=juce::jmin(18000.0,sr*0.45);

            for (int band=0;band<spectrumBands;++band)
            {
                const double t=((double)band+0.5)/(double)spectrumBands;
                const double freq=lowHz*std::pow(highHz/lowHz,t);
                const int centre=juce::jlimit(
                    1,fftSize/2-2,(int)std::round(freq*(double)fftSize/sr));

                float mag=0.0f;
                for (int k=-1;k<=1;++k)
                    mag+=fftData[(size_t)(centre+k)];
                mag/=3.0f;

                const float db=juce::Decibels::gainToDecibels(mag/(float)fftSize,-96.0f);
                const float normal=juce::jmap(
                    juce::jlimit(-82.0f,-8.0f,db),-82.0f,-8.0f,0.0f,1.0f);

                spectrumValues[(size_t)band]=
                    juce::jmax(normal,spectrumValues[(size_t)band]*0.84f);
            }

            fftInputPos=0;
            transformed=true;
        }
    }

    if (!transformed)
        for (auto& v:spectrumValues) v*=0.975f;

    const auto smoothMeter=[](float current,float target)
    {
        return target>current?0.58f*current+0.42f*target:current*0.90f;
    };

    smoothInL=smoothMeter(smoothInL,processor.getInputMeter(0));
    smoothInR=smoothMeter(smoothInR,processor.getInputMeter(1));
    smoothOutL=smoothMeter(smoothOutL,processor.getOutputMeter(0));
    smoothOutR=smoothMeter(smoothOutR,processor.getOutputMeter(1));
}

void KGHumanEngineAudioProcessorEditor::drawSpectrumAndMeters(juce::Graphics& g)
{
    const juce::Rectangle<float> scope(270.0f,484.0f,484.0f,75.0f);

    g.setColour(juce::Colour(0x5a0b1210));
    g.fillRoundedRectangle(scope,12.0f);
    g.setColour(juce::Colour(0x77668454));
    g.drawRoundedRectangle(scope,12.0f,1.0f);

    const float gap=2.0f;
    const float bandW=(scope.getWidth()-18.0f-gap*(spectrumBands-1))/(float)spectrumBands;
    const float baseY=scope.getBottom()-9.0f;

    juce::Path line;

    for (int i=0;i<spectrumBands;++i)
    {
        const float v=juce::jlimit(0.0f,1.0f,spectrumValues[(size_t)i]);
        const float x=scope.getX()+9.0f+(bandW+gap)*(float)i;
        const float h=2.0f+v*44.0f;
        const float y=baseY-h;

        juce::ColourGradient bar(juce::Colour(0xff61dfa6),x,baseY,
                                 juce::Colour(0xffffc568),x,y,false);
        g.setGradientFill(bar);
        g.fillRoundedRectangle(x,y,juce::jmax(1.0f,bandW),h,1.0f);

        const float px=x+bandW*0.5f;
        const float py=y-2.0f;
        if (i==0) line.startNewSubPath(px,py);
        else line.lineTo(px,py);
    }

    g.setColour(juce::Colour(0x8065e5ae));
    g.strokePath(line,juce::PathStrokeType(4.5f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xfffff0c4));
    g.strokePath(line,juce::PathStrokeType(1.1f,
                                           juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xffb9a877));
    g.setFont(juce::FontOptions(9.0f,juce::Font::bold));
    g.drawText("HUMAN MOTION SPECTRUM",390,486,244,12,juce::Justification::centred);

    const auto meter=[&g](float x,float y,float w,float l,float r,const juce::String& label)
    {
        g.setColour(juce::Colour(0xff96aa9d));
        g.setFont(juce::FontOptions(8.2f,juce::Font::bold));
        g.drawText(label,(int)x,(int)(y-13.0f),(int)w,10,juce::Justification::centred);

        const int segments=13;
        const float gap=1.5f;
        const float segW=(w-gap*(segments-1))/(float)segments;

        for (int row=0;row<2;++row)
        {
            const float value=juce::jlimit(0.0f,1.0f,std::sqrt(row==0?l:r));
            const int lit=juce::roundToInt(value*(float)segments);

            for (int i=0;i<segments;++i)
            {
                const float sx=x+(segW+gap)*(float)i;
                const bool on=i<lit;
                g.setColour(on
                    ?juce::Colour(0xff63d9b1).interpolatedWith(
                        juce::Colour(0xffffc768),(float)i/(float)(segments-1))
                    :juce::Colour(0x26324050));
                g.fillRoundedRectangle(sx,y+row*8.0f,segW,4.5f,1.0f);
            }
        }
    };

    meter(282.0f,510.0f,72.0f,smoothInL,smoothInR,"IN");
    meter(670.0f,510.0f,72.0f,smoothOutL,smoothOutR,"OUT");
}

void KGHumanEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff040807));

    const float scale=(float)getWidth()/1024.0f;
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(scale));

    juce::ColourGradient bg(juce::Colour(0xff040807),0.0f,0.0f,
                            juce::Colour(0xff141b18),1024.0f,600.0f,false);
    bg.addColour(0.42,juce::Colour(0xff071411));
    g.setGradientFill(bg);
    g.fillRect(0.0f,0.0f,1024.0f,600.0f);

    const float energy=juce::jlimit(0.0f,1.0f,std::sqrt(juce::jmax(smoothOutL,smoothOutR)));
    const float pulse=0.72f
                    +0.16f*std::sin(juce::MathConstants<float>::twoPi*animationPhase)
                    +0.20f*energy;

    juce::ColourGradient nebula(
        juce::Colour::fromFloatRGBA(0.12f,0.58f,0.44f,0.28f*pulse),
        410.0f,235.0f,
        juce::Colour::fromFloatRGBA(0.04f,0.25f,0.18f,0.0f),
        830.0f,260.0f,true);
    nebula.addColour(0.46,
        juce::Colour::fromFloatRGBA(0.58f,0.46f,0.18f,0.22f*pulse));
    g.setGradientFill(nebula);
    g.fillEllipse(80.0f,28.0f,800.0f,450.0f);

    juce::ColourGradient warmGlow(
        juce::Colour::fromFloatRGBA(1.0f,0.62f,0.20f,0.18f*pulse),
        600.0f,250.0f,
        juce::Colour::fromFloatRGBA(0.35f,0.14f,0.02f,0.0f),
        930.0f,290.0f,true);
    g.setGradientFill(warmGlow);
    g.fillEllipse(350.0f,76.0f,660.0f,385.0f);

    // Organic planet.
    juce::ColourGradient planet(
        juce::Colour(0xffd6e8ce),878.0f,70.0f,
        juce::Colour(0xff3c4b42),930.0f,128.0f,true);
    planet.addColour(0.42,juce::Colour(0xff789b83));
    planet.addColour(0.66,juce::Colour(0xffb89b61));
    g.setGradientFill(planet);
    g.fillEllipse(854.0f,47.0f,92.0f,92.0f);
    g.setColour(juce::Colour(0x66ffffff));
    g.drawEllipse(854.0f,47.0f,92.0f,92.0f,1.0f);

    for (int i=0;i<112;++i)
    {
        const int sx=(i*149+41)%1024;
        const int sy=(i*91+19)%470;
        const float twinkle=0.5f+0.5f*std::sin(
            juce::MathConstants<float>::twoPi*(animationPhase+0.061f*(float)i));
        const float a=0.10f+0.70f*twinkle;
        const float rr=(i%13==0)?2.2f:((i%5==0)?1.45f:0.78f);

        g.setColour((i%4==0)
            ?juce::Colour::fromFloatRGBA(1.0f,0.76f,0.36f,a)
            :juce::Colour::fromFloatRGBA(0.48f,0.88f,0.68f,a));

        g.fillEllipse((float)sx,(float)sy,rr,rr);
    }

    g.setColour(juce::Colour(0x9a0c1210));
    g.fillRoundedRectangle(286.0f,15.0f,536.0f,48.0f,10.0f);
    g.setColour(juce::Colour(0x92706740));
    g.drawRoundedRectangle(286.0f,15.0f,536.0f,48.0f,10.0f,1.0f);

    g.setColour(juce::Colour(0x72111618));
    g.fillRoundedRectangle(22.0f,92.0f,758.0f,370.0f,18.0f);
    g.setColour(juce::Colour(0x7a546d5f));
    g.drawRoundedRectangle(22.0f,92.0f,758.0f,370.0f,18.0f,1.0f);

    g.setColour(juce::Colour(0x65131619));
    g.fillRoundedRectangle(792.0f,158.0f,208.0f,304.0f,18.0f);
    g.setColour(juce::Colour(0x80706040));
    g.drawRoundedRectangle(792.0f,158.0f,208.0f,304.0f,18.0f,1.0f);

    g.setColour(juce::Colour(0xfff4f8f5));
    g.setFont(juce::FontOptions(25.5f,juce::Font::bold));
    g.drawText("KG HUMAN ENGINE",30,18,248,34,juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffffd57a));
    g.setFont(juce::FontOptions(10.4f,juce::Font::bold));
    g.drawText("GALAXY SIGNAL RECORDS",33,52,245,16,juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8ca89a));
    g.setFont(juce::FontOptions(9.8f));
    g.drawText("HUMAN  |  FEEL  |  ATTACK  |  FLOW  |  DRIFT  |  COLOR  |  MOTION  |  WIDTH",
               305,68,480,15,juce::Justification::centred);

    g.setColour(juce::Colour(0xffffd57a));
    g.setFont(juce::FontOptions(10.2f,juce::Font::bold));
    g.drawText("MODE",806,164,180,13,juce::Justification::centred);

    g.setColour(juce::Colour(0xff93a99d));
    g.setFont(juce::FontOptions(9.2f));
    g.drawText("MIX",804,374,84,14,juce::Justification::centred);
    g.drawText("OUTPUT",900,374,84,14,juce::Justification::centred);

    drawSpectrumAndMeters(g);

    g.setColour(juce::Colour(0xff8a8063));
    g.setFont(juce::FontOptions(9.2f));
    g.drawText("MELODIC = SOUL   |   RHYTHMIC = PULSE   |   ATMOSPHERE = FLOW   |   DELTA = CHANGES ONLY",
               185,566,655,16,juce::Justification::centred);

    g.setColour(juce::Colour(0xff64786e));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("KG Human Engine VST3  v0.2",32,574,205,14,juce::Justification::centredLeft);
}

void KGHumanEngineAudioProcessorEditor::resized()
{
    const float s=(float)getWidth()/1024.0f;

    const auto R=[s](float x,float y,float w,float h)
    {
        return juce::Rectangle<int>(
            juce::roundToInt(x*s),juce::roundToInt(y*s),
            juce::roundToInt(w*s),juce::roundToInt(h*s));
    };

    const std::array<juce::Rectangle<float>,8> knobRects{
        juce::Rectangle<float>(40,112,154,174),
        juce::Rectangle<float>(212,132,126,150),
        juce::Rectangle<float>(360,132,126,150),
        juce::Rectangle<float>(508,132,126,150),
        juce::Rectangle<float>(68,292,126,148),
        juce::Rectangle<float>(226,292,126,148),
        juce::Rectangle<float>(384,292,126,148),
        juce::Rectangle<float>(542,292,126,148)
    };

    for (int i=0;i<8;++i)
    {
        const bool hero=i==0;
        const auto r=knobRects[(size_t)i];

        labels[(size_t)i].setBounds(
            R(r.getX(),r.getY()-18.0f,r.getWidth(),20.0f));
        labels[(size_t)i].setFont(
            juce::FontOptions((hero?15.2f:13.0f)*s,juce::Font::bold));
        knobs[(size_t)i].setBounds(
            R(r.getX(),r.getY(),r.getWidth(),r.getHeight()));
        knobs[(size_t)i].setTextBoxStyle(
            juce::Slider::TextBoxBelow,false,
            juce::roundToInt((hero?76.0f:64.0f)*s),
            juce::roundToInt(19.0f*s));
    }

    mixLabel.setBounds(R(806,382,82,18));
    mixKnob.setBounds(R(806,396,82,68));
    outputLabel.setBounds(R(902,382,82,18));
    outputKnob.setBounds(R(902,396,82,68));

    prevPresetButton.setBounds(R(296,25,28,28));
    presetButton.setBounds(R(330,25,164,28));
    nextPresetButton.setBounds(R(500,25,28,28));
    saveButton.setBounds(R(538,25,58,28));
    abButton.setBounds(R(602,25,64,28));
    resetButton.setBounds(R(672,25,66,28));
    menuButton.setBounds(R(744,25,68,28));

    modeBox.setBounds(R(806,179,178,27));

    autoButton.setBounds(R(806,216,82,28));
    goldenButton.setBounds(R(902,216,82,28));
    maxButton.setBounds(R(806,252,82,28));
    deltaButton.setBounds(R(902,252,82,28));
    bypassButton.setBounds(R(854,288,82,28));
}

void KGHumanEngineAudioProcessorEditor::timerCallback()
{
    updateSpectrum();

    animationPhase+=0.018f;
    if (animationPhase>1.0f)
        animationPhase-=1.0f;

    repaint();
}
