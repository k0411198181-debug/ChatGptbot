#include "PluginEditor.h"

MusicGoldLookAndFeel::MusicGoldLookAndFeel()
{
    setColour(juce::Slider::textBoxTextColourId, juce::Colour(0xfff3f6ff));
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour(0xff11182a));
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colour(0x00263658));
    setColour(juce::ToggleButton::textColourId, juce::Colour(0xffeef4ff));
}

void MusicGoldLookAndFeel::drawRotarySlider(juce::Graphics& g,
                                            int x, int y, int width, int height,
                                            float sliderPos,
                                            float rotaryStartAngle,
                                            float rotaryEndAngle,
                                            juce::Slider& slider)
{
    const float diameter = (float)juce::jmin(width, height) - 14.0f;
    const float radius = diameter * 0.5f;
    const float cx = x + width * 0.5f;
    const float cy = y + height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const auto name = slider.getName();
    const bool hero = name == "WEIGHT";
    const bool goldFamily = name == "WEIGHT" || name == "PUNCH"
                         || name == "GLUE" || name == "LOUD"
                         || name == "SUB";

    const auto activeA = goldFamily ? juce::Colour(0xffffc857) : juce::Colour(0xff48cfff);
    const auto activeB = goldFamily ? juce::Colour(0xffff8e3a) : juce::Colour(0xff6f8dff);

    if (hero)
    {
        juce::ColourGradient halo(juce::Colour(0x88ffb52e), cx, cy,
                                  juce::Colour(0x00ffb52e), cx + radius * 1.65f, cy, true);
        g.setGradientFill(halo);
        g.fillEllipse(cx - radius * 1.45f, cy - radius * 1.45f,
                      radius * 2.90f, radius * 2.90f);
    }

    g.setColour(hero ? juce::Colour(0x88ffb43c) : juce::Colour(0x552b59aa));
    g.drawEllipse(cx - radius - 4.0f, cy - radius - 4.0f,
                  diameter + 8.0f, diameter + 8.0f,
                  hero ? 2.2f : 1.2f);

    juce::ColourGradient face(juce::Colour(0xff27324b),
                              cx - radius * 0.55f, cy - radius * 0.78f,
                              juce::Colour(0xff080d18),
                              cx + radius * 0.72f, cy + radius * 0.82f,
                              false);
    face.addColour(0.48, juce::Colour(0xff141d31));
    g.setGradientFill(face);
    g.fillEllipse(cx - radius, cy - radius, diameter, diameter);

    juce::Path bgArc;
    bgArc.addCentredArc(cx, cy, radius - 3.0f, radius - 3.0f, 0.0f,
                       rotaryStartAngle, rotaryEndAngle, true);
    g.setColour(juce::Colour(0xff29364f));
    g.strokePath(bgArc, juce::PathStrokeType(hero ? 5.2f : 3.5f,
                                             juce::PathStrokeType::curved,
                                             juce::PathStrokeType::rounded));

    juce::Path valueArc;
    valueArc.addCentredArc(cx, cy, radius - 3.0f, radius - 3.0f, 0.0f,
                          rotaryStartAngle, angle, true);

    juce::ColourGradient arc(activeA, cx - radius, cy,
                             activeB, cx + radius, cy, false);
    g.setGradientFill(arc);
    g.strokePath(valueArc, juce::PathStrokeType(hero ? 5.8f : 4.0f,
                                                juce::PathStrokeType::curved,
                                                juce::PathStrokeType::rounded));

    juce::Path pointer;
    const float pointerLength = radius * 0.64f;
    const float pointerThickness = hero ? 4.1f : 3.0f;
    pointer.addRoundedRectangle(-pointerThickness * 0.5f,
                                -pointerLength,
                                pointerThickness,
                                pointerLength * 0.56f,
                                2.0f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle).translated(cx, cy));
    g.setColour(juce::Colour(0xfff8fbff));
    g.fillPath(pointer);

    g.setColour(goldFamily ? juce::Colour(0x664f3611) : juce::Colour(0x5538c8ff));
    g.drawEllipse(cx - radius * 0.17f, cy - radius * 0.17f,
                  radius * 0.34f, radius * 0.34f, 1.0f);

    const int ledCount = hero ? 31 : 25;
    const float ledR = radius + (hero ? 8.0f : 6.0f);
    const float now = (float)(juce::Time::getMillisecondCounterHiRes() * 0.001);

    for (int i = 0; i < ledCount; ++i)
    {
        const float t = (float)i / (float)(ledCount - 1);
        const float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        const float lx = cx + std::sin(a) * ledR;
        const float ly = cy - std::cos(a) * ledR;
        const bool on = t <= sliderPos + 0.002f;

        const float distance = std::abs(t - sliderPos);
        const float nearPointer = juce::jlimit(0.0f, 1.0f,
                                              1.0f - distance * (hero ? 12.0f : 15.0f));
        const float pulse = 0.70f + 0.30f * std::sin(now * 5.3f - (float)i * 0.39f);
        const float dot = hero ? 4.1f : 3.2f;

        if (on)
        {
            const auto colour = activeA.interpolatedWith(activeB, t);
            g.setColour(colour.withAlpha(0.20f + 0.35f * nearPointer * pulse));
            g.fillEllipse(lx - dot * 1.05f, ly - dot * 1.05f,
                          dot * 2.10f, dot * 2.10f);

            g.setColour(colour.withAlpha(0.72f + 0.28f * nearPointer * pulse));
            g.fillEllipse(lx - dot * 0.50f, ly - dot * 0.50f, dot, dot);
        }
        else
        {
            g.setColour(juce::Colour(0x44374758));
            g.fillEllipse(lx - dot * 0.36f, ly - dot * 0.36f,
                          dot * 0.72f, dot * 0.72f);
        }
    }

    const float midAngle = rotaryStartAngle + 0.5f * (rotaryEndAngle - rotaryStartAngle);
    const float markerR = ledR;
    const float mx = cx + std::sin(midAngle) * markerR;
    const float my = cy - std::cos(midAngle) * markerR;

    g.setColour(juce::Colour(0xfff4f8ff));
    g.fillEllipse(mx - (hero ? 2.7f : 2.1f),
                  my - (hero ? 2.7f : 2.1f),
                  hero ? 5.4f : 4.2f,
                  hero ? 5.4f : 4.2f);
}

