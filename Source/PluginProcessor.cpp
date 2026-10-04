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

namespace
{
constexpr std::array<const char*, 13> controlIDs { "volume", "pan", "expression", "reverb", "chorus",
    "attack", "decay", "sustain", "release", "bend", "modulation", "velocity", "fine" };
constexpr std::array<int, 6> ccNumbers { 7, 10, 11, 91, 93, 1 };
bool isRestorableController (int cc)
{
    // Data-entry and RPN/NRPN selectors are protocol messages, not standalone state.
    return cc != 6 && cc != 38 && (cc < 96 || cc > 101);
}
}

juce::String JekaborGMAudioProcessor::channelID (const juce::String& base, int channel)
{
    return base + "_" + juce::String (channel);
}

juce::AudioProcessorValueTreeState::ParameterLayout JekaborGMAudioProcessor::createParameters()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&] (const juce::String& id, const juce::String& name, float lo, float hi,
                    float step, float def, float skew = 1.0f)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
            juce::NormalisableRange<float> (lo, hi, step, skew), def));
    };
    for (int ch = 1; ch <= 16; ++ch)
    {
        auto param = [&] (const char* id, const char* name, float lo, float hi, float step, float def)
        { add (channelID (id, ch), "Ch " + juce::String (ch) + " " + name, lo, hi, step, def); };
        param ("volume", "Volume", 0, 127, 1, 100);
        param ("pan", "Pan", 0, 127, 1, 64);
        param ("expression", "Expression", 0, 127, 1, 127);
        param ("reverb", "Reverb send", 0, 127, 1, 40);
        param ("chorus", "Chorus send", 0, 127, 1, 0);
        param ("attack", "Attack offset", -2400, 2400, 1, 0);
        param ("decay", "Decay offset", -2400, 2400, 1, 0);
        param ("sustain", "Sustain offset", -24, 24, 0.1f, 0);
        param ("release", "Release offset", -2400, 2400, 1, 0);
        param ("bend", "Pitch bend range", 0, 24, 1, 2);
        param ("modulation", "Modulation", 0, 127, 1, 0);
        param ("velocity", "Velocity sensitivity", 0, 200, 1, 100);
        param ("fine", "Fine tune", -100, 100, 1, 0);
    }
    add ("master", "Master volume", -60, 6, 0.1f, 0);
    add ("cutoff", "Filter cutoff", 30, 20000, 1, 20000, 0.25f);
    add ("resonance", "Filter resonance", 0.7071f, 2.0f, 0.001f, 0.7071f);
    add ("bass", "Bass", -12, 12, 0.1f, 0);
    add ("mid", "Mid", -12, 12, 0.1f, 0);
    add ("treble", "Treble", -12, 12, 0.1f, 0);
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "filterOn", 1 },
                                                         "Filter enabled", false));
    return layout;
}

JekaborGMAudioProcessor::Maintenance::Maintenance (JekaborGMAudioProcessor& p)
    : owner (p), lock (p.maintenanceLock)
{
    owner.maintaining.store (true, std::memory_order_seq_cst);
    // Only non-realtime callers wait. The callback always returns immediately during maintenance.
    while (owner.audioActive.load (std::memory_order_seq_cst))
        juce::Thread::sleep (1);
}
JekaborGMAudioProcessor::Maintenance::~Maintenance()
{
    owner.maintaining.store (false, std::memory_order_seq_cst);
}

juce::File JekaborGMAudioProcessor::findSoundFont()
{
    const auto path = juce::SystemStats::getEnvironmentVariable ("JEKABORGM_SOUNDFONT", {});
    if (path.isNotEmpty())
        return juce::File::getCurrentWorkingDirectory().getChildFile (path);
    const auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                         .getChildFile ("JekaborGM");
    for (const auto* name : { "FluidR3_GM.sf2", "GeneralUser-GS.sf2" })
        if (dir.getChildFile (name).existsAsFile())
            return dir.getChildFile (name);
   #if JUCE_LINUX
    return juce::File ("/usr/share/sounds/sf2/FluidR3_GM.sf2");
   #else
    return dir.getChildFile ("FluidR3_GM.sf2");
   #endif
}

