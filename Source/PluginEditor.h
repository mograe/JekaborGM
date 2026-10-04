#pragma once
#include "PluginProcessor.h"

class ModuleLookAndFeel final : public juce::LookAndFeel_V4
{
public:
    ModuleLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int, int, int, int, float, float, float, juce::Slider&) override;
    void drawComboBox (juce::Graphics&, int, int, bool, int, int, int, int, juce::ComboBox&) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool, bool) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    juce::Font getTextButtonFont (juce::TextButton&, int) override;
};

class JekaborGMAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit JekaborGMAudioProcessorEditor (JekaborGMAudioProcessor&);
    ~JekaborGMAudioProcessorEditor() override;
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    struct Knob
    {
        juce::Slider slider;
        juce::String id, label;
        std::function<juce::String (double)> format;
        std::function<double (const juce::String&)> parse;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };
    void timerCallback() override;
    void refreshInstruments (bool rebuild = false);
    void bindChannel();
    void stepProgram (int direction);
    void drawPanel (juce::Graphics&, juce::Rectangle<int>, const juce::String&, const juce::String&);
    void drawKnobLabels (juce::Graphics&);
    JekaborGMAudioProcessor& processorRef;
    ModuleLookAndFeel look;
    juce::Component controls;
    juce::ComboBox channelSelector, bankSelector, instrumentSelector;
    juce::TextButton previous { "<" }, next { ">" }, selectSoundFont { "LOAD SF" }, panicButton { "ALL NOTES OFF" };
    juce::TextButton resetButton { "RESET ALL" };
    juce::ToggleButton filterSwitch { "ON" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> filterAttachment;
    std::array<Knob, 19> knobs;
    juce::Array<JekaborGMAudioProcessor::Instrument> instruments;
    juce::String instrumentName, fontName, status;
    int displayedBank = 0, displayedProgram = 0;
    std::array<int, 3> refreshedSelectionIds {};
    unsigned seenFont = ~0u, seenMidi = 0, seenClip = 0;
    int midiHold = 0, clipHold = 0;
    std::array<float, 2> meters {};
    std::unique_ptr<juce::FileChooser> soundFontChooser;
    juce::TooltipWindow tooltips { this, 650 };
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JekaborGMAudioProcessorEditor)
};