void MusicGoldLookAndFeel::drawToggleButton(juce::Graphics& g,
                                            juce::ToggleButton& b,
                                            bool over,
                                            bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced(1.0f);
    const bool on = b.getToggleState();

    auto fill = on ? juce::Colour(0xff8f5c13) : juce::Colour(0xff101727);

    if (b.getButtonText() == "AUTO" && on)
        fill = juce::Colour(0xff315a9c);
    if (b.getButtonText() == "SAFE" && on)
        fill = juce::Colour(0xff73551a);
    if (b.getButtonText() == "BYPASS" && on)
        fill = juce::Colour(0xff5e2732);

    if (over) fill = fill.brighter(0.10f);
    if (down) fill = fill.darker(0.10f);

    g.setColour(fill);
    g.fillRoundedRectangle(r, 8.0f);

    g.setColour(on ? juce::Colour(0xffffd477) : juce::Colour(0xff33405c));
    g.drawRoundedRectangle(r, 8.0f, on ? 1.8f : 1.0f);

    g.setColour(on ? juce::Colours::white : juce::Colour(0xffaab8ce));
    g.setFont(juce::FontOptions(juce::jlimit(10.0f, 15.0f, b.getHeight() * 0.36f),
                                juce::Font::bold));
    g.drawFittedText(b.getButtonText(), b.getLocalBounds(),
                     juce::Justification::centred, 1);
}

