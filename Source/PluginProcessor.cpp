//EgoA DSP FX Papa's Fuzz Ball
//Daniel Allen Rinker 2025 daniel.rinker@protonmail.ch
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

StompCrushAudioProcessor::StompCrushAudioProcessor()
: AudioProcessor (BusesProperties().withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                                    .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
  apvts (*this, nullptr, "PARAMS", createLayout())
{
    bypassParam = dynamic_cast<juce::AudioParameterBool*>(apvts.getParameter (PID_BYPASS));
}

juce::AudioProcessorValueTreeState::ParameterLayout StompCrushAudioProcessor::createLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.push_back (std::make_unique<AudioParameterFloat>(PID_GAIN_DB, "Gain (dB)",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f, 1.0f), 6.0f));

    params.push_back (std::make_unique<AudioParameterInt>(PID_BITS, "Bit Depth", 4, 16, 6));
    params.push_back (std::make_unique<AudioParameterInt>(PID_DOWNSAMPLE, "Downsample", 1, 16, 4));

    StringArray octChoices { "Down", "Off", "Up" };
    params.push_back (std::make_unique<AudioParameterChoice>(PID_OCTAVE, "Octave", octChoices, 1));

    params.push_back (std::make_unique<AudioParameterFloat>(PID_CUTOFF, "LPF Cutoff (Hz)",
        NormalisableRange<float> (500.0f, 20000.0f, 1.0f, 0.25f), 8000.0f));

    params.push_back (std::make_unique<AudioParameterFloat>(PID_WET, "Mix (%)",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f, 1.0f), 100.0f));

    params.push_back (std::make_unique<AudioParameterFloat>(PID_TRIM_DB, "Output Trim (dB)",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f, 1.0f), 0.0f));

    // New: Sustain (0..100), controls compressor amount
    params.push_back (std::make_unique<AudioParameterFloat>(PID_SUSTAIN, "Sustain",
        NormalisableRange<float> (0.0f, 100.0f, 0.01f, 1.0f), 60.0f));

    // Host bypass (not shown in UI)
    params.push_back (std::make_unique<AudioParameterBool>(PID_BYPASS, "Bypass", false));

    return { params.begin(), params.end() };
}

bool StompCrushAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto mainIn  = layouts.getMainInputChannelSet();
    const auto mainOut = layouts.getMainOutputChannelSet();
    if (mainIn != mainOut) return false;
    if (mainIn.isDisabled() || (mainIn.size() > 2)) return false;
    return true;
}

void StompCrushAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = (juce::uint32) getTotalNumOutputChannels();

    compressor.reset();
    compressor.prepare (spec);
    compressor.setThreshold (-18.0f);
    compressor.setRatio (3.0f);
    compressor.setAttack (5.0f);
    compressor.setRelease (80.0f);

    lowpass.reset();
    lowpass.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    lowpass.prepare (spec);

    const int ch = getTotalNumOutputChannels();
    dsCounters.assign (ch, 0);
    dsHold.assign (ch, 0.0f);
    oct.assign (ch, {});

    dryBuffer.setSize (ch, samplesPerBlock);
}

float StompCrushAudioProcessor::lightSaturate (float x) noexcept
{
    const float drive = 1.7f;
    const float y = std::tanh (drive * x);
    const float makeup = 1.15f;
    return makeup * y;
}

float StompCrushAudioProcessor::crushSample (float x, int bits) noexcept
{
    const int maxSteps = (1 << (bits - 1)) - 1;
    const float clamped = juce::jlimit (-1.0f, 1.0f, x);
    return std::round (clamped * maxSteps) / (float) maxSteps;
}

void StompCrushAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int numCh = buffer.getNumChannels();
    const int numSmps = buffer.getNumSamples();

    if ((int)dsCounters.size() != numCh) { dsCounters.assign (numCh, 0); dsHold.assign (numCh, 0.0f); oct.assign (numCh, {}); }
    if (dryBuffer.getNumChannels() != numCh || dryBuffer.getNumSamples() < numSmps)
        dryBuffer.setSize (numCh, numSmps, false, false, true);

    dryBuffer.makeCopyOf (buffer);

    auto* bypass    = apvts.getRawParameterValue (PID_BYPASS);
    auto* gainDb    = apvts.getRawParameterValue (PID_GAIN_DB);
    auto* bitsParam = apvts.getRawParameterValue (PID_BITS);
    auto* dsParam   = apvts.getRawParameterValue (PID_DOWNSAMPLE);
    auto* octParam  = apvts.getRawParameterValue (PID_OCTAVE);
    auto* cutoffHz  = apvts.getRawParameterValue (PID_CUTOFF);
    auto* wetPct    = apvts.getRawParameterValue (PID_WET);
    auto* trimDb    = apvts.getRawParameterValue (PID_TRIM_DB);
    auto* sustain   = apvts.getRawParameterValue (PID_SUSTAIN);

    if (bypass->load() >= 0.5f)
    {
        for (int ch = 0; ch < numCh; ++ch)
            buffer.copyFrom (ch, 0, dryBuffer, ch, 0, numSmps);
        return;
    }

    // Sustain -> compressor settings
    const float s = sustain->load(); // 0..100
    const float thr = juce::jmap (s, 0.0f, 100.0f, -12.0f, -30.0f);
    const float rat = juce::jmap (s, 0.0f, 100.0f,   2.0f,   6.0f);
    compressor.setThreshold (thr);
    compressor.setRatio (rat);

    const float linGain = dbToGain (gainDb->load());
    const int bits = juce::jlimit (4, 24, (int) std::lrint (bitsParam->load()));
    const int dsN  = juce::jmax (1, (int) std::lrint (dsParam->load()));
    const int octModeIndex = (int) std::lrint (octParam->load()); // 0=Down,1=Off,2=Up
    const int octMode = (octModeIndex == 0 ? -1 : (octModeIndex == 2 ? +1 : 0));
    const float wet = juce::jlimit (0.0f, 1.0f, (wetPct->load()) / 100.0f);
    const float dry = 1.0f - wet;
    const float outGain = dbToGain (trimDb->load());

    lowpass.setCutoffFrequency (cutoffHz->load());

    // 0) Gain
    for (int ch = 0; ch < numCh; ++ch)
        buffer.applyGain (ch, 0, numSmps, linGain);

    // 1) Light Saturation
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSmps; ++i)
            data[i] = lightSaturate (data[i]);
    }

    // 2) Compression
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        compressor.process (ctx);
    }

    // 3) Bitcrusher with +6 dB pre-drive
    const float crushDrive = juce::Decibels::decibelsToGain (6.0f);
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* data = buffer.getWritePointer (ch);
        auto& counter = dsCounters[(size_t) ch];
        auto& hold    = dsHold[(size_t) ch];

        for (int i = 0; i < numSmps; ++i)
        {
            if (counter == 0)
            {
                const float driven = data[i] * crushDrive;
                hold = crushSample (driven, bits);
            }
            data[i] = hold;
            if (++counter >= dsN) counter = 0;
        }
    }

    // 4) Octave
    if (octMode != 0)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& st = oct[(size_t) ch];

            for (int i = 0; i < numSmps; ++i)
            {
                const float x = data[i];

                if (octMode > 0) // up
                {
                    float y = std::abs (x);
                    y = (2.0f * y) - 1.0f;
                    data[i] = juce::jlimit (-1.0f, 1.0f, y);
                }
                else              // down
                {
                    const bool crossed = ((x >= 0.0f && st.lastSample < 0.0f) ||
                                          (x <  0.0f && st.lastSample >= 0.0f));
                    if (crossed)
                    {
                        st.zeroCrossCount++;
                        if (st.zeroCrossCount >= 2) { st.flip = -st.flip; st.zeroCrossCount = 0; }
                    }
                    st.lastSample = x;

                    const float targetEnv = std::abs (x);
                    const float atk = 0.01f, rel = 0.001f;
                    const float coeff = (targetEnv > st.env ? atk : rel);
                    st.env = (1.0f - coeff) * st.env + coeff * targetEnv;

                    data[i] = juce::jlimit (-1.0f, 1.0f, (float) st.flip * st.env);
                }
            }
        }
    }

    // 5) LPF
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        lowpass.process (ctx);
    }

    // Wet/Dry
    if (wet < 1.0f)
    {
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* wetPtr = buffer.getWritePointer (ch);
            const auto* dryPtr = dryBuffer.getReadPointer (ch);
            for (int i = 0; i < numSmps; ++i)
                wetPtr[i] = wetPtr[i] * wet + dryPtr[i] * dry;
        }
    }

    // Output
    buffer.applyGain (outGain);
}

juce::AudioProcessorEditor* StompCrushAudioProcessor::createEditor()
{
    return new StompCrushAudioProcessorEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new StompCrushAudioProcessor();
}

void StompCrushAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream stream (destData, true);
    apvts.state.writeToStream (stream);
}

void StompCrushAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = juce::ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        apvts.replaceState (tree);
}

