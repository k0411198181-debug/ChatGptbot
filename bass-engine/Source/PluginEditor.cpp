#include "PluginEditor.h"

BassEngineLookAndFeel::BassEngineLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff7f2e7));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff111827));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x0037465f));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xfff3f6ff));
}

void BassEngineLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                                             juce::Slider& slider)
{
    const float diameter = (float)juce::jmin(width, height) - 16.0f;
    const float radius = diameter * 0.5f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto name = slider.getName();
    const bool hero = name == "WEIGHT";
    const bool gold = name == "SUB" || name == "WEIGHT" || name == "DRIVE" || name == "LOUD";

    const auto c1 = gold ? juce::Colour(0xffffc75a) : juce::Colour(0xff51c9ff);
    const auto c2 = gold ? juce::Colour(0xffff8c37) : juce::Colour(0xff8b65ff);

    if (hero)
    {
        juce::ColourGradient heroGlow(juce::Colour(0x8affbd46), cx, cy,
                                      juce::Colour(0x00ffbd46), cx + radius * 1.7f, cy, true);
        g.setGradientFill(heroGlow);
        g.fillEllipse(cx - radius * 1.50f, cy - radius * 1.50f,
                      radius * 3.0f, radius * 3.0f);
    }

    g.setColour(hero ? juce::Colour(0xa0705120) : juce::Colour(0x705071b8));
    g.drawEllipse(cx - radius - 5.0f, cy - radius - 5.0f,
                  diameter + 10.0f, diameter + 10.0f, hero ? 2.3f : 1.2f);

    juce::ColourGradient face(juce::Colour(0xff30394c), cx - radius * 0.50f, cy - radius * 0.75f,
                              juce::Colour(0xff090d15), cx + radius * 0.75f, cy + radius * 0.85f, false);
    face.addColour(0.46, juce::Colour(0xff171e2d));
    g.setGradientFill(face);
    g.fillEllipse(cx - radius, cy - radius, diameter, diameter);

    g.setColour(juce::Colour(0xff48536b));
    g.drawEllipse(cx - radius + 2.0f, cy - radius + 2.0f,
                  diameter - 4.0f, diameter - 4.0f, 1.0f);

    juce::Path bgArc;
    bgArc.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff2b3448));
    g.strokePath(bgArc, juce::PathStrokeType(hero ? 5.5f : 4.0f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(cx, cy, radius - 4.0f, radius - 4.0f, 0.0f,
                          rotaryStartAngle, angle, true);
    juce::ColourGradient arc(c1, cx - radius, cy, c2, cx + radius, cy, false);
    g.setGradientFill(arc);
    g.strokePath(valueArc, juce::PathStrokeType(hero ? 6.0f : 4.3f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    const float now = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001);
    const int ledCount = hero ? 33 : 25;
    const float ledR = radius + (hero ? 9.0f : 7.0f);
    const float dot = hero ? 4.3f : 3.2f;

    for (int i = 0; i < ledCount; ++i)
    {
        const float t = (float)i / (float)(ledCount - 1);
        const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float lx = cx + std::sin(a) * ledR;
        const float ly = cy - std::cos(a) * ledR;
        const bool on = t <= sliderPos + 0.002f;
        const float dist = std::abs(t - sliderPos);
        const float nearPointer = juce::jlimit(0.0f, 1.0f, 1.0f - dist * 15.0f);
        const float pulse = 0.68f + 0.32f * std::sin(now * 5.8f - (float)i * 0.43f);

        if (on)
        {
            const auto led = c1.interpolatedWith(c2, t);
            g.setColour(led.withAlpha(0.18f + 0.42f * nearPointer * pulse));
            g.fillEllipse(lx - dot, ly - dot, dot * 2.0f, dot * 2.0f);
            g.setColour(led.withAlpha(0.72f + 0.28f * nearPointer * pulse));
            g.fillEllipse(lx - dot * 0.48f, ly - dot * 0.48f, dot * 0.96f, dot * 0.96f);
        }
        else
        {
            g.setColour(juce::Colour(0x4438495f));
            g.fillEllipse(lx - dot * 0.35f, ly - dot * 0.35f, dot * 0.70f, dot * 0.70f);
        }
    }

    const float midAngle = rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle);
    const float mx = cx + std::sin(midAngle) * ledR;
    const float my = cy - std::cos(midAngle) * ledR;
    g.setColour(juce::Colour(0xfff4fbff));
    g.fillEllipse(mx - 2.3f, my - 2.3f, 4.6f, 4.6f);

    juce::Path pointer;
    const float pointerLength = radius * 0.66f;
    const float pointerThickness = hero ? 4.3f : 3.2f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f, -pointerLength,
                                pointerThickness, pointerLength * 0.57f, 2.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
    g.setColour(juce::Colour(0xfff8fbff));
    g.fillPath(pointer);

    g.setColour(gold ? juce::Colour(0x88543b11) : juce::Colour(0x66354e83));
    g.fillEllipse(cx - radius * 0.12f, cy - radius * 0.12f,
                  radius * 0.24f, radius * 0.24f);
}

void BassEngineLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b,
                                             bool over, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState();

    auto fill = juce::Colour(0xff111827);
    if (on)
    {
        if (b.getButtonText() == "AUTO") fill = juce::Colour(0xff264f8f);
        else if (b.getButtonText() == "MAX") fill = juce::Colour(0xff9a5516);
        else if (b.getButtonText() == "BYPASS") fill = juce::Colour(0xff6a2a35);
        else fill = juce::Colour(0xff604719);
    }

    if (over) fill = fill.brighter(0.10f);
    if (down) fill = fill.darker(0.12f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 7.0f);
    g.setColour(on ? juce::Colour(0xffffd36f) : juce::Colour(0xff3e4b62));
    g.drawRoundedRectangle(r, 7.0f, on ? 1.8f : 1.0f);
    g.setColour(on ? juce::Colours::white : juce::Colour(0xffaab8ce));
    g.setFont(juce::FontOptions(juce::jlimit(10.0f, 15.0f, b.getHeight() * 0.36f), juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(), juce::Justification::centred, 1);
}

BassEngineAudioProcessorEditor::BassEngineAudioProcessorEditor(BassEngineAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lnf);
    setSize(1024, 600);
    setResizable(true, true);
    setResizeLimits(780, 457, 1536, 900);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(1024.0 / 600.0);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        configureKnob(knobs[i], labels[i], names[i], i == 3);
        knobs[i].setName(names[i]);
        attachments[i] = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, ids[i], knobs[i]);
    }

    configureKnob(clarityKnob, clarityLabel, "CLARITY");
    configureKnob(mixKnob, mixLabel, "MIX");
    configureKnob(outputKnob, outputLabel, "OUTPUT");

    clarityKnob.setTextValueSuffix(" %");
    mixKnob.setTextValueSuffix(" %");
    outputKnob.setTextValueSuffix(" dB");

    clarityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "clarity", clarityKnob);
    mixAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "mix", mixKnob);
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, "output", outputKnob);

    for (auto* b : { &autoButton, &maxButton, &bypassButton })
    {
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }

    addAndMakeVisible(goldenButton);
    goldenButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5b4317));
    goldenButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffda80));

    autoAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "auto", autoButton);
    maxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "max", maxButton);
    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, "bypass", bypassButton);

    autoButton.onClick = [this]
    {
        if (autoButton.getToggleState())
            applyGoldenSettings();
    };

    goldenButton.onClick = [this] { applyGoldenSettings(); };

    startTimerHz(30);
}

