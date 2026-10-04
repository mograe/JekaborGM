#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <fluidsynth.h>
#include <array>

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

    bool loadSoundFont (const juce::File& file);
    juce::String getSoundFontStatus() const;
    struct Instrument
    {
        int bank = 0;
        int program = 0;
        juce::String name;
    };
    juce::Array<Instrument> getInstruments() const;
    Instrument getChannelInstrument (int channel) const;
    void selectInstrument (int channel, int bank, int program);

private:
    void handleMidiMessage (const juce::MidiMessage& message);
    void renderAudio (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    static juce::File findSoundFont();
    void applyChannelInstruments();
    std::array<Instrument, 16> channelInstruments {};

    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;
    juce::File loadedSoundFont;
    int soundFontId = -1;
    double synthSampleRate = 44100.0;
    juce::String soundFontStatus { "SoundFont not loaded. Select an .sf2 file." };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JekaborGMAudioProcessor)
};
