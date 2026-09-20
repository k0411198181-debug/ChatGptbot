#include "PluginEditor.h"

GalaxyLookAndFeel::GalaxyLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xffdce8ff));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff11182a));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00263658));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffd6e4ff));
}

void GalaxyLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                         float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                         juce::Slider& slider)
{
    const float diameter = (float)juce::jmin(width, height) - 12.0f;
    const float radius = diameter * 0.5f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    const bool hero = slider.getName() == "SIZE";

    if (hero)
    {
        juce::ColourGradient glow(juce::Colour(0x663c2fff), cx, cy,
                                  juce::Colour(0x003c2fff), cx + radius * 1.5f, cy, true);
        g.setGradientFill(glow);
        g.fillEllipse(cx - radius * 1.32f, cy - radius * 1.32f, radius * 2.64f, radius * 2.64f);
    }

    g.setColour(juce::Colour(0x552151ff));
    g.drawEllipse(cx - radius - 4.0f, cy - radius - 4.0f, diameter + 8.0f, diameter + 8.0f, hero ? 2.5f : 1.2f);

    juce::ColourGradient face(juce::Colour(0xff252c46), cx - radius * 0.55f, cy - radius * 0.75f,
                              juce::Colour(0xff090d18), cx + radius * 0.75f, cy + radius * 0.80f, false);
    face.addColour(0.52, juce::Colour(0xff171d31));
    g.setGradientFill(face);
    g.fillEllipse(cx - radius, cy - radius, diameter, diameter);

    juce::Path bgArc;
    bgArc.addCentredArc(cx, cy, radius - 3.0f, radius - 3.0f, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff27334f));
    g.strokePath(bgArc, juce::PathStrokeType(hero ? 5.0f : 3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(cx, cy, radius - 3.0f, radius - 3.0f, 0.0f,
                          rotaryStartAngle, angle, true);
    juce::ColourGradient arc(juce::Colour(0xff35c7ff), cx - radius, cy,
                             juce::Colour(0xff9a57ff), cx + radius, cy, false);
    g.setGradientFill(arc);
    g.strokePath(valueArc, juce::PathStrokeType(hero ? 5.3f : 3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path pointer;
    const float pointerLength = radius * 0.63f;
    const float pointerThickness = hero ? 4.0f : 3.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength, pointerThickness, pointerLength * 0.54f, 2.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
    g.setColour(juce::Colour(0xffe7f7ff));
    g.fillPath(pointer);

    g.setColour(juce::Colour(0x5538c8ff));
    g.drawEllipse(cx - radius * 0.17f, cy - radius * 0.17f, radius * 0.34f, radius * 0.34f, 1.0f);
}

void GalaxyLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState();
    auto fill = on ? juce::Colour(0xff5138bd) : juce::Colour(0xff101727);
    if (over) fill = fill.brighter(0.10f);
    if (down) fill = fill.darker(0.10f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 8.0f);
    g.setColour(on ? juce::Colour(0xff6fdcff) : juce::Colour(0xff33405c));
    g.drawRoundedRectangle(r, 8.0f, on ? 1.8f : 1.0f);
    g.setColour(on ? juce::Colours::white : juce::Colour(0xff9aaccc));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

KGVocalEngineAudioProcessorEditor::KGVocalEngineAudioProcessorEditor(KGVocalEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&galaxyLnf);
    setSize(980, 510);
    setResizable(false, false);

    for (size_t i=0; i<knobs.size(); ++i)
    {
        configureKnob(knobs[i], labels[i], names[i], i == 3);
        knobs[i].setName(names[i]);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, ids[i], knobs[i]);
    }

    configureKnob(inputKnob, inputLabel, "INPUT");
    configureKnob(mixKnob, mixLabel, "MIX");
    configureKnob(outputKnob, outputLabel, "OUTPUT");
    configureKnob(throwKnob, throwLabel, "THROW");

    inputKnob.setTextValueSuffix(" dB");
    outputKnob.setTextValueSuffix(" dB");
    mixKnob.setTextValueSuffix(" %");
    throwKnob.setTextValueSuffix(" %");

    inputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "input", inputKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "mix", mixKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "output", outputKnob);
    throwAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(processor.apvts, "throw", throwKnob);

    for (auto* b : { &autoButton, &liveButton, &syncButton, &bypassButton })
    {
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }

    autoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "auto", autoButton);
    liveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "live", liveButton);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "sync", syncButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processor.apvts, "bypass", bypassButton);

    // AUTO is intentionally visible and deterministic: when switched on by the user,
    // recall the KG LIVE VOCAL golden macro settings, while the processor's hidden
    // adaptive gain / de-ess / breath logic continues to operate.
    autoButton.onClick = [this]
    {
        if (!autoButton.getToggleState())
            return;

        const std::array<float, 7> golden { 55.0f, 46.0f, 38.0f, 46.0f, 48.0f, 45.0f, 28.0f };

        for (size_t i = 0; i < ids.size(); ++i)
        {
            if (auto* p = processor.apvts.getParameter(ids[i]))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->convertTo0to1(golden[i]));
                p->endChangeGesture();
            }
        }

        const auto setParam = [this](const char* id, float value)
        {
            if (auto* p = processor.apvts.getParameter(id))
            {
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->convertTo0to1(value));
                p->endChangeGesture();
            }
        };

        setParam("input",  0.0f);
        setParam("throw",  0.0f);
        setParam("mix",  100.0f);
        setParam("output", 0.0f);
    };

    startTimerHz(20);
}