BassEngineAudioProcessorEditor::~BassEngineAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void BassEngineAudioProcessorEditor::configureKnob(juce::Slider& k, juce::Label& label,
                                                    const juce::String& name, bool hero)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, hero ? 76 : 64, 19);
    k.setDoubleClickReturnValue(true, 50.0);
    k.setPopupDisplayEnabled(false, false, this);
    addAndMakeVisible(k);

    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId,
                    hero ? juce::Colour(0xffffdc86) : juce::Colour(0xffd8e5f7));
    label.setFont(juce::FontOptions(hero ? 15.2f : 13.0f, juce::Font::bold));
    addAndMakeVisible(label);
}

void BassEngineAudioProcessorEditor::applyGoldenSettings()
{
    const std::array<float, 7> golden {
        52.0f, 42.0f, 48.0f, 62.0f, 50.0f, 44.0f, 30.0f
    };

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

    setParam("clarity", 45.0f);
    setParam("mix", 100.0f);
    setParam("output", 0.0f);

    if (auto* p = processor.apvts.getParameter("auto"))
        p->setValueNotifyingHost(1.0f);

    if (auto* p = processor.apvts.getParameter("max"))
        p->setValueNotifyingHost(0.0f);
}

void BassEngineAudioProcessorEditor::updateSpectrum()
{
    const int pulled = processor.pullVisualizationSamples(
        visualPullBuffer.data(), (int)visualPullBuffer.size());

    bool transformed = false;

    for (int i = 0; i < pulled; ++i)
    {
        fftInput[(size_t)fftInputPos++] = visualPullBuffer[(size_t)i];

        if (fftInputPos >= fftSize)
        {
            std::fill(fftData.begin(), fftData.end(), 0.0f);
            std::copy(fftInput.begin(), fftInput.end(), fftData.begin());

            fftWindow.multiplyWithWindowingTable(fftData.data(), fftSize);
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

            const double sr = processor.getSampleRate() > 1000.0
                            ? processor.getSampleRate() : 44100.0;
            const double lowHz = 24.0;
            const double highHz = juce::jmin(6000.0, sr * 0.45);

            for (int band = 0; band < spectrumBands; ++band)
            {
                const double t = ((double)band + 0.5) / (double)spectrumBands;
                const double freq = lowHz * std::pow(highHz / lowHz, t);
                const int centre = juce::jlimit(
                    1, fftSize / 2 - 2,
                    (int)std::round(freq * (double)fftSize / sr));

                float mag = 0.0f;
                for (int k = -1; k <= 1; ++k)
                    mag += fftData[(size_t)(centre + k)];
                mag /= 3.0f;

                const float db = juce::Decibels::gainToDecibels(
                    mag / (float)fftSize, -96.0f);
                const float normal = juce::jmap(
                    juce::jlimit(-82.0f, -8.0f, db),
                    -82.0f, -8.0f, 0.0f, 1.0f);

                spectrumValues[(size_t)band] =
                    juce::jmax(normal, spectrumValues[(size_t)band] * 0.84f);
            }

            fftInputPos = 0;
            transformed = true;
        }
    }

    if (!transformed)
        for (auto& v : spectrumValues)
            v *= 0.975f;

    const auto smoothMeter = [](float current, float target)
    {
        return target > current ? 0.58f * current + 0.42f * target
                                : current * 0.90f;
    };

    smoothInL = smoothMeter(smoothInL, processor.getInputMeter(0));
    smoothInR = smoothMeter(smoothInR, processor.getInputMeter(1));
    smoothOutL = smoothMeter(smoothOutL, processor.getOutputMeter(0));
    smoothOutR = smoothMeter(smoothOutR, processor.getOutputMeter(1));
}