JekaborGMAudioProcessor::JekaborGMAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      parameters (*this, nullptr, "Parameters", createParameters())
{
    for (int ch = 0; ch < 16; ++ch)
    {
        auto& c = channels[static_cast<size_t> (ch)];
        for (int i = 0; i < controlCount; ++i)
            c.values[static_cast<size_t> (i)] = parameters.getRawParameterValue
                (channelID (controlIDs[static_cast<size_t> (i)], ch + 1));
        c.controllers.fill (-1);
        for (auto& v : c.controllerState) v.store (-1);
        for (auto& v : c.editorCC) v.store (-1);
    }
    channels[9].instrument.store (128 << 7);
    channels[9].requested.store (128 << 7);
    constexpr std::array<const char*, 7> outputIDs { "master", "cutoff", "resonance", "bass", "mid", "treble", "filterOn" };
    for (size_t i = 0; i < outputIDs.size(); ++i)
        outputValues[i] = parameters.getRawParameterValue (outputIDs[i]);
   #if defined (JEKABORGM_DELAY_LOAD_FLUIDSYNTH)
    if (! loadFluidSynthRuntime()) { soundFontStatus = "FLUIDSYNTH RUNTIME UNAVAILABLE"; return; }
   #endif
    settings = new_fluid_settings();
    if (settings == nullptr) { soundFontStatus = "COULD NOT CREATE SYNTH SETTINGS"; return; }
    fluid_settings_setnum (settings, "synth.sample-rate", synthSampleRate);
    fluid_settings_setint (settings, "synth.audio-channels", 1);
    fluid_settings_setint (settings, "synth.audio-groups", 1);
    fluid_settings_setint (settings, "synth.polyphony", 256);
    fluid_settings_setint (settings, "synth.cpu-cores", 1);
    // This synth is accessed exclusively by the callback or under Maintenance.
    fluid_settings_setint (settings, "synth.threadsafe-api", 0);
    synth = new_fluid_synth (settings);
    if (synth == nullptr) { soundFontStatus = "COULD NOT CREATE SYNTH"; return; }
    if (const auto file = findSoundFont(); file.existsAsFile()) loadSoundFont (file);
}
JekaborGMAudioProcessor::~JekaborGMAudioProcessor()
{
    Maintenance guard (*this);
    if (synth != nullptr) delete_fluid_synth (synth);
    if (settings != nullptr) delete_fluid_settings (settings);
}

bool JekaborGMAudioProcessor::loadSoundFont (const juce::File& file)
{
    Maintenance guard (*this);
    return loadSoundFontInternal (file);
}
bool JekaborGMAudioProcessor::loadSoundFontInternal (const juce::File& file)
{
    const juce::ScopedLock metadata (metadataLock);
    if (synth == nullptr) return false;
    if (soundFontId >= 0 && file == loadedSoundFont) return true;
    if (! file.existsAsFile()) { soundFontStatus = "FILE NOT FOUND: " + file.getFileName(); return false; }
    const auto newId = fluid_synth_sfload (synth, file.getFullPathName().toRawUTF8(), 0);
    if (newId < 0) { soundFontStatus = "LOAD FAILED: " + file.getFileName(); return false; }
    for (int ch = 0; ch < 16; ++ch) fluid_synth_all_sounds_off (synth, ch);
    if (soundFontId >= 0) fluid_synth_sfunload (synth, soundFontId, 0);
    soundFontId = newId;
    loadedSoundFont = file;
    soundFontStatus = "READY / " + file.getFileName();
    cachePresets();
    applyChannelInstruments();
    resetControls();
    fontRevision.fetch_add (1);
    return true;
}
void JekaborGMAudioProcessor::cachePresets()
{
    presets.clear();
    auto* font = synth != nullptr && soundFontId >= 0 ? fluid_synth_get_sfont_by_id (synth, soundFontId) : nullptr;
    if (font == nullptr) return;
    fluid_sfont_iteration_start (font);
    while (auto* p = fluid_sfont_iteration_next (font))
        presets.add ({ fluid_preset_get_banknum (p), fluid_preset_get_num (p),
                       juce::String::fromUTF8 (fluid_preset_get_name (p)) });
    std::sort (presets.begin(), presets.end(), [] (const auto& a, const auto& b)
               { return a.bank == b.bank ? a.program < b.program : a.bank < b.bank; });
}
juce::String JekaborGMAudioProcessor::getSoundFontStatus() const
{ const juce::ScopedLock lock (metadataLock); return soundFontStatus; }
juce::String JekaborGMAudioProcessor::getSoundFontName() const
{ const juce::ScopedLock lock (metadataLock); return loadedSoundFont.getFileName(); }
juce::Array<JekaborGMAudioProcessor::Instrument> JekaborGMAudioProcessor::getInstruments() const
{ const juce::ScopedLock lock (metadataLock); return presets; }
JekaborGMAudioProcessor::Instrument JekaborGMAudioProcessor::getChannelInstrument (int channel) const
{
    const auto& c = channels[static_cast<size_t> (juce::jlimit (1, 16, channel) - 1)];
    const auto packed = c.revision.load() != c.appliedRevision.load() ? c.requested.load() : c.instrument.load();
    Instrument result { packed >> 7, packed & 127, {} };
    const juce::ScopedLock lock (metadataLock);
    for (const auto& p : presets)
        if (p.bank == result.bank && p.program == result.program) { result.name = p.name; break; }
    return result;
}
void JekaborGMAudioProcessor::selectInstrument (int channel, int bank, int program)
{
    if (channel < 1 || channel > 16 || bank < 0 || bank > 16383 || program < 0 || program > 127) return;
    auto& c = channels[static_cast<size_t> (channel - 1)];
    c.requested.store ((bank << 7) | program);
    c.revision.fetch_add (1);
}
void JekaborGMAudioProcessor::applyChannelInstruments()
{
    if (synth == nullptr || soundFontId < 0) return;
    for (int ch = 0; ch < 16; ++ch)
    {
        auto& c = channels[static_cast<size_t> (ch)];
        const auto rev = c.revision.load();
        const auto p = rev != c.appliedRevision ? c.requested.load() : c.instrument.load();
        if (fluid_synth_program_select (synth, ch, soundFontId, p >> 7, p & 127) == FLUID_OK)
            c.instrument.store (p);
        c.appliedRevision = rev;
    }
}

