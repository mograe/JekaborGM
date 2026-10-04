#include "PluginProcessor.h"
#include "PluginEditor.h"

#if defined (JEKABORGM_DELAY_LOAD_FLUIDSYNTH)
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>

namespace
{
bool loadFluidSynthRuntime()
{
    // Keep the runtime loaded for the process lifetime, including all instances
    // and the delay-import function pointers maintained by the linker.
    static const HMODULE runtime = []() -> HMODULE
    {
        static const int moduleAnchor = 0;
        HMODULE pluginModule = nullptr;
        if (! GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                     | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                                 reinterpret_cast<LPCWSTR> (&moduleAnchor), &pluginModule))
            return nullptr;

        wchar_t modulePath[32768] {};
        const auto length = GetModuleFileNameW (pluginModule, modulePath, 32768);
        if (length == 0 || length >= 32768)
            return nullptr;

        const auto dll = juce::File (juce::String (modulePath)).getSiblingFile
                            (JEKABORGM_FLUIDSYNTH_DLL_NAME);
        const auto handle = LoadLibraryExW (dll.getFullPathName().toWideCharPointer(), nullptr,
                                           LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR
                                               | LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (handle == nullptr)
            juce::Logger::writeToLog ("JekaborGM: failed to load " + dll.getFullPathName()
                                     + " (Windows error " + juce::String (GetLastError()) + ")");
        return handle;
    }();

    return runtime != nullptr;
}
}
#endif

juce::File JekaborGMAudioProcessor::findSoundFont()
{
    const auto overridePath = juce::SystemStats::getEnvironmentVariable ("JEKABORGM_SOUNDFONT", {});
    if (overridePath.isNotEmpty())
        return juce::File::getCurrentWorkingDirectory().getChildFile (overridePath);

    const auto userDirectory = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                                  .getChildFile ("JekaborGM");
    const auto userSoundFont = userDirectory.getChildFile ("FluidR3_GM.sf2");
    if (userSoundFont.existsAsFile())
        return userSoundFont;

    const auto generalUser = userDirectory.getChildFile ("GeneralUser-GS.sf2");
    if (generalUser.existsAsFile())
        return generalUser;

   #if JUCE_LINUX
    return juce::File ("/usr/share/sounds/sf2/FluidR3_GM.sf2");
   #else
    return userSoundFont;
   #endif
}

JekaborGMAudioProcessor::JekaborGMAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    channelInstruments[9].bank = 128;
   #if defined (JEKABORGM_DELAY_LOAD_FLUIDSYNTH)
    if (! loadFluidSynthRuntime())
    {
        soundFontStatus = "FluidSynth could not be loaded.";
        return;
    }
   #endif

    settings = new_fluid_settings();
    if (settings == nullptr)
    {
        soundFontStatus = "Could not create FluidSynth settings.";
        return;
    }

    fluid_settings_setnum (settings, "synth.sample-rate", 44100.0);
    fluid_settings_setint (settings, "synth.audio-channels", 2);
    fluid_settings_setint (settings, "synth.audio-groups", 2);

    synth = new_fluid_synth (settings);
    if (synth == nullptr)
    {
        soundFontStatus = "Could not create FluidSynth synthesizer.";
        return;
    }

    const auto soundFont = findSoundFont();
    if (soundFont.existsAsFile())
        loadSoundFont (soundFont);
}

bool JekaborGMAudioProcessor::loadSoundFont (const juce::File& file)
{
    const juce::ScopedLock lock (getCallbackLock());
    if (synth == nullptr)
        return false;

    if (soundFontId >= 0 && file == loadedSoundFont)
    {
        soundFontStatus = "SoundFont: " + file.getFileName();
        return true;
    }

    if (! file.existsAsFile())
    {
        soundFontStatus = "SoundFont file not found: " + file.getFileName();
        return false;
    }

    const auto newId = fluid_synth_sfload (synth, file.getFullPathName().toRawUTF8(), 1);
    if (newId < 0)
    {
        soundFontStatus = "Could not load SoundFont: " + file.getFileName();
        return false;
    }

    if (soundFontId >= 0)
        fluid_synth_sfunload (synth, soundFontId, 0);

    soundFontId = newId;
    applyChannelInstruments();
    loadedSoundFont = file;
    soundFontStatus = "SoundFont: " + file.getFileName();
    return true;
}

juce::String JekaborGMAudioProcessor::getSoundFontStatus() const
{
    const juce::ScopedLock lock (getCallbackLock());
    return soundFontStatus;
}