void BassEngineAudioProcessorEditor::drawSpectrumAndMeters(juce::Graphics& g)
{
    const juce::Rectangle<float> scope(276.0f, 484.0f, 472.0f, 75.0f);

    g.setColour(juce::Colour(0x5a0c111a));
    g.fillRoundedRectangle(scope, 12.0f);
    g.setColour(juce::Colour(0x77765a2a));
    g.drawRoundedRectangle(scope, 12.0f, 1.0f);

    const float gap = 2.0f;
    const float bandW = (scope.getWidth() - 18.0f - gap * (spectrumBands - 1))
                      / (float)spectrumBands;
    const float baseY = scope.getBottom() - 9.0f;

    juce::Path line;

    for (int i = 0; i < spectrumBands; ++i)
    {
        const float v = juce::jlimit(0.0f, 1.0f, spectrumValues[(size_t)i]);
        const float x = scope.getX() + 9.0f + (bandW + gap) * (float)i;
        const float h = 2.0f + v * 44.0f;
        const float y = baseY - h;

        juce::ColourGradient bar(juce::Colour(0xffffc85a), x, baseY,
                                 juce::Colour(0xff675cff), x, y, false);
        g.setGradientFill(bar);
        g.fillRoundedRectangle(x, y, juce::jmax(1.0f, bandW), h, 1.0f);

        const float px = x + bandW * 0.5f;
        const float py = y - 2.0f;
        if (i == 0) line.startNewSubPath(px, py);
        else line.lineTo(px, py);
    }

    g.setColour(juce::Colour(0x80ffb84a));
    g.strokePath(line, juce::PathStrokeType(4.5f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));
    g.setColour(juce::Colour(0xfffff0c4));
    g.strokePath(line, juce::PathStrokeType(1.1f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xffbea46e));
    g.setFont(juce::FontOptions(9.0f, juce::Font::bold));
    g.drawText("BASS SPECTRUM  24 Hz - 6 kHz", 390, 486, 244, 12,
               juce::Justification::centred);

    const auto meter = [&g](float x, float y, float w, float l, float r, const juce::String& label)
    {
        g.setColour(juce::Colour(0xff9a885f));
        g.setFont(juce::FontOptions(8.2f, juce::Font::bold));
        g.drawText(label, (int)x, (int)(y - 13.0f), (int)w, 10, juce::Justification::centred);

        const int segments = 13;
        const float gap = 1.5f;
        const float segW = (w - gap * (segments - 1)) / (float)segments;

        for (int row = 0; row < 2; ++row)
        {
            const float value = juce::jlimit(0.0f, 1.0f, std::sqrt(row == 0 ? l : r));
            const int lit = juce::roundToInt(value * (float)segments);

            for (int i = 0; i < segments; ++i)
            {
                const float sx = x + (segW + gap) * (float)i;
                const bool on = i < lit;
                g.setColour(on
                    ? juce::Colour(0xff4fc8ff).interpolatedWith(
                        juce::Colour(0xffffbd4d), (float)i / (float)(segments - 1))
                    : juce::Colour(0x26324050));
                g.fillRoundedRectangle(sx, y + row * 8.0f, segW, 4.5f, 1.0f);
            }
        }
    };

    meter(286.0f, 510.0f, 72.0f, smoothInL, smoothInR, "IN");
    meter(666.0f, 510.0f, 72.0f, smoothOutL, smoothOutR, "OUT");
}

void BassEngineAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff04070d));

    const float scale = (float)getWidth() / 1024.0f;
    juce::Graphics::ScopedSaveState save(g);
    g.addTransform(juce::AffineTransform::scale(scale));

    juce::ColourGradient bg(juce::Colour(0xff04070d), 0.0f, 0.0f,
                            juce::Colour(0xff111827), 1024.0f, 600.0f, false);
    bg.addColour(0.42, juce::Colour(0xff08121f));
    g.setGradientFill(bg);
    g.fillRect(0.0f, 0.0f, 1024.0f, 600.0f);

    const float energy = juce::jlimit(0.0f, 1.0f, std::sqrt(juce::jmax(smoothOutL, smoothOutR)));
    const float pulse = 0.72f
                      + 0.16f * std::sin(juce::MathConstants<float>::twoPi * animationPhase)
                      + 0.20f * energy;

    juce::ColourGradient nebula(juce::Colour::fromFloatRGBA(0.18f, 0.34f, 0.95f, 0.30f * pulse),
                                405.0f, 235.0f,
                                juce::Colour::fromFloatRGBA(0.15f, 0.05f, 0.35f, 0.0f),
                                825.0f, 250.0f, true);
    nebula.addColour(0.46, juce::Colour::fromFloatRGBA(0.52f, 0.20f, 1.0f, 0.22f * pulse));
    g.setGradientFill(nebula);
    g.fillEllipse(80.0f, 30.0f, 790.0f, 450.0f);

    juce::ColourGradient goldGlow(juce::Colour::fromFloatRGBA(1.0f, 0.58f, 0.10f, 0.20f * pulse),
                                  585.0f, 255.0f,
                                  juce::Colour::fromFloatRGBA(0.40f, 0.16f, 0.02f, 0.0f),
                                  920.0f, 290.0f, true);
    g.setGradientFill(goldGlow);
    g.fillEllipse(330.0f, 80.0f, 650.0f, 385.0f);

    // Planet / moon in the upper-right, inspired by the concept face.
    juce::ColourGradient planet(juce::Colour(0xffd4d8e4), 869.0f, 76.0f,
                                juce::Colour(0xff33394b), 920.0f, 132.0f, true);
    planet.addColour(0.42, juce::Colour(0xff777e92));
    g.setGradientFill(planet);
    g.fillEllipse(844.0f, 52.0f, 92.0f, 92.0f);
    g.setColour(juce::Colour(0x66ffffff));
    g.drawEllipse(844.0f, 52.0f, 92.0f, 92.0f, 1.0f);
    g.setColour(juce::Colour(0x55707a90));
    g.fillEllipse(865.0f, 75.0f, 18.0f, 10.0f);
    g.fillEllipse(897.0f, 94.0f, 14.0f, 9.0f);
    g.fillEllipse(878.0f, 112.0f, 10.0f, 6.0f);

    for (int i = 0; i < 110; ++i)
    {
        const int sx = (i * 151 + 37) % 1024;
        const int sy = (i * 89 + 23) % 470;
        const float twinkle = 0.5f + 0.5f * std::sin(
            juce::MathConstants<float>::twoPi * (animationPhase + 0.059f * (float)i));
        const float a = 0.10f + 0.72f * twinkle;
        const float rr = (i % 13 == 0) ? 2.2f : ((i % 5 == 0) ? 1.45f : 0.78f);
        g.setColour((i % 4 == 0)
            ? juce::Colour::fromFloatRGBA(1.0f, 0.78f, 0.35f, a)
            : juce::Colour::fromFloatRGBA(0.55f, 0.78f, 1.0f, a));
        g.fillEllipse((float)sx, (float)sy, rr, rr);
    }

    // Brushed-metal style panels.
    g.setColour(juce::Colour(0x72111622));
    g.fillRoundedRectangle(22.0f, 92.0f, 758.0f, 370.0f, 18.0f);
    g.setColour(juce::Colour(0x7a53627c));
    g.drawRoundedRectangle(22.0f, 92.0f, 758.0f, 370.0f, 18.0f, 1.0f);

    g.setColour(juce::Colour(0x65131621));
    g.fillRoundedRectangle(792.0f, 158.0f, 208.0f, 304.0f, 18.0f);
    g.setColour(juce::Colour(0x80735b2e));
    g.drawRoundedRectangle(792.0f, 158.0f, 208.0f, 304.0f, 18.0f, 1.0f);

    g.setColour(juce::Colour(0xfff4f7ff));
    g.setFont(juce::FontOptions(28.5f, juce::Font::bold));
    g.drawText("BASS ENGINE", 32, 20, 380, 38, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xffffd36d));
    g.setFont(juce::FontOptions(10.8f, juce::Font::bold));
    g.drawText("GALAXY SIGNAL RECORDS", 35, 58, 260, 17, juce::Justification::centredLeft);

    g.setColour(juce::Colour(0xff8399bb));
    g.setFont(juce::FontOptions(10.4f));
    g.drawText("SUB IMPACT  |  WEIGHT  |  SATURATION  |  PUNCH  |  TIGHT CONTROL",
               328, 64, 446, 16, juce::Justification::centredRight);

    g.setColour(juce::Colour(0xffffc95d));
    g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    g.drawText("LOW END CONTROL", 806, 174, 180, 18, juce::Justification::centred);

    g.setColour(juce::Colour(0xff8699b5));
    g.setFont(juce::FontOptions(9.2f));
    g.drawText("CLARITY", 804, 292, 184, 14, juce::Justification::centred);
    g.drawText("MIX", 804, 369, 84, 14, juce::Justification::centred);
    g.drawText("OUTPUT", 900, 369, 84, 14, juce::Justification::centred);

    drawSpectrumAndMeters(g);

    g.setColour(juce::Colour(0xff7f7358));
    g.setFont(juce::FontOptions(9.4f));
    g.drawText("AUTO = GOLDEN + ADAPTIVE LOW PROTECTION   |   MAX = UNLOCK AGGRESSIVE FAT MODE",
               205, 566, 614, 16, juce::Justification::centred);

    g.setColour(juce::Colour(0xff596981));
    g.setFont(juce::FontOptions(9.0f));
    g.drawText("Bass Engine VST3  v0.1", 32, 574, 190, 14, juce::Justification::centredLeft);
}

