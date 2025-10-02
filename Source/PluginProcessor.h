//EgoA DSP FX Papa's Fuzz Ball
//Daniel Allen Rinker 2025 daniel.rinker@protonmail.ch
#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

class StompCrushAudioProcessor  : public juce::AudioProcessor
{
public:
    StompCrushAudioProcessor();
    ~StompCrushAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Papa Fuzz"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Keep a proper host bypass param (even if not shown in the UI)
    juce::AudioProcessorParameter* getBypassParameter() const override { return bypassParam; }

    juce::AudioProcessorValueTreeState apvts;

private:
    // Parameter IDs
    static constexpr auto PID_GAIN_DB    = "gainDb";
    static constexpr auto PID_BITS       = "bitDepth";
    static constexpr auto PID_DOWNSAMPLE = "downsample";
    static constexpr auto PID_OCTAVE     = "octaveMode";
    static constexpr auto PID_CUTOFF     = "cutoffHz";
    static constexpr auto PID_WET        = "wet";
    static constexpr auto PID_TRIM_DB    = "outTrimDb";
    static constexpr auto PID_SUSTAIN    = "sustain";
    static constexpr auto PID_BYPASS     = "bypass";

    // DSP
    juce::dsp::Compressor<float> compressor;
    juce::dsp::StateVariableTPTFilter<float> lowpass;
    juce::dsp::ProcessSpec spec {};

    std::vector<int>   dsCounters;
    std::vector<float> dsHold;

    struct OctState { float lastSample = 0.0f; int zeroCrossCount = 0; int flip = 1; float env = 0.0f; };
    std::vector<OctState> oct;

    juce::AudioBuffer<float> dryBuffer;
    juce::AudioParameterBool* bypassParam = nullptr;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    static inline float dbToGain (float db) { return juce::Decibels::decibelsToGain (db); }

    float lightSaturate (float x) noexcept;
    float crushSample (float x, int bits) noexcept;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StompCrushAudioProcessor)
};

