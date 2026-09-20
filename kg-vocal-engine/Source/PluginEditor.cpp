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

    // Fixed 50% reference point on the scale.
    const float midAngle = rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle);
    const float markerR = radius + (hero ? 5.0f : 3.5f);
    const float mx = cx + std::sin(midAngle) * markerR;
    const float my = cy - std::cos(midAngle) * markerR;
    g.setColour(juce::Colour(0xffa9eaff));
    g.fillEllipse(mx - (hero ? 2.7f : 2.1f), my - (hero ? 2.7f : 2.1f),
                  hero ? 5.4f : 4.2f, hero ? 5.4f : 4.2f);
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
    g.setFont(juce::FontOptions(juce::jlimit(10.0f, 16.0f, b.getHeight() * 0.36f), juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

KGVocalEngineAudioProcessorEditor::KGVocalEngineAudioProcessorEditor(KGVocalEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&galaxyLnf);
    setSize(980, 510);
    setResizable(true, true);
    setResizeLimits(760, 395, 1470, 765);
    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(980.0 / 510.0);

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

    // AUTO recalls the KG golden start and keeps hidden adaptive processing enabled.
    autoButton.onClick = [this]
    {
        if (autoButton.getToggleState())
            applyGoldenSettings();
    };

    for (auto* b : { &goldenButton, &infoButton, &menuButton })
    {
        addAndMakeVisible(*b);
        b->setColour(juce::TextButton::buttonColourId, juce::Colour(0xff101727));
        b->setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff293b72));
        b->setColour(juce::TextButton::textColourOffId, juce::Colour(0xffa9bddd));
        b->setColour(juce::TextButton::textColourOnId, juce::Colours::white);
    }

    goldenButton.onClick = [this]
    {
        if (auto* p = processor.apvts.getParameter("auto"))
            p->setValueNotifyingHost(1.0f);
        applyGoldenSettings();
    };

    infoButton.onClick = [this]
    {
        showHints = !showHints;
        repaint();
    };

    menuButton.onClick = [this] { showTopMenu(); };

    startTimerHz(15);
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

void KGVocalEngineAudioProcessorEditor::applyGoldenSettings()
{
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

    setParam("input", 0.0f);
    setParam("throw", 0.0f);
    setParam("mix", 100.0f);
    setParam("output", 0.0f);
}

void KGVocalEngineAudioProcessorEditor::setMainKnobsTo50()
{
    for (const auto& id : ids)
    {
        if (auto* p = processor.apvts.getParameter(id))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost(p->convertTo0to1(50.0f));
            p->endChangeGesture();
        }
    }
}

void KGVocalEngineAudioProcessorEditor::showTopMenu()
{
    juce::PopupMenu menu;
    menu.addItem(1, "Recall KG LIVE VOCAL");
    menu.addItem(2, "Set 7 main knobs to 50%");
    menu.addItem(3, "Reset THROW to 0%");
    menu.addSeparator();
    menu.addItem(4, showHints ? "Hide interface hints" : "Show interface hints");

    auto safeThis = juce::Component::SafePointer<KGVocalEngineAudioProcessorEditor>(this);
    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&menuButton),
        [safeThis](int result)
        {
            if (safeThis == nullptr)
                return;

            if (result == 1)
            {
                if (auto* p = safeThis->processor.apvts.getParameter("auto"))
                    p->setValueNotifyingHost(1.0f);
                safeThis->applyGoldenSettings();
            }
            else if (result == 2)
            {
                safeThis->setMainKnobsTo50();
            }
            else if (result == 3)
            {
                if (auto* p = safeThis->processor.apvts.getParameter("throw"))
                    p->setValueNotifyingHost(p->convertTo0to1(0.0f));
            }
            else if (result == 4)
            {
                safeThis->showHints = !safeThis->showHints;
                safeThis->repaint();
            }
        });
}

void KGVocalEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff050811));

    const float scale = (float)getWidth() / 980.0f;
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(scale));

    const juce::Rectangle<float> logical(0.0f, 0.0f, 980.0f, 510.0f);

    juce::ColourGradient bg(juce::Colour(0xff050813), 0.0f, 0.0f,
                            juce::Colour(0xff10172c), 980.0f, 510.0f, false);
    bg.addColour(0.43, juce::Colour(0xff081021));
    g.setGradientFill(bg);
    g.fillRect(logical);

    // Bright galaxy band inspired by the approved dashboard: visible, but behind the controls.
    const float pulse = 0.72f + 0.18f * std::sin(juce::MathConstants<float>::twoPi * sparklePhase);
    juce::ColourGradient halo(juce::Colour::fromFloatRGBA(0.25f, 0.43f, 1.0f, 0.34f * pulse),
                              490.0f, 170.0f,
                              juce::Colour::fromFloatRGBA(0.16f, 0.10f, 0.55f, 0.0f),
                              820.0f, 250.0f, true);
    halo.addColour(0.40, juce::Colour::fromFloatRGBA(0.48f, 0.28f, 1.0f, 0.20f * pulse));
    g.setGradientFill(halo);
    g.fillEllipse(175.0f, 25.0f, 650.0f, 345.0f);

    // Luminous galactic arc / horizon.
    for (int i = 0; i < 5; ++i)
    {
        juce::Path arc;
        const float inset = (float)i * 7.5f;
        arc.addCentredArc(490.0f, 170.0f,
                          330.0f - inset, 132.0f - inset * 0.35f,
                          0.0f, 3.95f, 5.48f, true);
        const float a = (0.28f - i * 0.035f) * pulse;
        g.setColour(juce::Colour::fromFloatRGBA(0.34f + 0.05f * i, 0.62f, 1.0f, a));
        g.strokePath(arc, juce::PathStrokeType(i == 0 ? 2.2f : 1.1f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    }

    // Secondary violet cloud on the right.
    juce::ColourGradient violet(juce::Colour::fromFloatRGBA(0.45f, 0.18f, 0.95f, 0.22f * pulse),
                                735.0f, 210.0f,
                                juce::Colour::fromFloatRGBA(0.18f, 0.08f, 0.40f, 0.0f),
                                970.0f, 250.0f, true);
    g.setGradientFill(violet);
    g.fillEllipse(530.0f, 55.0f, 455.0f, 330.0f);

    // Twinkling star field.
    for (int i = 0; i < 92; ++i)
    {
        const int sx = (i * 137 + 41) % 980;
        const int sy = (i * 83 + 27) % 390;
        const float twinkle = 0.5f + 0.5f * std::sin(juce::MathConstants<float>::twoPi
                                                    * (sparklePhase + 0.071f * (float)i));
        const float alpha = 0.16f + 0.64f * twinkle;
        const float rr = (i % 11 == 0) ? 2.2f : ((i % 5 == 0) ? 1.45f : 0.82f);
        g.setColour(juce::Colour::fromFloatRGBA(0.58f, 0.78f, 1.0f, alpha));
        g.fillEllipse((float)sx, (float)sy, rr, rr);
    }

    // Main glass panel.
    g.setColour(juce::Colour(0x4a111a2d));
    g.fillRoundedRectangle(24.0f, 92.0f, 932.0f, 296.0f, 18.0f);
    g.setColour(juce::Colour(0x705b82d4));
    g.drawRoundedRectangle(24.0f, 92.0f, 932.0f, 296.0f, 18.0f, 1.0f);

    // Header.
    g.setColour(juce::Colour(0xffeef6ff));
    g.setFont(juce::FontOptions(29.0f, juce::Font::bold));
    g.drawText("KG VOCAL ENGINE", 250, 20, 480, 38, juce::Justification::centred);

    g.setColour(juce::Colour(0xff87a9d8));
    g.setFont(juce::FontOptions(11.2f));
    g.drawText("GALAXY VOCAL PROCESSOR  |  KG LIVE VOCAL", 286, 57, 408, 18, juce::Justification::centred);

    g.setColour(juce::Colour(0xff7e9ad1));
    g.setFont(juce::FontOptions(10.5f, juce::Font::bold));
    g.drawText("AUTO SHAPE  |  WIDTH  |  DUCKED DELAY  |  SPACE", 535, 75, 392, 16, juce::Justification::centredRight);

    if (showHints)
    {
        g.setColour(juce::Colour(0xff8ca6cf));
        g.setFont(juce::FontOptions(10.3f));
        g.drawText("AUTO = GOLDEN START + ADAPTIVE CONTROL   |   LIVE FX = TIGHTER DELAY   |   BPM SYNC = REAPER TEMPO",
                   182, 442, 616, 16, juce::Justification::centred);

        g.setColour(juce::Colour(0xff708ab5));
        g.setFont(juce::FontOptions(9.8f));
        g.drawText("THROW: automate selected words for a larger delay tail.",
                   250, 458, 480, 15, juce::Justification::centred);
    }

    g.setColour(juce::Colour(0xff52698f));
    g.setFont(juce::FontOptions(10.0f));
    g.drawText("KG MUSIC RECORDS  |  VST3 v0.4.0", 33, 482, 300, 17, juce::Justification::centredLeft);
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