void BassEngineAudioProcessorEditor::resized()
{
    const float s = (float)getWidth() / 1024.0f;

    const auto R = [s](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(
            juce::roundToInt(x * s), juce::roundToInt(y * s),
            juce::roundToInt(w * s), juce::roundToInt(h * s));
    };

    // Top row: SUB / DRIVE / PUNCH / WEIGHT(hero)
    const std::array<juce::Rectangle<float>, 7> knobRects {
        juce::Rectangle<float>(48, 132, 126, 156),
        juce::Rectangle<float>(206, 132, 126, 156),
        juce::Rectangle<float>(364, 132, 126, 156),
        juce::Rectangle<float>(522, 112, 154, 182),
        juce::Rectangle<float>(126, 292, 126, 148),
        juce::Rectangle<float>(316, 292, 126, 148),
        juce::Rectangle<float>(506, 292, 126, 148)
    };

    for (int i = 0; i < 7; ++i)
    {
        const bool hero = i == 3;
        const auto r = knobRects[(size_t)i];
        labels[(size_t)i].setBounds(R(r.getX(), r.getY() - 18.0f, r.getWidth(), 20.0f));
        labels[(size_t)i].setFont(juce::FontOptions((hero ? 15.2f : 13.0f) * s, juce::Font::bold));
        knobs[(size_t)i].setBounds(R(r.getX(), r.getY(), r.getWidth(), r.getHeight()));
        knobs[(size_t)i].setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                         juce::roundToInt((hero ? 76.0f : 64.0f) * s),
                                         juce::roundToInt(19.0f * s));
    }

    clarityLabel.setBounds(R(817, 302, 158, 18));
    clarityKnob.setBounds(R(827, 316, 138, 108));

    mixLabel.setBounds(R(806, 382, 82, 18));
    mixKnob.setBounds(R(806, 396, 82, 68));

    outputLabel.setBounds(R(902, 382, 82, 18));
    outputKnob.setBounds(R(902, 396, 82, 68));

    for (auto* label : { &clarityLabel, &mixLabel, &outputLabel })
        label->setFont(juce::FontOptions(11.0f * s, juce::Font::bold));

    clarityKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                                juce::roundToInt(68.0f * s), juce::roundToInt(18.0f * s));
    mixKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                            juce::roundToInt(58.0f * s), juce::roundToInt(17.0f * s));
    outputKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false,
                               juce::roundToInt(58.0f * s), juce::roundToInt(17.0f * s));

    autoButton.setBounds(R(806, 205, 82, 28));
    goldenButton.setBounds(R(902, 205, 82, 28));
    maxButton.setBounds(R(806, 245, 82, 28));
    bypassButton.setBounds(R(902, 245, 82, 28));
}

void BassEngineAudioProcessorEditor::timerCallback()
{
    updateSpectrum();

    animationPhase += 0.018f;
    if (animationPhase > 1.0f)
        animationPhase -= 1.0f;

    repaint();
}
