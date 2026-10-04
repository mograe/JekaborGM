#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <fluidsynth.h>
#include <array>
#include <atomic>

class JekaborGMAudioProcessor final : public juce::AudioProcessor
{
public:
    JekaborGMAudioProcessor();
    ~JekaborGMAudioProcessor() override;
    void prepareToPlay (double, int) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }
    const juce::String getName() const override { return JucePlugin_Name; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 10.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;
    struct Instrument { int bank = 0, program = 0; juce::String name; };
    bool loadSoundFont (const juce::File&);
    juce::String getSoundFontStatus() const;
    juce::String getSoundFontName() const;
    juce::Array<Instrument> getInstruments() const;
    Instrument getChannelInstrument (int) const;
    void selectInstrument (int channel, int bank, int program);
    static juce::String channelID (const juce::String& base, int channel);
    juce::AudioProcessorValueTreeState parameters;
    std::atomic<int> selectedChannel { 1 }, voices { 0 }, lastNote { -1 }, lastVelocity { 0 };
    std::atomic<unsigned> midiActivity { 0 }, fontRevision { 0 }, clipActivity { 0 };
    std::array<std::atomic<float>, 2> peaks {};
    void panic() { panicRequested.store (true); }
    int getControllerValue (int channel, int cc) const
    {
        const auto& c = channels[static_cast<size_t> (juce::jlimit (1, 16, channel) - 1)];
        constexpr std::array<int, 6> numbers { 7, 10, 11, 91, 93, 1 };
        for (size_t i = 0; i < numbers.size(); ++i)
            if (cc == numbers[i])
                if (const auto pending = c.editorCC[i].load(); pending >= 0) return pending;
        return c.controllerState[static_cast<size_t> (juce::jlimit (0, 127, cc))].load();
    }
    void setControllerFromEditor (int channel, int control, int value)
    { channels[static_cast<size_t> (juce::jlimit (1, 16, channel) - 1)].editorCC[static_cast<size_t> (control)].store (juce::jlimit (0, 127, value)); }
private:
    static constexpr int controlCount = 13;
    struct Channel
    {
        std::atomic<int> instrument { 0 }, requested { 0 };
        std::atomic<unsigned> revision { 0 };
        std::atomic<unsigned> appliedRevision { 0 };
        std::array<std::atomic<float>*, controlCount> values {};
        std::array<float, controlCount> previous {};
        std::array<float, controlCount> applied {};
        std::array<juce::SmoothedValue<float>, controlCount> smooth;
        std::array<int, 128> controllers {};
        std::array<std::atomic<int>, 128> controllerState {};
        std::array<std::atomic<int>, 6> editorCC {};
        std::array<std::atomic<bool>, 6> midiOverride {};
    };
    struct ToneFilter
    {
        std::array<double, 5> c { 1, 0, 0, 0, 0 };
        std::array<double, 2> z1 {}, z2 {};
        float process (int channel, float x);
    };
    class Maintenance
    {
    public:
        explicit Maintenance (JekaborGMAudioProcessor&);
        ~Maintenance();
    private:
        JekaborGMAudioProcessor& owner;
        juce::ScopedLock lock;
    };
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameters();
    static juce::File findSoundFont();
    bool loadSoundFontInternal (const juce::File&);
    void cachePresets();
    void applyChannelInstruments();
    void resetControls();
    void updateControls (int samples);
    void handleMidiMessage (const juce::MidiMessage&);
    void renderAudio (juce::AudioBuffer<float>&, int, int);
    void processOutput (juce::AudioBuffer<float>&);
    void updateTone (float low, float mid, float high);
    std::array<Channel, 16> channels;
    std::array<std::atomic<float>*, 7> outputValues {};
    std::array<juce::SmoothedValue<float>, 7> outputSmooth;
    juce::dsp::StateVariableTPTFilter<float> filter;
    std::array<ToneFilter, 3> tone;
    mutable juce::CriticalSection maintenanceLock, metadataLock;
    std::atomic<bool> maintaining { false }, audioActive { false }, panicRequested { false };
    fluid_settings_t* settings = nullptr;
    fluid_synth_t* synth = nullptr;
    juce::File loadedSoundFont;
    int soundFontId = -1;
    double synthSampleRate = 44100.0;
    bool rateReady = false;
    juce::String soundFontStatus { "NO SOUNDFONT / LOAD SF2 OR SF3" };
    juce::Array<Instrument> presets;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JekaborGMAudioProcessor)
};