KGVocalEngineAudioProcessorEditor::~KGVocalEngineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void KGVocalEngineAudioProcessorEditor::configureKnob(juce::Slider& k, juce::Label& label,
                                                       const juce::String& name, bool hero)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, hero ? 72 : 62, 19);
    k.setDoubleClickReturnValue(true, hero ? 46.0 : 50.0);
    k.setPopupDisplayEnabled(false, false, this);
    addAndMakeVisible(k);

    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId, hero ? juce::Colour(0xfff0ecff) : juce::Colour(0xffc9d8f2));
    label.setFont(juce::FontOptions(hero ? 14.5f : 13.0f, juce::Font::bold));
    addAndMakeVisible(label);
}

void KGVocalEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    juce::ColourGradient bg(juce::Colour(0xff070a14), 0, 0,
                            juce::Colour(0xff11162b), getWidth(), getHeight(), false);
    bg.addColour(0.42, juce::Colour(0xff0b1020));
    g.setGradientFill(bg);
    g.fillRect(bounds);

    juce::ColourGradient glowA(juce::Colour(0x552d55ff), 230.0f, 220.0f,
                               juce::Colour(0x002d55ff), 520.0f, 220.0f, true);
    g.setGradientFill(glowA);
    g.fillEllipse(20.0f, 30.0f, 560.0f, 390.0f);

    // Stronger visible galaxy core / nebula, still kept behind controls.
    juce::ColourGradient galaxyCore(juce::Colour(0x7048b8ff), 490.0f, 235.0f,
                                    juce::Colour(0x0048b8ff), 760.0f, 235.0f, true);
    galaxyCore.addColour(0.42, juce::Colour(0x3f9f68ff));
    g.setGradientFill(galaxyCore);
    g.fillEllipse(285.0f, 78.0f, 500.0f, 325.0f);

    g.setColour(juce::Colour(0x235fdcff));
    for (int a = 0; a < 5; ++a)
    {
        juce::Path arm;
        const float inset = (float)a * 18.0f;
        arm.addCentredArc(520.0f, 236.0f, 245.0f - inset, 118.0f - inset * 0.30f,
                          -0.40f + 0.18f * (float)a, 0.20f, 2.75f, true);
        g.strokePath(arm, juce::PathStrokeType(1.0f + 0.22f * (float)a,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }
    juce::ColourGradient glowB(juce::Colour(0x553f1d8e), 760.0f, 250.0f,
                               juce::Colour(0x003f1d8e), 970.0f, 250.0f, true);
    g.setGradientFill(glowB);
    g.fillEllipse(560.0f, 80.0f, 430.0f, 330.0f);

    g.setColour(juce::Colour(0x6686baff));
    for (int i=0; i<58; ++i)
    {
        const int sx = (i * 137 + 41) % getWidth();
        const int sy = (i * 83 + 27) % getHeight();
        const float rr = (i % 7 == 0) ? 1.6f : 0.8f;
        g.fillEllipse((float)sx, (float)sy, rr, rr);
    }

    g.setColour(juce::Colour(0xffedf5ff));
    g.setFont(juce::FontOptions(29.0f, juce::Font::bold));
    g.drawText("KG VOCAL ENGINE", 34, 22, 460, 38, juce::Justification::centredLeft);
    g.setColour(juce::Colour(0xff7ba8d8));
    g.setFont(juce::FontOptions(11.5f));
    g.drawText("GALAXY VOCAL PROCESSOR  |  KG LIVE VOCAL", 37, 59, 500, 20, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8c6cff));
    g.setFont(juce::FontOptions(12.0f, juce::Font::bold));
    g.drawText("AUTO SHAPE  |  WIDTH  |  DUCKED DELAY  |  SPACE", 530, 32, 405, 22, juce::Justification::centredRight);

    g.setColour(juce::Colour(0x55273756));
    g.fillRoundedRectangle(24.0f, 92.0f, 932.0f, 296.0f, 18.0f);
    g.setColour(juce::Colour(0x553e64a8));
    g.drawRoundedRectangle(24.0f, 92.0f, 932.0f, 296.0f, 18.0f, 1.0f);

    g.setColour(juce::Colour(0xff8ca6cf));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("THROW is automatable in REAPER: raise it only on words where the delay tail must bloom.",
               260, 445, 460, 18, juce::Justification::centred);

    g.setColour(juce::Colour(0xff52698f));
    g.setFont(juce::FontOptions(10.5f));
    g.drawText("KG MUSIC RECORDS  |  VST3 v0.3.4", 33, 482, 300, 17, juce::Justification::centredLeft);
}

