#pragma once

#include "PluginProcessor.h"

class JekaborGMAudioProcessorEditor final : public juce::AudioProcessorEditor,
                                          private juce::Timer
{
public:
    explicit JekaborGMAudioProcessorEditor (JekaborGMAudioProcessor&);
    ~JekaborGMAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshInstruments();
    JekaborGMAudioProcessor& processorRef;
    juce::Label channelLabel { {}, "MIDI channel" };
    juce::Label instrumentLabel { {}, "Instrument" };
    juce::ComboBox channelSelector;
    juce::ComboBox instrumentSelector;
    juce::Array<JekaborGMAudioProcessor::Instrument> instruments;
    juce::TextButton selectSoundFont { "Select SoundFont..." };
    std::unique_ptr<juce::FileChooser> soundFontChooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JekaborGMAudioProcessorEditor)
};