void JekaborGMAudioProcessor::resetControls()
{
    for (int ch = 0; ch < 16; ++ch)
    {
        auto& c = channels[static_cast<size_t> (ch)];
        c.controllers.fill (-1);
        c.applied.fill (std::numeric_limits<float>::quiet_NaN());
        for (int cc = 0; cc < 120; ++cc)
        {
            const auto value = c.controllerState[static_cast<size_t> (cc)].load();
            if (value >= 0 && isRestorableController (cc)) fluid_synth_cc (synth, ch, cc, value);
        }
        for (int i = 0; i < controlCount; ++i)
        {
            const auto n = static_cast<size_t> (i);
            const auto parameter = c.values[n]->load();
            c.previous[n] = parameter;
            c.smooth[n].reset (synthSampleRate, 0.025);
            const auto cc = i < 5 ? ccNumbers[n] : i == 10 ? 1 : -1;
            const auto saved = cc >= 0 ? c.controllerState[static_cast<size_t> (cc)].load() : -1;
            const bool midi = cc >= 0 && c.midiOverride[i == 10 ? 5 : n].load();
            c.smooth[n].setCurrentAndTargetValue (midi && saved >= 0 ? static_cast<float> (saved) : parameter);
        }
    }
    updateControls (0);
}
void JekaborGMAudioProcessor::resetAllParameters()
{
    Maintenance guard (*this);
    for (auto* parameter : getParameters())
    {
        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (parameter->getDefaultValue());
        parameter->endChangeGesture();
    }
    for (int ch = 0; ch < 16; ++ch)
    {
        auto& c = channels[static_cast<size_t> (ch)];
        for (auto& pending : c.editorCC) pending.store (-1);
        for (auto& flag : c.midiOverride) flag.store (false);
        for (auto& value : c.controllerState) value.store (-1);
        if (synth != nullptr)
        {
            // Reset performance controllers without changing the bank or preset.
            fluid_synth_cc (synth, ch, 121, 0);
            fluid_synth_pitch_bend (synth, ch, 8192);
            fluid_synth_channel_pressure (synth, ch, 0);
            for (int cc = 0; cc < 120; ++cc)
            {
                int value = 0;
                if (isRestorableController (cc) && fluid_synth_get_cc (synth, ch, cc, &value) == FLUID_OK)
                    c.controllerState[static_cast<size_t> (cc)].store (value);
            }
        }
        else
            for (size_t i = 0; i < ccNumbers.size(); ++i)
                c.controllerState[static_cast<size_t> (ccNumbers[i])].store
                    (juce::roundToInt (c.values[i == 5 ? 10 : i]->load()));
    }
    // Also clear MIDI overrides when the host target was already at its default.
    if (synth != nullptr) resetControls();
}
void JekaborGMAudioProcessor::prepareToPlay (double sampleRate, int maximumBlockSize)
{
    Maintenance guard (*this);
    if (sampleRate <= 0 || synth == nullptr) return;
    if (sampleRate != synthSampleRate)
    {
        fluid_settings_setnum (settings, "synth.sample-rate", sampleRate);
        auto* replacement = new_fluid_synth (settings);
        if (replacement != nullptr)
        {
            const juce::ScopedLock metadata (metadataLock);
            const auto id = loadedSoundFont.existsAsFile()
                ? fluid_synth_sfload (replacement, loadedSoundFont.getFullPathName().toRawUTF8(), 0) : -1;
            if (soundFontId < 0 || id >= 0)
            {
                delete_fluid_synth (synth); synth = replacement; soundFontId = id; synthSampleRate = sampleRate;
            }
            else { delete_fluid_synth (replacement); soundFontStatus = "SAMPLE RATE RELOAD FAILED"; }
        }
        if (synthSampleRate != sampleRate)
        {
            fluid_settings_setnum (settings, "synth.sample-rate", synthSampleRate);
            // Never render a synth at the wrong rate.
            rateReady = false;
            return;
        }
    }
    applyChannelInstruments();
    resetControls();
    filter.prepare ({ sampleRate, static_cast<juce::uint32> (juce::jmax (1, maximumBlockSize)), 2 });
    filter.setType (juce::dsp::StateVariableTPTFilterType::lowpass);
    filter.reset();
    for (auto& t : tone) { t.z1.fill (0); t.z2.fill (0); }
    for (size_t i = 0; i < outputSmooth.size(); ++i)
    {
        outputSmooth[i].reset (sampleRate, 0.035);
        outputSmooth[i].setCurrentAndTargetValue (outputValues[i]->load());
    }
    voices.store (0);
    rateReady = true;
}
bool JekaborGMAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet().isDisabled();
}

