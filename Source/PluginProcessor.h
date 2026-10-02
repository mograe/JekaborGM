#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <fluidsynth.h>

class JekaborGMAudioProcessor final : public juce::AudioProcessor
{
public:
    JekaborGMAudioProcessor();
    ~JekaborGMAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    void handleMidiMessage (const juce::MidiMessage& message);
    void renderAudio (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    // Temporary path for the first milestone. The plugin still compiles and
    // runs if this file is missing.
    static constexpr const char* soundFontPath = "/usr/share/sounds/sf2/FluidR3_GM.sf2";

    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JekaborGMAudioProcessor)
};
