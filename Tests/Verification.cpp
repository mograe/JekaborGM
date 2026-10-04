#include "PluginEditor.h"
#include <juce_audio_utils/juce_audio_utils.h>
#include <iostream>
#include <thread>
#include <stdexcept>

namespace
{
void require (bool success, const char* message)
{
    if (! success) throw std::runtime_error (message);
    std::cout << "PASS: " << message << std::endl;
}
void set (JekaborGMAudioProcessor& p, const juce::String& id, float value)
{
    auto* parameter = p.parameters.getParameter (id);
    if (parameter == nullptr) throw std::runtime_error ("Missing parameter");
    parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
}
float render (JekaborGMAudioProcessor& p, int blocks = 64)
{
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    double energy = 0;
    for (int block = 0; block < blocks; ++block)
    {
        p.processBlock (audio, midi);
        for (int ch = 0; ch < 2; ++ch)
            for (int i = 0; i < audio.getNumSamples(); ++i)
            {
                const auto v = audio.getSample (ch, i);
                if (! std::isfinite (v) || std::abs (v) > 1.00001f) throw std::runtime_error ("Unbounded/nonfinite audio");
                energy += v * v;
            }
    }
    return static_cast<float> (std::sqrt (energy / (blocks * 512)));
}
void note (JekaborGMAudioProcessor& p, int channel = 1, int key = 60, bool checkTiming = false)
{
    juce::AudioBuffer<float> audio (2, 256);
    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn (channel, key, static_cast<juce::uint8> (100)), 96);
    p.processBlock (audio, midi);
    for (int i = 0; checkTiming && i < 96; ++i)
        if (std::abs (audio.getSample (0, i)) > 0.000001f || std::abs (audio.getSample (1, i)) > 0.000001f)
        {
            std::cerr << "Pre-note sample " << i << ": " << audio.getSample (0, i) << ", " << audio.getSample (1, i) << std::endl;
            throw std::runtime_error ("MIDI timestamp not preserved");
        }
}
void screenshot (juce::AudioProcessorEditor& editor, const juce::File& dir, int w, int h)
{
    editor.setSize (w, h);
    auto snapshot = editor.createComponentSnapshot (editor.getLocalBounds());
    auto stream = dir.getChildFile ("panel-" + juce::String (w) + ".png").createOutputStream();
    if (stream == nullptr || ! stream->setPosition (0) || stream->truncate().failed()
        || ! juce::PNGImageFormat().writeImageToStream (snapshot, *stream))
        throw std::runtime_error ("Snapshot write failed");
}
juce::Slider* findSlider (juce::Component& component, const juce::String& name)
{
    if (auto* slider = dynamic_cast<juce::Slider*> (&component); slider != nullptr && slider->getName() == name) return slider;
    for (auto* child : component.getChildren()) if (auto* slider = findSlider (*child, name)) return slider;
    return nullptr;
}
juce::ComboBox* findComboBox (juce::Component& component, const juce::String& name)
{
    if (auto* box = dynamic_cast<juce::ComboBox*> (&component); box != nullptr && box->getName() == name) return box;
    for (auto* child : component.getChildren()) if (auto* box = findComboBox (*child, name)) return box;
    return nullptr;
}
void selectBeforeTimer (juce::ComboBox& box, int id)
{
    // Popup selections notify asynchronously. Force the editor timer to run first.
    box.setSelectedId (id, juce::sendNotificationAsync);
    juce::Thread::sleep (100);
    juce::Timer::callPendingTimersSynchronously();
    require (box.getSelectedId() == id, "Editor refresh preserves a pending dropdown selection");
    box.onChange();
}
}
int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI initialise;
    try
    {
        const auto dir = argc > 1 ? juce::File (juce::String (argv[1])) : juce::File::getCurrentWorkingDirectory().getChildFile ("build/verification");
        dir.createDirectory();
        JekaborGMAudioProcessor p;
        require (! p.getInstruments().isEmpty(), "Default SoundFont loads and enumerates presets");
        require (p.getParameters().size() == 215, "All 215 channel/output parameters registered");
        for (auto* param : p.getParameters())
            if (! param->isAutomatable()) throw std::runtime_error ("Parameter is not automatable");
        require (true, "All parameters support automation");
        p.prepareToPlay (48000, 256);
        note (p, 1, 60, true);
        const auto audible = render (p);
        require (audible > 0.0001f, "Sample-timed MIDI produces finite stereo audio");
        require (p.midiActivity.load() > 0 && p.voices.load() > 0, "MIDI activity and voice snapshots respond");
        p.panic(); render (p);
        p.selectInstrument (1, 0, 24); render (p, 1);
        require (p.getChannelInstrument (1).program == 24, "GUI program selection reaches FluidSynth");
        require (p.getChannelInstrument (10).bank == 128, "Channel 10 retains percussion bank");
        juce::AudioBuffer<float> audio (2, 256); juce::MidiBuffer midi;
        midi.addEvent (juce::MidiMessage::programChange (2, 40), 0);
        midi.addEvent (juce::MidiMessage::controllerEvent (2, 7, 83), 1);
        midi.addEvent (juce::MidiMessage::controllerEvent (2, 64, 127), 2);
        midi.addEvent (juce::MidiMessage::pitchWheel (2, 10000), 3);
        p.processBlock (audio, midi);
        require (p.getChannelInstrument (2).program == 40, "Incoming MIDI Program Change retains its channel");
        require (p.getControllerValue (2, 7) == 83, "Incoming MIDI CC is preserved");
        render (p);
        require (p.getControllerValue (2, 7) == 83, "GUI parameters do not overwrite incoming CC every block");
        set (p, "volume_2", 35); render (p);
        require (p.getControllerValue (2, 7) == 35 && p.getControllerValue (1, 7) == 100, "Channel automation is independent and reaches MIDI CC");
        set (p, "pan_1", 0); set (p, "expression_1", 98); set (p, "reverb_1", 70); set (p, "chorus_1", 65);
        set (p, "attack_1", 1200); set (p, "decay_1", -600); set (p, "sustain_1", -6); set (p, "release_1", 800);
        set (p, "bend_1", 12); set (p, "modulation_1", 48); set (p, "velocity_1", 150); set (p, "fine_1", 23);
        set (p, "master", -8); set (p, "filterOn", 1); set (p, "cutoff", 2500); set (p, "resonance", 1.2f);
        set (p, "bass", 3); set (p, "mid", -2); set (p, "treble", 4);
        midi.addEvent (juce::MidiMessage::controllerEvent (2, 10, 25), 0); p.processBlock (audio, midi);
        render (p); p.selectedChannel.store (2);
        juce::MemoryBlock state; p.getStateInformation (state);
        JekaborGMAudioProcessor restored; restored.setStateInformation (state.getData(), static_cast<int> (state.getSize()));
        restored.prepareToPlay (48000, 256);
        for (auto* parameter : p.getParameters())
        {
            const auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (parameter);
            if (ranged == nullptr || std::abs (ranged->getValue() - restored.parameters.getParameter (ranged->paramID)->getValue()) > 0.000001f)
                throw std::runtime_error ("Parameter state roundtrip failed");
        }
        require (true, "All parameters survive DAW state roundtrip");
        require (restored.getChannelInstrument (1).program == 24 && restored.getChannelInstrument (2).program == 40
                 && restored.selectedChannel.load() == 2, "All channel presets and selected part survive state restore");
        require (restored.getControllerValue (2, 7) == 35, "MIDI controller snapshot survives state restore");
        require (restored.getControllerValue (2, 10) == 25 && restored.parameters.getRawParameterValue ("pan_2")->load() == 64,
                 "MIDI CC override restores without changing the automation target");
        require (! restored.loadSoundFont (dir.getChildFile ("missing.sf2")) && ! restored.getInstruments().isEmpty(),
                 "Failed SoundFont load retains the working font");
        const auto savedXml = juce::AudioProcessor::getXmlFromBinary (state.getData(), static_cast<int> (state.getSize()));
        const juce::File originalFont (savedXml->getStringAttribute ("soundFont"));
        const auto copiedFont = dir.getChildFile ("reload-fixture." + originalFont.getFileExtension().trimCharactersAtStart ("."));
        require (originalFont.copyFileTo (copiedFont) && restored.loadSoundFont (copiedFont)
                 && restored.getChannelInstrument (2).program == 40 && restored.getControllerValue (2, 10) == 25,
                 "Successful SoundFont replacement retains presets and MIDI controls");
        restored.prepareToPlay (96000, 256);
        require (restored.getChannelInstrument (2).program == 40 && restored.getControllerValue (2, 7) == 35,
                 "Sample-rate recreation preserves channel programs and controllers");
        render (restored);
        set (p, "volume_2", 65); render (p, 1);
        juce::MemoryBlock rampState; p.getStateInformation (rampState);
        restored.setStateInformation (rampState.getData(), static_cast<int> (rampState.getSize())); render (restored);
        require (restored.getControllerValue (2, 7) == 65, "Saving during smoothing restores the parameter target");
        juce::XmlElement old ("JekaborGMState");
        auto* oldChannel = old.createNewChildElement ("Channel");
        oldChannel->setAttribute ("number", 1); oldChannel->setAttribute ("bank", 0); oldChannel->setAttribute ("program", 10);
        juce::MemoryBlock oldState; juce::AudioProcessor::copyXmlToBinary (old, oldState);
        restored.setStateInformation (oldState.getData(), static_cast<int> (oldState.getSize())); render (restored);
        require (restored.getChannelInstrument (1).program == 10 && restored.parameters.getRawParameterValue ("bass")->load() == 0,
                 "Legacy project state restores with neutral new controls");
        // Compare neutral and filtered high notes, after smoothing and silence reset.
        JekaborGMAudioProcessor clean; clean.prepareToPlay (48000, 256);
        note (clean, 1, 96); const float dry = render (clean);
        clean.panic(); render (clean);
        set (clean, "filterOn", 1); set (clean, "cutoff", 150); render (clean);
        note (clean, 1, 96); const float filtered = render (clean);
        require (filtered < dry * 0.6f, "Output filter audibly attenuates high-frequency content");
        clean.panic(); render (clean);
        set (clean, "filterOn", 0); set (clean, "master", -60); render (clean);
        note (clean); require (render (clean) < audible * 0.02f, "Master volume affects audio");
        clean.panic(); render (clean); set (clean, "master", 6); set (clean, "bass", 12); set (clean, "mid", 12); set (clean, "treble", 12);
        set (clean, "resonance", 2); set (clean, "filterOn", 1); set (clean, "cutoff", 1000); render (clean);
        midi.clear(); for (int key = 36; key < 84; ++key) midi.addEvent (juce::MidiMessage::noteOn (1, key, static_cast<juce::uint8> (127)), 0);
        clean.processBlock (audio, midi); render (clean, 180);
        require (true, "Extreme gain/EQ/resonance and 48-note load remain finite and bounded");
        std::atomic<bool> running { true };
        std::thread callback ([&]
        {
            juce::AudioBuffer<float> b (2, 256); juce::MidiBuffer m;
            while (running.load()) clean.processBlock (b, m);
        });
        for (int i = 0; i < 20; ++i) clean.loadSoundFont (dir.getChildFile ("missing.sf2"));
        running.store (false); callback.join(); require (true, "Concurrent SoundFont maintenance/audio callback completes safely");
        p.selectedChannel.store (1);
        auto editor = std::unique_ptr<juce::AudioProcessorEditor> (p.createEditor());
        auto* programMenu = findComboBox (*editor, "Program and instrument");
        auto* bankMenu = findComboBox (*editor, "SoundFont bank");
        auto* channelMenu = findComboBox (*editor, "MIDI channel");
        require (programMenu != nullptr && bankMenu != nullptr && channelMenu != nullptr, "Instrument dropdowns are available");
        const auto presets = p.getInstruments();
        int programId = 0;
        for (int i = 0; i < presets.size(); ++i)
            if (presets[i].bank == 0 && presets[i].program == 40) programId = i + 1;
        require (programId != 0, "Regression preset is available");
        selectBeforeTimer (*programMenu, programId);
        render (p, 1);
        require (p.getChannelInstrument (1).program == 40 && programMenu->getSelectedId() == programId,
                 "Dropdown selection reaches the synth after a timer tick");
        selectBeforeTimer (*bankMenu, 129);
        render (p, 1);
        require (p.getChannelInstrument (1).bank == 128, "Bank dropdown reaches the synth after a timer tick");
        selectBeforeTimer (*channelMenu, 10);
        require (p.selectedChannel.load() == 10, "Channel dropdown survives a timer tick");
        const auto firstPart = p.getChannelInstrument (1);
        selectBeforeTimer (*programMenu, programMenu->getItemId (1));
        render (p, 1);
        require (p.getChannelInstrument (10).program == presets[programMenu->getSelectedId() - 1].program
                 && p.getChannelInstrument (1).bank == firstPart.bank && p.getChannelInstrument (1).program == firstPart.program,
                 "Preset selection changes only the selected MIDI channel");
        channelMenu->setSelectedId (1, juce::sendNotificationSync);
        p.selectInstrument (1, 0, 24); render (p, 1);
        juce::Thread::sleep (100); juce::Timer::callPendingTimersSynchronously();
        auto* volumeKnob = findSlider (*editor, "VOLUME"); auto* attackKnob = findSlider (*editor, "ATTACK");
        require (volumeKnob != nullptr && attackKnob != nullptr && attackKnob->getTextFromValue (1200) == "2.00x"
                 && attackKnob->getValueFromText ("2.00x") == 1200, "Knob envelope multiplier formatting and numeric entry work");
        midi.addEvent (juce::MidiMessage::controllerEvent (1, 7, 20), 0); p.processBlock (audio, midi);
        volumeKnob->setValue (20, juce::dontSendNotification);
        volumeKnob->setValue (volumeKnob->getDoubleClickReturnValue(), juce::sendNotificationSync); render (p);
        require (p.getControllerValue (1, 7) == 100, "Knob default reset overrides MIDI even when the host value already equals the default");
        for (const auto size : { juce::Point<int> (880, 560), { 792, 504 }, { 1320, 840 } }) screenshot (*editor, dir, size.x, size.y);
        require (true, "Editor renders at default, minimum, and maximum scale");
        // Visual QA includes neutral controls, bypass, and an error with a working font retained.
        JekaborGMAudioProcessor neutral;
        const auto defaultDir = dir.getChildFile ("default"); defaultDir.createDirectory();
        auto defaultEditor = std::unique_ptr<juce::AudioProcessorEditor> (neutral.createEditor());
        screenshot (*defaultEditor, defaultDir, 880, 560);
        screenshot (*defaultEditor, defaultDir, 792, 504);
        defaultEditor.reset();
        neutral.loadSoundFont (dir.getChildFile ("missing-soundfont.sf2"));
        const auto errorDir = dir.getChildFile ("load-error"); errorDir.createDirectory();
        auto errorEditor = std::unique_ptr<juce::AudioProcessorEditor> (neutral.createEditor());
        screenshot (*errorEditor, errorDir, 880, 560);
        std::cout << "RMS baseline=" << audible << " filter dry=" << dry << " wet=" << filtered << std::endl;
        std::cout << "VERIFICATION COMPLETE" << std::endl;
        return 0;
    }
    catch (const std::exception& error) { std::cerr << "FAIL: " << error.what() << std::endl; return 1; }
}