void JekaborGMAudioProcessor::updateControls (int samples)
{
    for (int ch = 0; ch < 16; ++ch)
    {
        auto& c = channels[static_cast<size_t> (ch)];
        const auto rev = c.revision.load();
        if (rev != c.appliedRevision)
        {
            const auto p = c.requested.load();
            if (soundFontId >= 0 && fluid_synth_program_select (synth, ch, soundFontId, p >> 7, p & 127) == FLUID_OK)
                c.instrument.store (p);
            c.appliedRevision = rev;
        }
        for (size_t i = 0; i < c.editorCC.size(); ++i)
            if (const auto value = c.editorCC[i].exchange (-1); value >= 0)
            {
                c.midiOverride[i].store (false);
                c.smooth[i == 5 ? 10 : i].setTargetValue (static_cast<float> (value));
            }
        for (int i = 0; i < controlCount; ++i)
        {
            const auto n = static_cast<size_t> (i);
            const float target = c.values[n]->load();
            const bool changed = target != c.previous[n];
            if (changed)
            {
                c.previous[n] = target; c.smooth[n].setTargetValue (target);
                if (i < 5 || i == 10) c.midiOverride[i == 10 ? 5 : n].store (false);
            }
            const float value = c.smooth[n].skip (samples);
            if (i < 5 || i == 10)
            {
                const int cc = i == 10 ? 1 : ccNumbers[n];
                const int v = juce::jlimit (0, 127, juce::roundToInt (value));
                if (c.controllers[static_cast<size_t> (cc)] != v)
                {
                    fluid_synth_cc (synth, ch, cc, v);
                    c.controllers[static_cast<size_t> (cc)] = v;
                    c.controllerState[static_cast<size_t> (cc)].store (v);
                }
            }
            else if (c.applied[n] != value)
            {
                c.applied[n] = value;
                switch (i)
                {
                    case 5: fluid_synth_set_gen (synth, ch, GEN_VOLENVATTACK, value); break;
                    case 6: fluid_synth_set_gen (synth, ch, GEN_VOLENVDECAY, value); break;
                    case 7: fluid_synth_set_gen (synth, ch, GEN_VOLENVSUSTAIN, -10.0f * value); break;
                    case 8: fluid_synth_set_gen (synth, ch, GEN_VOLENVRELEASE, value); break;
                    case 9: fluid_synth_pitch_wheel_sens (synth, ch, juce::roundToInt (target)); break;
                    case 12: fluid_synth_set_gen (synth, ch, GEN_FINETUNE, value); break;
                    default: break;
                }
            }
        }
    }
}

