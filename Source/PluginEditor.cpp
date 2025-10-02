//EgoA DSP FX Papa's Fuzz Ball
//Daniel Allen Rinker 2025 daniel.rinker@protonmail.ch

#include "BinaryData.h"
#include "PluginEditor.h"
#include "PluginProcessor.h"
#include <cmath>

// Knob look
void PedalLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                           juce::Slider& slider)
{
    auto area = juce::Rectangle<float>(x, y, width, height).reduced (width * 0.10f);
    auto radius = juce::jmin (area.getWidth(), area.getHeight()) * 0.5f;
    auto centre = area.getCentre();

    g.setGradientFill (juce::ColourGradient (juce::Colour(0xff2b2b2b), centre.x, centre.y,
                                             juce::Colour(0xff0f0f10), centre.x, centre.y - radius, true));
    g.fillEllipse (area);
    g.setColour (juce::Colours::black.withAlpha (0.9f));
    g.drawEllipse (area, 2.0f);

    g.setColour (juce::Colours::dimgrey);
    const int ticks = 16;
    for (int i = 0; i < ticks; ++i)
    {
        float t = (float) i / (ticks - 1);
        float a = rotaryStartAngle + t * (rotaryEndAngle - rotaryStartAngle);
        auto p1 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * (radius * 0.82f);
        auto p2 = centre + juce::Point<float>(std::cos(a), std::sin(a)) * (radius * 0.95f);
        g.drawLine (juce::Line<float> (p1, p2), (i % 4 == 0) ? 2.0f : 1.0f);
    }

    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
    auto p1 = centre + juce::Point<float>(std::cos (angle), std::sin (angle)) * (radius * 0.55f);
    auto p2 = centre + juce::Point<float>(std::cos (angle), std::sin (angle)) * (radius * 0.95f);
    g.setColour (juce::Colours::white);
    g.drawLine (juce::Line<float> (p1, p2), 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.fillEllipse (area.withSizeKeepingCentre (area.getWidth()*0.8f, area.getHeight()*0.8f).translated (-radius*0.08f, -radius*0.12f));
}

// Unused now (no footswitch in UI), kept for completeness
void PedalLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool, bool)
{
    auto r = b.getLocalBounds().toFloat();
    g.setColour (juce::Colours::darkgrey);
    g.fillRoundedRectangle (r, r.getHeight()*0.2f);
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.drawRoundedRectangle (r, r.getHeight()*0.2f, 1.5f);
}
void PedalLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setColour (juce::Colours::white);
    g.setFont (juce::Font (12.0f, juce::Font::bold));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

namespace { constexpr int UI_W = 500; constexpr int UI_H = 700; }

StompCrushAudioProcessorEditor::StompCrushAudioProcessorEditor (StompCrushAudioProcessor& p)
: AudioProcessorEditor (&p), processor (p)
{
    setResizable (false, false);
    setLookAndFeel (&lnf);
    setSize (UI_W, UI_H);

    if (auto* data = BinaryData::guibg_png; true)
        background = juce::ImageCache::getFromMemory (data, BinaryData::guibg_pngSize);

    auto& apvts = processor.apvts;

    auto rotary = [this](juce::Slider& s, const juce::String& name) {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 64, 18);
        s.setName (name); s.setLookAndFeel (&lnf);
    };

    // Knobs
    rotary (gainSlider, "Gain");        addAndMakeVisible (gainSlider);
    gainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "gainDb", gainSlider);

    rotary (bitSlider, "Bits");         bitSlider.setRange (4, 16, 1); addAndMakeVisible (bitSlider);
    bitAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "bitDepth", bitSlider);

    rotary (dsSlider, "Downsample");    dsSlider.setRange (1, 16, 1); addAndMakeVisible (dsSlider);
    dsAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "downsample", dsSlider);

    rotary (cutoffSlider, "LPF");       cutoffSlider.setSkewFactor (0.5f); addAndMakeVisible (cutoffSlider);
    cutoffAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "cutoffHz", cutoffSlider);

    rotary (wetSlider, "Mix");          addAndMakeVisible (wetSlider);
    wetAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "wet", wetSlider);

    rotary (trimSlider, "Trim");        addAndMakeVisible (trimSlider);
    trimAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "outTrimDb", trimSlider);

    rotary (sustainSlider, "Sustain");  addAndMakeVisible (sustainSlider);
    sustainAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, "sustain", sustainSlider);

    // Switches / Menus
    octaveBox.addItem ("Down", 1); octaveBox.addItem ("Off", 2); octaveBox.addItem ("Up", 3);
    addAndMakeVisible (octaveBox);
    octAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "octaveMode", octaveBox);

    // Preset menu (GUI-only) — moved to top-right, no label
    presetBox.addItem ("Init",       1);
    presetBox.addItem ("Warm Fuzz",  2);
    presetBox.addItem ("8-bit Lead", 3);
    presetBox.addItem ("Doom Bass",  4);
    presetBox.addItem ("Lo-Fi Pad",  5);
    presetBox.onChange = [this]{ applyPreset (presetBox.getSelectedId()); };
    presetBox.setSelectedId (1, juce::dontSendNotification);
    addAndMakeVisible (presetBox);
}

void StompCrushAudioProcessorEditor::drawOutlinedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                           juce::Justification just, juce::Colour fill, juce::Colour outline, int outlinePx, const juce::Font& font)
{
    g.setFont (font);
    g.setColour (outline);
    for (int dx = -outlinePx; dx <= outlinePx; ++dx)
        for (int dy = -outlinePx; dy <= outlinePx; ++dy)
            if (dx != 0 || dy != 0)
                g.drawText (text, area.translated (dx, dy), just, false);
    g.setColour (fill);
    g.drawText (text, area, just, false);
}

void StompCrushAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::black);
    if (background.isValid())
        g.drawImageWithin (background, 0, 0, getWidth(), getHeight(), juce::RectanglePlacement::stretchToFit);

    // Title + footer branding
    juce::Font title ("Helvetica", 26.0f, juce::Font::bold);
    drawOutlinedText (g, "papa's fuzz ball", {12, 10, getWidth()/2, 34},
                      juce::Justification::topLeft, juce::Colours::white, juce::Colours::black, 2, title);

    juce::Font foot ("Helvetica", 16.0f, juce::Font::bold);
    drawOutlinedText (g, "EgoA FX", {12, getHeight()-28, getWidth()/3, 22},
                      juce::Justification::bottomLeft, juce::Colours::white, juce::Colours::black, 2, foot);
    drawOutlinedText (g, "egoagnosia", {getWidth()-getWidth()/3 - 12, getHeight()-28, getWidth()/3, 22},
                      juce::Justification::bottomRight, juce::Colours::white, juce::Colours::black, 2, foot);

    // Labels
    juce::Font labelF ("Helvetica", 15.0f, juce::Font::bold);
    auto knobLabel = [&](const juce::String& text, juce::Component& c)
    {
        auto r = c.getBounds();
        drawOutlinedText (g, text, { r.getX(), r.getBottom()+2, r.getWidth(), 20 },
                          juce::Justification::centredTop, juce::Colours::white, juce::Colours::black, 2, labelF);
    };
    knobLabel ("Gain",       gainSlider);
    knobLabel ("Bits",       bitSlider);
    knobLabel ("Downsample", dsSlider);
    knobLabel ("Octave",     octaveBox);
    knobLabel ("LPF",        cutoffSlider);
    knobLabel ("Mix",        wetSlider);
    knobLabel ("Trim",       trimSlider);
    knobLabel ("Sustain",    sustainSlider);

    // (Preset label removed; dropdown alone in top-right)
}

void StompCrushAudioProcessorEditor::placeKnob(juce::Component& c, float cx, float cy, int d)
{
    c.setBounds (int(cx - d*0.5f), int(cy - d*0.5f), d, d);
}

void StompCrushAudioProcessorEditor::resized()
{
    auto W = (float)getWidth(); auto H = (float)getHeight();
    int D = int (0.18f * juce::jmin(W, H));

    // Keep the face area clear: knobs to the sides/top
    placeKnob (gainSlider,   0.20f * W, 0.22f * H, D);
    placeKnob (bitSlider,    0.80f * W, 0.22f * H, D);
    placeKnob (dsSlider,     0.20f * W, 0.38f * H, D);
    placeKnob (cutoffSlider, 0.80f * W, 0.38f * H, D);
    placeKnob (wetSlider,    0.20f * W, 0.54f * H, D);
    placeKnob (trimSlider,   0.80f * W, 0.54f * H, D);

    // Sustain centered below
    placeKnob (sustainSlider, 0.50f * W, 0.70f * H, D);

    int comboW = int (D * 1.2f), comboH = int (D * 0.5f);
    octaveBox.setBounds (int(0.5f * W - comboW/2), int(0.86f * H - comboH/2), comboW, comboH);

    // Preset menu top-right (no label)
    int presetW = int(comboW * 1.2f), presetH = comboH;
    int margin = 12;
    presetBox.setBounds (int(W) - presetW - margin, margin, presetW, presetH);
}

void StompCrushAudioProcessorEditor::applyPreset (int id)
{
    // Helper to set params by real value (not normalised)
    auto setParam = [this](const juce::String& pid, float realValue)
    {
        if (auto* p = processor.apvts.getParameter (pid))
        {
            if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p))
            {
                const float norm = rp->convertTo0to1 (realValue);
                rp->beginChangeGesture();
                rp->setValueNotifyingHost (norm);
                rp->endChangeGesture();
            }
        }
    };

    // For the octave choice param, use the index via Value for reliability (0=Down,1=Off,2=Up)
    auto setOct = [this](int index0Based)
    {
        processor.apvts.getParameterAsValue ("octaveMode") = index0Based;
    };

    switch (id)
    {
        case 2: // Warm Fuzz
            setParam ("gainDb",     6.0f);
            setParam ("bitDepth",   8.0f);
            setParam ("downsample", 3.0f);
            setOct (1); // Off
            setParam ("cutoffHz",   9000.0f);
            setParam ("wet",        100.0f);
            setParam ("outTrimDb",  0.0f);
            setParam ("sustain",    60.0f);
            break;

        case 3: // 8-bit Lead
            setParam ("gainDb",     9.0f);
            setParam ("bitDepth",   6.0f);
            setParam ("downsample", 5.0f);
            setOct (2); // Up
            setParam ("cutoffHz",   8000.0f);
            setParam ("wet",        100.0f);
            setParam ("outTrimDb", -3.0f);
            setParam ("sustain",    40.0f);
            break;

        case 4: // Doom Bass
            setParam ("gainDb",     12.0f);
            setParam ("bitDepth",   7.0f);
            setParam ("downsample", 3.0f);
            setOct (0); // Down
            setParam ("cutoffHz",   5000.0f);
            setParam ("wet",        100.0f);
            setParam ("outTrimDb", -6.0f);
            setParam ("sustain",    70.0f);
            break;

        case 5: // Lo-Fi Pad
            setParam ("gainDb",     3.0f);
            setParam ("bitDepth",   8.0f);
            setParam ("downsample", 8.0f);
            setOct (1); // Off
            setParam ("cutoffHz",   6000.0f);
            setParam ("wet",        70.0f);
            setParam ("outTrimDb",  0.0f);
            setParam ("sustain",    30.0f);
            break;

        default: // Init
            break;
    }
}

