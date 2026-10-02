#pragma once

#include "PluginProcessor.h"

class JekaborGMAudioProcessorEditor final : public juce::AudioProcessorEditor
{
public:
    explicit JekaborGMAudioProcessorEditor (JekaborGMAudioProcessor&);
    ~JekaborGMAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    JekaborGMAudioProcessor& processorRef;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (JekaborGMAudioProcessorEditor)
};
