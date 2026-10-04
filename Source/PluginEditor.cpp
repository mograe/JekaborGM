#include "PluginProcessor.h"
#include "PluginEditor.h"

JekaborGMAudioProcessorEditor::JekaborGMAudioProcessorEditor (JekaborGMAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setSize (480, 360);
    addAndMakeVisible (channelLabel);
    addAndMakeVisible (instrumentLabel);
    addAndMakeVisible (channelSelector);
    addAndMakeVisible (instrumentSelector);
    for (int channel = 1; channel <= 16; ++channel)
        channelSelector.addItem (juce::String (channel) + (channel == 10 ? " (Drums)" : ""), channel);
    channelSelector.setSelectedId (1, juce::dontSendNotification);
    channelSelector.onChange = [this] { refreshInstruments(); };
    instrumentSelector.setTextWhenNothingSelected ("Instrument unavailable");
    instrumentSelector.setTextWhenNoChoicesAvailable ("Load a SoundFont first");
    instrumentSelector.onChange = [this]
    {
        const auto index = instrumentSelector.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, instruments.size()))
        {
            const auto& instrument = instruments.getReference (index);
            processorRef.selectInstrument (channelSelector.getSelectedId(), instrument.bank, instrument.program);
            refreshInstruments();
        }
    };
    addAndMakeVisible (selectSoundFont);
    selectSoundFont.onClick = [this]
    {
        soundFontChooser = std::make_unique<juce::FileChooser>
            ("Select a SoundFont", juce::File {}, "*.sf2;*.sf3");
        soundFontChooser->launchAsync (juce::FileBrowserComponent::openMode
                                          | juce::FileBrowserComponent::canSelectFiles,
                                      [safeThis = juce::Component::SafePointer<JekaborGMAudioProcessorEditor> (this)]
                                      (const juce::FileChooser& chooser)
        {
            if (safeThis != nullptr && chooser.getResult().existsAsFile())
            {
                safeThis->processorRef.loadSoundFont (chooser.getResult());
                safeThis->refreshInstruments();
                safeThis->repaint();
            }
        });
    };
    refreshInstruments();
    startTimerHz (4);
}

JekaborGMAudioProcessorEditor::~JekaborGMAudioProcessorEditor()
{
    stopTimer();
}

void JekaborGMAudioProcessorEditor::timerCallback()
{
    refreshInstruments();
    repaint();
}

void JekaborGMAudioProcessorEditor::refreshInstruments()
{
    const auto available = processorRef.getInstruments();
    bool changed = available.size() != instruments.size();
    for (int i = 0; ! changed && i < available.size(); ++i)
        changed = available[i].bank != instruments[i].bank
                  || available[i].program != instruments[i].program
                  || available[i].name != instruments[i].name;
    if (changed)
    {
        instruments = available;
        instrumentSelector.clear (juce::dontSendNotification);
        for (int i = 0; i < instruments.size(); ++i)
        {
            const auto& instrument = instruments.getReference (i);
            instrumentSelector.addItem (juce::String (instrument.program + 1).paddedLeft ('0', 3)
                                        + " - " + instrument.name
                                        + " (Bank " + juce::String (instrument.bank) + ")", i + 1);
        }
    }
    const auto selected = processorRef.getChannelInstrument (channelSelector.getSelectedId());
    int selectedId = 0;
    for (int i = 0; i < instruments.size(); ++i)
        if (instruments[i].bank == selected.bank && instruments[i].program == selected.program)
        {
            selectedId = i + 1;
            break;
        }
    instrumentSelector.setSelectedId (selectedId, juce::dontSendNotification);
    instrumentSelector.setEnabled (! instruments.isEmpty());
}

void JekaborGMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1b1d23));

    auto bounds = getLocalBounds().reduced (24);
    g.setColour (juce::Colours::white);
    g.setFont (juce::FontOptions (28.0f, juce::Font::bold));
    g.drawFittedText ("JekaborGM", bounds.removeFromTop (40), juce::Justification::centred, 1);

    g.setColour (juce::Colour (0xffc8cdd8));
    g.setFont (juce::FontOptions (16.0f));
    g.drawFittedText ("General MIDI SoundFont Synth", bounds.removeFromTop (28), juce::Justification::centred, 1);

    bounds.removeFromTop (16);
    g.setFont (juce::FontOptions (14.0f));
    g.drawFittedText (processorRef.getSoundFontStatus(), bounds.removeFromTop (60),
                     juce::Justification::centred, 3);
}

void JekaborGMAudioProcessorEditor::resized()
{
    channelLabel.setBounds (24, 184, 120, 32);
    channelSelector.setBounds (144, 184, getWidth() - 168, 32);
    instrumentLabel.setBounds (24, 228, 120, 32);
    instrumentSelector.setBounds (144, 228, getWidth() - 168, 32);
    selectSoundFont.setBounds ((getWidth() - 200) / 2, getHeight() - 60, 200, 32);
}
