#include "PluginProcessor.h"
#include "PluginEditor.h"

JekaborGMAudioProcessor::JekaborGMAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    settings = new_fluid_settings();
    if (settings == nullptr)
        return;

    fluid_settings_setnum (settings, "synth.sample-rate", 44100.0);
    fluid_settings_setint (settings, "synth.audio-channels", 2);
    fluid_settings_setint (settings, "synth.audio-groups", 2);

    synth = new_fluid_synth (settings);
    if (synth == nullptr)
        return;

    if (juce::File (soundFontPath).existsAsFile())
        fluid_synth_sfload (synth, soundFontPath, 1);
}

JekaborGMAudioProcessor::~JekaborGMAudioProcessor()
{
    if (synth != nullptr)
    {
        delete_fluid_synth (synth);
        synth = nullptr;
    }

    if (settings != nullptr)
    {
        delete_fluid_settings (settings);
        settings = nullptr;
    }
}

const juce::String JekaborGMAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool JekaborGMAudioProcessor::acceptsMidi() const
{
    return true;
}

bool JekaborGMAudioProcessor::producesMidi() const
{
    return false;
}

bool JekaborGMAudioProcessor::isMidiEffect() const
{
    return false;
}

double JekaborGMAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int JekaborGMAudioProcessor::getNumPrograms()
{
    return 1;
}

int JekaborGMAudioProcessor::getCurrentProgram()
{
    return 0;
}

void JekaborGMAudioProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String JekaborGMAudioProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void JekaborGMAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

void JekaborGMAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    juce::ignoreUnused (samplesPerBlock);

    if (synth != nullptr)
        fluid_synth_set_sample_rate (synth, static_cast<float> (sampleRate));
}

void JekaborGMAudioProcessor::releaseResources()
{
}

bool JekaborGMAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    return layouts.getMainInputChannelSet().isDisabled();
}

void JekaborGMAudioProcessor::handleMidiMessage (const juce::MidiMessage& message)
{
    if (synth == nullptr)
        return;

    const int channel = message.getChannel() - 1;

    if (message.isNoteOn())
    {
        fluid_synth_noteon (synth, channel, message.getNoteNumber(), message.getVelocity());
    }
    else if (message.isNoteOff())
    {
        fluid_synth_noteoff (synth, channel, message.getNoteNumber());
    }
    else if (message.isProgramChange())
    {
        fluid_synth_program_change (synth, channel, message.getProgramChangeNumber());
    }
    else if (message.isController())
    {
        fluid_synth_cc (synth, channel, message.getControllerNumber(), message.getControllerValue());
    }
    else if (message.isPitchWheel())
    {
        fluid_synth_pitch_bend (synth, channel, message.getPitchWheelValue());
    }
}

void JekaborGMAudioProcessor::renderAudio (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (synth == nullptr || numSamples <= 0 || buffer.getNumChannels() < 2)
        return;

    fluid_synth_write_float (synth,
                             numSamples,
                             buffer.getWritePointer (0), startSample, 1,
                             buffer.getWritePointer (1), startSample, 1);
}

void JekaborGMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    if (synth == nullptr)
        return;

    int currentSample = 0;
    const int numSamples = buffer.getNumSamples();

    for (const auto metadata : midiMessages)
    {
        const int eventSample = juce::jlimit (0, numSamples, metadata.samplePosition);

        if (eventSample > currentSample)
        {
            renderAudio (buffer, currentSample, eventSample - currentSample);
            currentSample = eventSample;
        }

        handleMidiMessage (metadata.getMessage());
    }

    if (currentSample < numSamples)
        renderAudio (buffer, currentSample, numSamples - currentSample);
}

bool JekaborGMAudioProcessor::hasEditor() const
{
    return true;
}

juce::AudioProcessorEditor* JekaborGMAudioProcessor::createEditor()
{
    return new JekaborGMAudioProcessorEditor (*this);
}

void JekaborGMAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::ignoreUnused (destData);
}

void JekaborGMAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::ignoreUnused (data, sizeInBytes);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JekaborGMAudioProcessor();
}