void JekaborGMAudioProcessor::handleMidiMessage (const juce::MidiMessage& m)
{
    if (m.isNoteOnOrOff() || m.isController() || m.isProgramChange() || m.isPitchWheel()
        || m.isAftertouch() || m.isChannelPressure()) midiActivity.fetch_add (1);
    const int ch = m.getChannel() - 1;
    if (ch < 0 || ch >= 16) return;
    auto& c = channels[static_cast<size_t> (ch)];
    if (m.isNoteOn())
    {
        const float sensitivity = c.values[11]->load() / 100.0f;
        const auto velocity = juce::jlimit (1, 127, juce::roundToInt (127.0f * std::pow
            (static_cast<float> (m.getVelocity()) / 127.0f, sensitivity)));
        fluid_synth_noteon (synth, ch, m.getNoteNumber(), velocity);
        lastNote.store (m.getNoteNumber()); lastVelocity.store (velocity);
    }
    else if (m.isNoteOff()) fluid_synth_noteoff (synth, ch, m.getNoteNumber());
    else if (m.isProgramChange())
    {
        fluid_synth_program_change (synth, ch, m.getProgramChangeNumber());
        int font = 0, bank = 0, program = 0;
        fluid_synth_get_program (synth, ch, &font, &bank, &program);
        c.instrument.store ((bank << 7) | program);
    }
    else if (m.isController())
    {
        const int cc = m.getControllerNumber(), value = m.getControllerValue();
        fluid_synth_cc (synth, ch, cc, value);
        c.controllers[static_cast<size_t> (cc)] = value;
        if (cc < 120) c.controllerState[static_cast<size_t> (cc)].store (value);
        for (int i = 0; i < 6; ++i)
            if (cc == ccNumbers[static_cast<size_t> (i)])
            {
                c.midiOverride[static_cast<size_t> (i)].store (true);
                c.smooth[static_cast<size_t> (i == 5 ? 10 : i)].setCurrentAndTargetValue (static_cast<float> (value));
            }
        if (cc == 121)
        {
            for (int number = 0; number < 120; ++number)
            {
                int actual = 0;
                if (fluid_synth_get_cc (synth, ch, number, &actual) == FLUID_OK)
                {
                    c.controllers[static_cast<size_t> (number)] = actual;
                    c.controllerState[static_cast<size_t> (number)].store (actual);
                }
            }
            for (int i = 0; i < 6; ++i)
            {
                const auto number = ccNumbers[static_cast<size_t> (i)];
                int actual = 0; fluid_synth_get_cc (synth, ch, number, &actual);
                c.controllers[static_cast<size_t> (number)] = actual;
                c.controllerState[static_cast<size_t> (number)].store (actual);
                c.midiOverride[static_cast<size_t> (i)].store (true);
                c.smooth[static_cast<size_t> (i == 5 ? 10 : i)].setCurrentAndTargetValue (static_cast<float> (actual));
            }
        }
    }
    else if (m.isPitchWheel()) fluid_synth_pitch_bend (synth, ch, m.getPitchWheelValue());
    else if (m.isAftertouch()) fluid_synth_key_pressure (synth, ch, m.getNoteNumber(), m.getAfterTouchValue());
    else if (m.isChannelPressure()) fluid_synth_channel_pressure (synth, ch, m.getChannelPressureValue());
}
void JekaborGMAudioProcessor::renderAudio (juce::AudioBuffer<float>& buffer, int start, int count)
{
    while (count > 0)
    {
        const int n = juce::jmin (64, count);
        updateControls (n);
        fluid_synth_write_float (synth, n, buffer.getWritePointer (0), start, 1,
                                          buffer.getWritePointer (1), start, 1);
        start += n; count -= n;
    }
}
float JekaborGMAudioProcessor::ToneFilter::process (int channel, float input)
{
    const auto n = static_cast<size_t> (channel);
    const double x = input, y = c[0] * x + z1[n];
    z1[n] = c[1] * x - c[3] * y + z2[n];
    z2[n] = c[2] * x - c[4] * y;
    return static_cast<float> (y);
}
void JekaborGMAudioProcessor::updateTone (float low, float mid, float high)
{
    using Coeff = juce::dsp::IIR::ArrayCoefficients<double>;
    const std::array<std::array<double, 6>, 3> coefficients {
        Coeff::makeLowShelf (synthSampleRate, juce::jmin (180.0, synthSampleRate * 0.2), 0.707,
                            juce::Decibels::decibelsToGain (static_cast<double> (low))),
        Coeff::makePeakFilter (synthSampleRate, juce::jmin (1000.0, synthSampleRate * 0.2), 0.8,
                              juce::Decibels::decibelsToGain (static_cast<double> (mid))),
        Coeff::makeHighShelf (synthSampleRate, juce::jmin (5000.0, synthSampleRate * 0.2), 0.707,
                             juce::Decibels::decibelsToGain (static_cast<double> (high))) };
    for (size_t i = 0; i < tone.size(); ++i)
    {
        const auto& a = coefficients[i];
        tone[i].c = { a[0] / a[3], a[1] / a[3], a[2] / a[3], a[4] / a[3], a[5] / a[3] };
    }
}
void JekaborGMAudioProcessor::processOutput (juce::AudioBuffer<float>& buffer)
{
    for (size_t i = 0; i < outputSmooth.size(); ++i) outputSmooth[i].setTargetValue (outputValues[i]->load());
    std::array<float, 2> blockPeaks {};
    bool clipped = false;
    auto* left = buffer.getWritePointer (0); auto* right = buffer.getWritePointer (1);
    std::array<float, 7> v {};
    for (int sample = 0; sample < buffer.getNumSamples(); ++sample)
    {
        for (size_t i = 0; i < v.size(); ++i) v[i] = outputSmooth[i].getNextValue();
        if (sample % 32 == 0)
        {
            filter.setCutoffFrequency (juce::jlimit (10.0f, static_cast<float> (synthSampleRate * 0.45), v[1]));
            filter.setResonance (v[2]);
            updateTone (v[3], v[4], v[5]);
        }
        const float gain = juce::Decibels::decibelsToGain (v[0]);
        for (int ch = 0; ch < 2; ++ch)
        {
            auto& x = ch == 0 ? left[sample] : right[sample];
            const float filtered = filter.processSample (ch, x);
            x += v[6] * (filtered - x);
            for (auto& t : tone) x = t.process (ch, x);
            x *= gain;
            if (! std::isfinite (x)) x = 0;
            const float magnitude = std::abs (x);
            clipped = clipped || magnitude >= 1.0f;
            // Transparent below -0.45 dBFS; bounded soft protection above it.
            if (magnitude > 0.95f) x = std::copysign (0.95f + 0.05f * std::tanh ((magnitude - 0.95f) / 0.05f), x);
            blockPeaks[static_cast<size_t> (ch)] = juce::jmax (blockPeaks[static_cast<size_t> (ch)], std::abs (x));
        }
    }
    filter.snapToZero();
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        auto current = peaks[i].load();
        while (current < blockPeaks[i] && ! peaks[i].compare_exchange_weak (current, blockPeaks[i])) {}
    }
    if (clipped) clipActivity.fetch_add (1);
}
void JekaborGMAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    audioActive.store (true, std::memory_order_seq_cst);
    struct Exit { std::atomic<bool>& flag; ~Exit() { flag.store (false, std::memory_order_seq_cst); } } exit { audioActive };
    if (maintaining.load (std::memory_order_seq_cst) || synth == nullptr || ! rateReady || buffer.getNumChannels() < 2) return;
    updateControls (0);
    if (panicRequested.exchange (false))
        for (int ch = 0; ch < 16; ++ch)
        {
            fluid_synth_cc (synth, ch, 64, 0); fluid_synth_all_sounds_off (synth, ch);
            channels[static_cast<size_t> (ch)].controllerState[64].store (0);
            channels[static_cast<size_t> (ch)].controllers[64] = 0;
        }
    int current = 0;
    const int count = buffer.getNumSamples();
    for (const auto metadata : midi)
    {
        const int eventSample = juce::jlimit (0, count, metadata.samplePosition);
        if (eventSample > current) { renderAudio (buffer, current, eventSample - current); current = eventSample; }
        handleMidiMessage (metadata.getMessage());
    }
    if (current < count) renderAudio (buffer, current, count - current);
    if (count > 0) processOutput (buffer);
    voices.store (fluid_synth_get_active_voice_count (synth));
    midi.clear();
}
juce::AudioProcessorEditor* JekaborGMAudioProcessor::createEditor()
{ return new JekaborGMAudioProcessorEditor (*this); }

