#include "PluginProcessor.h"
#include "PluginEditor.h"

JekaborGMAudioProcessorEditor::JekaborGMAudioProcessorEditor (JekaborGMAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);
    setSize (400, 220);
}

JekaborGMAudioProcessorEditor::~JekaborGMAudioProcessorEditor()
{
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
}

void JekaborGMAudioProcessorEditor::resized()
{
}