MusicEngeenAudioProcessorEditor::MusicEngeenAudioProcessorEditor(MusicEngeenAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setSize(980, 510);
    setResizable(true, true);
    setResizeLimits(760, 395, 1470, 765);

    if (auto* constrainer = getConstrainer())
        constrainer->setFixedAspectRatio(980.0 / 510.0);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        configureKnob(knobs[i], labels[i], names[i], i == 3);
        knobs[i].setName(names[i]);

        attachments[i] =
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processor.apvts, ids[i], knobs[i]);
    }

    configureKnob(subKnob, subLabel, "SUB");
    configureKnob(tiltKnob, tiltLabel, "TILT");
    configureKnob(mixKnob, mixLabel, "MIX");
    configureKnob(outputKnob, outputLabel, "OUTPUT");

    subKnob.setTextValueSuffix(" %");
    tiltKnob.setTextValueSuffix(" %");
    mixKnob.setTextValueSuffix(" %");
    outputKnob.setTextValueSuffix(" dB");

    subAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "sub", subKnob);
    tiltAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "tilt", tiltKnob);
    mixAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "mix", mixKnob);
    outputAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processor.apvts, "output", outputKnob);

    for (auto* b : { &autoButton, &safeButton, &bypassButton })
    {
        b->setClickingTogglesState(true);
        addAndMakeVisible(*b);
    }

    addAndMakeVisible(goldenButton);
    goldenButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff5c4215));
    goldenButton.setColour(juce::TextButton::textColourOffId, juce::Colour(0xffffdc83));

    autoAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, "auto", autoButton);
    safeAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, "safe", safeButton);
    bypassAttachment =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.apvts, "bypass", bypassButton);

    autoButton.onClick = [this]
    {
        if (autoButton.getToggleState())
            applyGoldenSettings();
    };

    goldenButton.onClick = [this] { applyGoldenSettings(); };

    startTimerHz(30);
}

MusicEngeenAudioProcessorEditor::~MusicEngeenAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel(nullptr);
}

void MusicEngeenAudioProcessorEditor::configureKnob(juce::Slider& k,
                                                     juce::Label& label,
                                                     const juce::String& name,
                                                     bool hero)
{
    k.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    k.setTextBoxStyle(juce::Slider::TextBoxBelow, false, hero ? 74 : 62, 19);
    k.setDoubleClickReturnValue(true, 50.0);
    k.setPopupDisplayEnabled(false, false, this);
    addAndMakeVisible(k);

    label.setText(name, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.setColour(juce::Label::textColourId,
                    hero ? juce::Colour(0xffffdc83) : juce::Colour(0xffd6e3f5));
    label.setFont(juce::FontOptions(hero ? 14.8f : 13.0f, juce::Font::bold));
    addAndMakeVisible(label);
}

void MusicEngeenAudioProcessorEditor::applyGoldenSettings()
{
    const std::array<float, 7> golden {
        48.0f, 46.0f, 50.0f, 58.0f, 38.0f, 46.0f, 34.0f
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

    setParam("sub", 42.0f);
    setParam("tilt", 50.0f);
    setParam("mix", 100.0f);
    setParam("output", 0.0f);

    if (auto* p = processor.apvts.getParameter("auto"))
        p->setValueNotifyingHost(1.0f);

    if (auto* p = processor.apvts.getParameter("safe"))
        p->setValueNotifyingHost(1.0f);
}

void MusicEngeenAudioProcessorEditor::updateSpectrum()
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

            const double lowHz = 35.0;
            const double highHz = juce::jmin(18000.0, sr * 0.46);

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
                    juce::jlimit(-80.0f, -8.0f, db),
                    -80.0f, -8.0f, 0.0f, 1.0f);

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
        return target > current
            ? 0.58f * current + 0.42f * target
            : current * 0.90f;
    };

    smoothInL = smoothMeter(smoothInL, processor.getInputMeter(0));
    smoothInR = smoothMeter(smoothInR, processor.getInputMeter(1));
    smoothOutL = smoothMeter(smoothOutL, processor.getOutputMeter(0));
    smoothOutR = smoothMeter(smoothOutR, processor.getOutputMeter(1));
}