void JekaborGMAudioProcessor::getStateInformation (juce::MemoryBlock& data)
{
    auto tree = parameters.copyState();
    auto state = std::make_unique<juce::XmlElement> ("JekaborGMState");
    state->setAttribute ("version", 2);
    { const juce::ScopedLock lock (metadataLock); state->setAttribute ("soundFont", loadedSoundFont.getFullPathName()); }
    state->setAttribute ("selectedChannel", selectedChannel.load());
    state->addChildElement (tree.createXml().release());
    for (int ch = 0; ch < 16; ++ch)
    {
        const auto& c = channels[static_cast<size_t> (ch)];
        const auto p = c.revision.load() != c.appliedRevision ? c.requested.load() : c.instrument.load();
        auto* entry = state->createNewChildElement ("Channel");
        entry->setAttribute ("number", ch + 1); entry->setAttribute ("bank", p >> 7); entry->setAttribute ("program", p & 127);
        for (int cc = 0; cc < 120; ++cc)
        {
            if (! isRestorableController (cc)) continue;
            int value = c.controllerState[static_cast<size_t> (cc)].load();
            for (size_t i = 0; i < ccNumbers.size(); ++i)
                if (cc == ccNumbers[i])
                {
                    if (! c.midiOverride[i].load()) value = juce::roundToInt (c.values[i == 5 ? 10 : i]->load());
                    if (const auto pending = c.editorCC[i].load(); pending >= 0) value = pending;
                    entry->setAttribute ("midiCC" + juce::String (cc), c.midiOverride[i].load());
                }
            if (value >= 0) entry->setAttribute ("cc" + juce::String (cc), value);
        }
    }
    copyXmlToBinary (*state, data);
}
void JekaborGMAudioProcessor::setStateInformation (const void* data, int size)
{
    const auto state = getXmlFromBinary (data, size);
    if (state == nullptr || ! state->hasTagName ("JekaborGMState")) return;
    Maintenance guard (*this);
    // Old sessions have no Parameters child: preserve their original neutral sound.
    if (auto* param = state->getChildByName ("Parameters")) parameters.replaceState (juce::ValueTree::fromXml (*param));
    else
        for (auto* p : getParameters())
        { p->setValueNotifyingHost (p->getDefaultValue()); }
    selectedChannel.store (juce::jlimit (1, 16, state->getIntAttribute ("selectedChannel", 1)));
    for (auto& c : channels)
    {
        for (auto& cc : c.controllerState) cc.store (-1);
        for (auto& cc : c.editorCC) cc.store (-1);
        for (auto& flag : c.midiOverride) flag.store (false);
    }
    for (auto* entry : state->getChildIterator())
    {
        if (! entry->hasTagName ("Channel")) continue;
        const int ch = entry->getIntAttribute ("number");
        if (ch < 1 || ch > 16) continue;
        selectInstrument (ch, entry->getIntAttribute ("bank"), entry->getIntAttribute ("program"));
        auto& c = channels[static_cast<size_t> (ch - 1)];
        for (size_t i = 0; i < ccNumbers.size(); ++i)
            c.midiOverride[i].store (entry->getBoolAttribute ("midiCC" + juce::String (ccNumbers[i])));
        for (int cc = 0; cc < 120; ++cc)
        {
            const int value = entry->getIntAttribute ("cc" + juce::String (cc), -1);
            c.controllerState[static_cast<size_t> (cc)].store (juce::jlimit (-1, 127, value));
        }
    }
    const auto path = state->getStringAttribute ("soundFont");
    if (juce::File::isAbsolutePath (path)) loadSoundFontInternal (juce::File (path));
    if (synth != nullptr)
    {
        for (int ch = 0; ch < 16; ++ch) fluid_synth_all_sounds_off (synth, ch);
        resetControls();
        applyChannelInstruments();
    }
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new JekaborGMAudioProcessor(); }