juce::Array<JekaborGMAudioProcessor::Instrument> JekaborGMAudioProcessor::getInstruments() const
{
    const juce::ScopedLock lock (getCallbackLock());
    juce::Array<Instrument> result;
    auto* font = synth != nullptr && soundFontId >= 0
                   ? fluid_synth_get_sfont_by_id (synth, soundFontId) : nullptr;
    if (font != nullptr)
    {
        fluid_sfont_iteration_start (font);
        while (auto* preset = fluid_sfont_iteration_next (font))
            result.add ({ fluid_preset_get_banknum (preset), fluid_preset_get_num (preset),
                          juce::String::fromUTF8 (fluid_preset_get_name (preset)) });
    }
    return result;
}

JekaborGMAudioProcessor::Instrument JekaborGMAudioProcessor::getChannelInstrument (int channel) const
{
    const juce::ScopedLock lock (getCallbackLock());
    auto result = channelInstruments[static_cast<size_t> (juce::jlimit (1, 16, channel) - 1)];
    int fontId = -1;
    if (synth != nullptr)
        fluid_synth_get_program (synth, juce::jlimit (1, 16, channel) - 1,
                                 &fontId, &result.bank, &result.program);
    return result;
}

void JekaborGMAudioProcessor::selectInstrument (int channel, int bank, int program)
{
    if (channel < 1 || channel > 16 || bank < 0 || program < 0 || program > 127)
        return;
    const juce::ScopedLock lock (getCallbackLock());
    if (synth != nullptr && soundFontId >= 0
        && fluid_synth_program_select (synth, channel - 1, soundFontId, bank, program) != FLUID_OK)
        return;
    channelInstruments[static_cast<size_t> (channel - 1)] = { bank, program, {} };
}

void JekaborGMAudioProcessor::applyChannelInstruments()
{
    if (synth == nullptr || soundFontId < 0)
        return;
    for (int channel = 0; channel < 16; ++channel)
    {
        const auto& instrument = channelInstruments[static_cast<size_t> (channel)];
        fluid_synth_program_select (synth, channel, soundFontId, instrument.bank, instrument.program);
    }
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

    const juce::ScopedLock lock (getCallbackLock());
    if (synth == nullptr || sampleRate <= 0.0 || sampleRate == synthSampleRate)
        return;

    // Recent FluidSynth versions require recreating the synth to change rate.
    fluid_settings_setnum (settings, "synth.sample-rate", sampleRate);
    auto* replacement = new_fluid_synth (settings);
    if (replacement == nullptr)
    {
        fluid_settings_setnum (settings, "synth.sample-rate", synthSampleRate);
        soundFontStatus = "Could not configure the audio sample rate.";
        return;
    }

    const auto replacementId = loadedSoundFont.existsAsFile()
                                 ? fluid_synth_sfload (replacement,
                                     loadedSoundFont.getFullPathName().toRawUTF8(), 1)
                                 : -1;
    delete_fluid_synth (synth);
    synth = replacement;
    synthSampleRate = sampleRate;
    soundFontId = replacementId;
    applyChannelInstruments();
    if (loadedSoundFont != juce::File {} && replacementId < 0)
        soundFontStatus = "Could not reload SoundFont: " + loadedSoundFont.getFileName();
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
        channelInstruments[static_cast<size_t> (channel)] = getChannelInstrument (channel + 1);
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
    const juce::ScopedLock lock (getCallbackLock());
    juce::XmlElement state ("JekaborGMState");
    state.setAttribute ("soundFont", loadedSoundFont.getFullPathName());
    for (int channel = 1; channel <= 16; ++channel)
    {
        const auto instrument = getChannelInstrument (channel);
        auto* entry = state.createNewChildElement ("Channel");
        entry->setAttribute ("number", channel);
        entry->setAttribute ("bank", instrument.bank);
        entry->setAttribute ("program", instrument.program);
    }
    copyXmlToBinary (state, destData);
}

void JekaborGMAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (const auto state = getXmlFromBinary (data, sizeInBytes))
        if (state->hasTagName ("JekaborGMState"))
        {
            const juce::ScopedLock lock (getCallbackLock());
            const auto path = state->getStringAttribute ("soundFont");
            if (juce::File::isAbsolutePath (path))
                loadSoundFont (juce::File (path));
            for (auto* entry : state->getChildIterator())
                if (entry->hasTagName ("Channel"))
                    selectInstrument (entry->getIntAttribute ("number"),
                                      entry->getIntAttribute ("bank"),
                                      entry->getIntAttribute ("program"));
        }
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new JekaborGMAudioProcessor();
}