void MusicEngeenAudioProcessorEditor::drawSpectrumAndMeters(juce::Graphics& g)
{
    const juce::Rectangle<float> scope(236.0f, 381.0f, 508.0f, 58.0f);

    g.setColour(juce::Colour(0x52101625));
    g.fillRoundedRectangle(scope, 12.0f);

    g.setColour(juce::Colour(0x706f5320));
    g.drawRoundedRectangle(scope, 12.0f, 1.0f);

    const float gap = 2.0f;
    const float bandW =
        (scope.getWidth() - 18.0f - gap * (spectrumBands - 1))
        / (float)spectrumBands;

    const float baseY = scope.getBottom() - 8.0f;

    juce::Path line;

    for (int i = 0; i < spectrumBands; ++i)
    {
        const float v = juce::jlimit(0.0f, 1.0f, spectrumValues[(size_t)i]);
        const float x = scope.getX() + 9.0f + (bandW + gap) * (float)i;
        const float h = 2.0f + v * 34.0f;
        const float y = baseY - h;

        juce::ColourGradient bar(juce::Colour(0xffffc857), x, baseY,
                                 juce::Colour(0xff42bfff), x, y, false);
        g.setGradientFill(bar);
        g.fillRoundedRectangle(x, y, juce::jmax(1.0f, bandW), h, 1.0f);

        const float px = x + bandW * 0.5f;
        const float py = y - 1.5f;

        if (i == 0) line.startNewSubPath(px, py);
        else        line.lineTo(px, py);
    }

    g.setColour(juce::Colour(0x80ffb950));
    g.strokePath(line, juce::PathStrokeType(4.0f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xffffedbd));
    g.strokePath(line, juce::PathStrokeType(1.0f,
                                            juce::PathStrokeType::curved,
                                            juce::PathStrokeType::rounded));

    g.setColour(juce::Colour(0xffb89b62));
    g.setFont(juce::FontOptions(8.8f, juce::Font::bold));
    g.drawText("MUSIC BUS SPECTRUM", 382, 383, 216, 11,
               juce::Justification::centred);

    const auto drawMeter = [&g](float x, float y, float w,
                                float l, float r,
                                const juce::String& label)
    {
        g.setColour(juce::Colour(0xff9a845c));
        g.setFont(juce::FontOptions(7.8f, juce::Font::bold));
        g.drawText(label, (int)x, (int)(y - 12.0f),
                   (int)w, 10, juce::Justification::centred);

        const int segments = 12;
        const float segGap = 1.4f;
        const float segW = (w - segGap * (segments - 1)) / (float)segments;

        for (int row = 0; row < 2; ++row)
        {
            const float value =
                juce::jlimit(0.0f, 1.0f, std::sqrt(row == 0 ? l : r));

            const int lit = juce::roundToInt(value * (float)segments);

            for (int i = 0; i < segments; ++i)
            {
                const float sx = x + (segW + segGap) * (float)i;
                const bool on = i < lit;

                g.setColour(on
                    ? juce::Colour(0xff43bfff).interpolatedWith(
                        juce::Colour(0xffffbd50),
                        (float)i / (float)(segments - 1))
                    : juce::Colour(0x242b3852));

                g.fillRoundedRectangle(sx, y + row * 7.0f,
                                       segW, 4.0f, 1.0f);
            }
        }
    };

    drawMeter(247.0f, 405.0f, 66.0f, smoothInL, smoothInR, "IN");
    drawMeter(667.0f, 405.0f, 66.0f, smoothOutL, smoothOutR, "OUT");
}

void MusicEngeenAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff050811));

    const float scale = (float)getWidth() / 980.0f;
    juce::Graphics::ScopedSaveState saved(g);
    g.addTransform(juce::AffineTransform::scale(scale));

    juce::ColourGradient bg(juce::Colour(0xff050811), 0.0f, 0.0f,
                            juce::Colour(0xff101728), 980.0f, 510.0f, false);
    bg.addColour(0.45, juce::Colour(0xff081121));
    g.setGradientFill(bg);
    g.fillRect(0.0f, 0.0f, 980.0f, 510.0f);

    const float signalEnergy =
        juce::jlimit(0.0f, 1.0f, std::sqrt(juce::jmax(smoothOutL, smoothOutR)));

    const float pulse =
        0.70f
        + 0.16f * std::sin(juce::MathConstants<float>::twoPi * animationPhase)
        + 0.22f * signalEnergy;

    juce::ColourGradient blueHalo(
        juce::Colour::fromFloatRGBA(0.12f, 0.38f, 0.90f, 0.33f * pulse),
        360.0f, 195.0f,
        juce::Colour::fromFloatRGBA(0.04f, 0.10f, 0.26f, 0.0f),
        720.0f, 220.0f,
        true);
    g.setGradientFill(blueHalo);
    g.fillEllipse(50.0f, 35.0f, 700.0f, 390.0f);

    juce::ColourGradient goldHalo(
        juce::Colour::fromFloatRGBA(1.0f, 0.62f, 0.13f, 0.26f * pulse),
        515.0f, 220.0f,
        juce::Colour::fromFloatRGBA(0.32f, 0.16f, 0.03f, 0.0f),
        845.0f, 240.0f,
        true);
    g.setGradientFill(goldHalo);
    g.fillEllipse(275.0f, 70.0f, 640.0f, 345.0f);

    for (int i = 0; i < 88; ++i)
    {
        const int sx = (i * 137 + 41) % 980;
        const int sy = (i * 83 + 27) % 370;

        const float twinkle =
            0.5f + 0.5f * std::sin(
                juce::MathConstants<float>::twoPi
                * (animationPhase + 0.071f * (float)i));

        const float alpha = 0.12f + 0.68f * twinkle;
        const float rr = (i % 11 == 0) ? 2.1f
                        : ((i % 5 == 0) ? 1.4f : 0.8f);

        const auto star = (i % 3 == 0)
            ? juce::Colour::fromFloatRGBA(1.0f, 0.79f, 0.38f, alpha)
            : juce::Colour::fromFloatRGBA(0.55f, 0.78f, 1.0f, alpha);

        g.setColour(star);
        g.fillEllipse((float)sx, (float)sy, rr, rr);
    }

    for (int i = 0; i < 5; ++i)
    {
        juce::Path arc;
        const float inset = (float)i * 8.0f;

        arc.addCentredArc(510.0f, 180.0f,
                          345.0f - inset,
                          135.0f - inset * 0.34f,
                          0.0f,
                          3.92f, 5.45f, true);

        g.setColour(juce::Colour::fromFloatRGBA(
            1.0f, 0.62f + i * 0.04f, 0.18f,
            (0.23f - i * 0.03f) * pulse));

        g.strokePath(arc,
                     juce::PathStrokeType(i == 0 ? 2.1f : 1.0f,
                                          juce::PathStrokeType::curved,
                                          juce::PathStrokeType::rounded));
    }

    g.setColour(juce::Colour(0x5010192b));
    g.fillRoundedRectangle(24.0f, 92.0f, 932.0f, 278.0f, 18.0f);

    g.setColour(juce::Colour(0x707b622d));
    g.drawRoundedRectangle(24.0f, 92.0f, 932.0f, 278.0f, 18.0f, 1.0f);

    g.setColour(juce::Colour(0xfffff3d5));
    g.setFont(juce::FontOptions(29.0f, juce::Font::bold));
    g.drawText("MUSIC ENGEEN", 260, 20, 460, 38,
               juce::Justification::centred);

    g.setColour(juce::Colour(0xffb89c66));
    g.setFont(juce::FontOptions(11.0f));
    g.drawText("BLUE GOLD MUSIC BUS PROCESSOR  |  GOLDEN BUS",
               270, 57, 440, 18,
               juce::Justification::centred);

    g.setColour(juce::Colour(0xff8ea6c9));
    g.setFont(juce::FontOptions(10.2f, juce::Font::bold));
    g.drawText("CLEAN  |  WEIGHT  |  PUNCH  |  GLUE  |  SHINE  |  WIDTH  |  LOUD",
               430, 75, 500, 16,
               juce::Justification::centredRight);

    drawSpectrumAndMeters(g);

    g.setColour(juce::Colour(0xff796b4d));
    g.setFont(juce::FontOptions(9.6f));
    g.drawText("AUTO = GOLDEN START + ADAPTIVE PROTECTION   |   SAFE = GUARDS LOW / WIDTH / LOUD",
               180, 478, 620, 16,
               juce::Justification::centred);

    g.setColour(juce::Colour(0xff5e6f8b));
    g.setFont(juce::FontOptions(9.5f));
    g.drawText("KG MUSIC RECORDS  |  Music Engeen VST3 v0.1",
               32, 486, 300, 15,
               juce::Justification::centredLeft);
}