void KGVocalEngineAudioProcessorEditor::resized()
{
    const int mainY = 126;
    const int standardW = 112;
    const int heroW = 136;
    const std::array<int, 7> xs { 35, 160, 285, 408, 550, 675, 800 };

    for (int i=0; i<7; ++i)
    {
        const bool hero = i == 3;
        const int w = hero ? heroW : standardW;
        const int x = xs[(size_t)i] - (hero ? 12 : 0);
        labels[(size_t)i].setBounds(x, mainY, w, 24);
        knobs[(size_t)i].setBounds(x, mainY + 22, w, hero ? 166 : 146);
    }

    inputLabel.setBounds(36, 398, 82, 19); inputKnob.setBounds(36, 414, 82, 72);
    throwLabel.setBounds(136, 398, 82, 19); throwKnob.setBounds(136, 414, 82, 72);
    mixLabel.setBounds(760, 398, 82, 19); mixKnob.setBounds(760, 414, 82, 72);
    outputLabel.setBounds(860, 398, 82, 19); outputKnob.setBounds(860, 414, 82, 72);

    autoButton.setBounds(286, 407, 84, 32);
    liveButton.setBounds(382, 407, 84, 32);
    syncButton.setBounds(478, 407, 84, 32);
    bypassButton.setBounds(574, 407, 94, 32);
}

void KGVocalEngineAudioProcessorEditor::timerCallback()
{
    sparklePhase += 0.04f;
    if (sparklePhase > 1.0f) sparklePhase -= 1.0f;
    repaint();
}
