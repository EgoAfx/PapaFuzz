//EgoA DSP FX Papa's Fuzz Ball
//Daniel Allen Rinker 2025 daniel.rinker@protonmail.ch
#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "PluginProcessor.h"

class PedalLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, const float rotaryStartAngle, const float rotaryEndAngle,
                           juce::Slider& slider) override;
    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& bg, bool isHighlighted, bool isDown) override;
    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool isHighlighted, bool isDown) override;
};

class StompCrushAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    explicit StompCrushAudioProcessorEditor (StompCrushAudioProcessor&);
    ~StompCrushAudioProcessorEditor() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    StompCrushAudioProcessor& processor;
    PedalLookAndFeel lnf;

    juce::Image background;

    // Controls
    juce::Slider gainSlider, bitSlider, dsSlider, cutoffSlider, wetSlider, trimSlider, sustainSlider;
    juce::ComboBox octaveBox, presetBox;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> gainAtt, bitAtt, dsAtt, cutoffAtt, wetAtt, trimAtt, sustainAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> octAtt;

    void placeKnob(juce::Component& c, float cx, float cy, int d);
    void drawOutlinedText (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                           juce::Justification just, juce::Colour fill, juce::Colour outline, int outlinePx, const juce::Font& font);

    void applyPreset (int presetId);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StompCrushAudioProcessorEditor)
};