void MusicEngeenAudioProcessorEditor::resized()
{
    const float s = (float)getWidth() / 980.0f;

    const auto R = [s](float x, float y, float w, float h)
    {
        return juce::Rectangle<int>(
            juce::roundToInt(x * s),
            juce::roundToInt(y * s),
            juce::roundToInt(w * s),
            juce::roundToInt(h * s));
    };

    const int mainY = 122;
    const int standardW = 112;
    const int heroW = 140;
    const std::array<int, 7> xs { 35, 160, 285, 408, 552, 677, 802 };

    for (int i = 0; i < 7; ++i)
    {
        const bool hero = i == 3;
        const int w = hero ? heroW : standardW;
        const int x = xs[(size_t)i] - (hero ? 14 : 0);

        labels[(size_t)i].setBounds(
            R((float)x, (float)mainY, (float)w, 24.0f));

        labels[(size_t)i].setFont(
            juce::FontOptions((hero ? 14.8f : 13.0f) * s,
                              juce::Font::bold));

        knobs[(size_t)i].setBounds(
            R((float)x, (float)(mainY + 22),
              (float)w, hero ? 166.0f : 146.0f));

        knobs[(size_t)i].setTextBoxStyle(
            juce::Slider::TextBoxBelow, false,
            juce::roundToInt((hero ? 74.0f : 62.0f) * s),
            juce::roundToInt(19.0f * s));
    }

    subLabel.setBounds(R(36, 382, 82, 18));
    subKnob.setBounds(R(36, 398, 82, 72));

    tiltLabel.setBounds(R(135, 382, 82, 18));
    tiltKnob.setBounds(R(135, 398, 82, 72));

    mixLabel.setBounds(R(764, 382, 82, 18));
    mixKnob.setBounds(R(764, 398, 82, 72));

    outputLabel.setBounds(R(862, 382, 82, 18));
    outputKnob.setBounds(R(862, 398, 82, 72));

    for (auto* label : { &subLabel, &tiltLabel, &mixLabel, &outputLabel })
        label->setFont(juce::FontOptions(11.5f * s, juce::Font::bold));

    for (auto* knob : { &subKnob, &tiltKnob, &mixKnob, &outputKnob })
        knob->setTextBoxStyle(
            juce::Slider::TextBoxBelow, false,
            juce::roundToInt(64.0f * s),
            juce::roundToInt(18.0f * s));

    autoButton.setBounds(R(304, 444, 78, 28));
    goldenButton.setBounds(R(392, 444, 92, 28));
    safeButton.setBounds(R(494, 444, 78, 28));
    bypassButton.setBounds(R(582, 444, 92, 28));
}

void MusicEngeenAudioProcessorEditor::timerCallback()
{
    updateSpectrum();

    animationPhase += 0.018f;

    if (animationPhase > 1.0f)
        animationPhase -= 1.0f;

    repaint();
}
