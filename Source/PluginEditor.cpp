#include "PluginEditor.h"

namespace
{
const juce::Colour background (0xff101923), panel (0xff1b2b39), steel (0xff3b5264),
    ink (0xffd9e6ec), muted (0xff9bb0bc), accent (0xff91d5c9), lcdInk (0xffcef2e5),
    controlFill (0xff11212d), controlBorder (0xff344b5c), amber (0xffdeb68a);
namespace layout
{
constexpr int width = 880, height = 560, margin = 24, gutter = 12;
const juce::Rectangle<int> display { margin, 78, 608, 128 }, output { 644, 78, 212, 128 },
    selection { margin, 218, 832, 64 }, mixer { margin, 294, 328, 120 },
    filter { 364, 294, 188, 120 }, tone { 564, 294, 292, 120 },
    envelope { margin, 426, 328, 120 }, performance { 364, 426, 492, 120 };
}
const char* families[] { "PIANO", "CHROMATIC PERCUSSION", "ORGAN", "GUITAR", "BASS", "STRINGS",
    "ENSEMBLE", "BRASS", "REED", "PIPE", "SYNTH LEAD", "SYNTH PAD", "SYNTH EFFECTS",
    "ETHNIC", "PERCUSSIVE", "SOUND EFFECTS" };
void text (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size,
           juce::Colour colour, int style = juce::Font::plain, juce::Justification align = juce::Justification::centredLeft)
{
    g.setColour (colour); g.setFont (juce::FontOptions (size, style));
    g.drawFittedText (s, r, align, 1);
}
juce::String three (int v) { return juce::String (v).paddedLeft ('0', 3); }
void lcdText (juce::Graphics& g, const juce::String& s, juce::Rectangle<int> r, float size,
              juce::Colour colour, int style = juce::Font::plain,
              juce::Justification align = juce::Justification::centredLeft)
{
    g.setColour (colour);
    g.setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), size, style));
    g.drawFittedText (s, r, align, 1);
}
}
ModuleLookAndFeel::ModuleLookAndFeel()
{
    setColour (juce::ComboBox::backgroundColourId, background);
    setColour (juce::ComboBox::textColourId, ink);
    setColour (juce::ComboBox::outlineColourId, steel);
    setColour (juce::PopupMenu::backgroundColourId, background);
    setColour (juce::PopupMenu::textColourId, ink);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, steel);
    setColour (juce::PopupMenu::highlightedTextColourId, lcdInk);
    setColour (juce::Slider::textBoxTextColourId, accent);
    setColour (juce::Slider::textBoxBackgroundColourId, controlFill);
    setColour (juce::Slider::textBoxOutlineColourId, controlBorder);
    setColour (juce::ToggleButton::textColourId, accent);
    setColour (juce::ToggleButton::tickColourId, accent);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TooltipWindow::backgroundColourId, panel);
    setColour (juce::TooltipWindow::textColourId, ink);
}
juce::Font ModuleLookAndFeel::getComboBoxFont (juce::ComboBox&) { return juce::Font (juce::FontOptions (13.0f)); }
juce::Font ModuleLookAndFeel::getTextButtonFont (juce::TextButton&, int) { return juce::Font (juce::FontOptions (11.0f, juce::Font::bold)); }
juce::Label* ModuleLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* label = juce::LookAndFeel_V4::createSliderTextBox (slider);
    label->setFont (juce::FontOptions (juce::Font::getDefaultMonospacedFontName(), 12.0f, juce::Font::plain));
    label->setJustificationType (juce::Justification::centred);
    label->setBorderSize (juce::BorderSize<int> (1, 2, 1, 2));
    label->setMinimumHorizontalScale (0.9f);
    return label;
}
void ModuleLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    g.setColour (controlFill); g.fillRoundedRectangle ({ 0.0f, 0.0f, static_cast<float> (w), static_cast<float> (h) }, 2);
    g.setColour (box.hasKeyboardFocus (true) ? accent : box.isEnabled() ? controlBorder : controlBorder.withAlpha (0.4f));
    g.drawRoundedRectangle ({ 0.5f, 0.5f, static_cast<float> (w - 1), static_cast<float> (h - 1) }, 2, 1);
    g.setColour (juce::Colours::black.withAlpha (0.18f)); g.drawHorizontalLine (1, 2, static_cast<float> (w - 2));
    g.setColour (muted); juce::Path p;
    p.addTriangle (static_cast<float> (w - 15), h * 0.44f, static_cast<float> (w - 7), h * 0.44f,
                   static_cast<float> (w - 11), h * 0.6f); g.fillPath (p);
}
void ModuleLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool over, bool down)
{
    const auto r = b.getLocalBounds().toFloat().reduced (0.5f);
    const bool reset = b.getName() == "panic";
    g.setColour (down ? steel : over ? panel.brighter (0.12f) : juce::Colour (0xff253949));
    g.fillRoundedRectangle (r, 2);
    g.setColour (over || b.hasKeyboardFocus (true) ? accent : reset ? amber.withAlpha (0.48f) : steel);
    g.drawRoundedRectangle (r, 2, 1);
    g.setColour (juce::Colours::white.withAlpha (0.04f)); g.drawHorizontalLine (1, 2, r.getRight() - 2);
}
void ModuleLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool over, bool down)
{
    const auto direction = button.getName();
    if (direction == "previousPreset" || direction == "nextPreset")
    {
        const auto centre = button.getLocalBounds().toFloat().getCentre();
        const float sign = direction == "previousPreset" ? -1.0f : 1.0f;
        juce::Path chevron;
        chevron.startNewSubPath (centre.x - sign * 2.5f, centre.y - 4);
        chevron.lineTo (centre.x + sign * 2.5f, centre.y);
        chevron.lineTo (centre.x - sign * 2.5f, centre.y + 4);
        g.setColour (button.isEnabled() ? ink : muted.withAlpha (0.4f));
        g.strokePath (chevron, juce::PathStrokeType (1.6f));
        return;
    }
    if (direction == "panic")
    {
        text (g, button.getButtonText(), button.getLocalBounds(), 11.0f, amber,
              juce::Font::bold, juce::Justification::centred);
        return;
    }
    juce::LookAndFeel_V4::drawButtonText (g, button, over, down);
}
void ModuleLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button, bool over, bool down)
{
    const bool on = button.getToggleState();
    const auto track = juce::Rectangle<float> (0.5f, 4.5f, 24.0f, 13.0f);
    g.setColour (down ? panel : controlFill); g.fillRoundedRectangle (track, 1.5f);
    g.setColour (over || button.hasKeyboardFocus (true) ? accent : on ? accent.withAlpha (0.7f) : steel);
    g.drawRoundedRectangle (track, 1.5f, 1);
    g.setColour (on ? accent : muted.withAlpha (0.65f));
    g.fillRect (on ? 15.0f : 4.0f, 7.0f, 6.0f, 8.0f);
    text (g, on ? "ON" : "OFF", { 30, 0, 26, 22 }, 11, on ? accent : muted, juce::Font::bold);
}
void ModuleLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h,
                                         float position, float start, float end, juce::Slider& slider)
{
    const float diameter = juce::jmin (slider.getName() == "MASTER" ? 54.0f : 44.0f,
                                       static_cast<float> (w - 12), static_cast<float> (h - 6));
    const auto r = juce::Rectangle<float> (diameter, diameter).withCentre
        ({ x + w * 0.5f, y + h * 0.5f });
    const auto centre = r.getCentre(); const float radius = diameter * 0.5f;
    const bool bypassed = slider.getProperties()["bypassed"];
    for (int i = 0; i <= 10; ++i)
    {
        const float angle = start + (end - start) * static_cast<float> (i) / 10.0f;
        const auto a = centre.getPointOnCircumference (radius + 1, angle);
        const auto b = centre.getPointOnCircumference (radius + 3, angle);
        g.setColour (i == 5 ? muted.withAlpha (0.8f) : steel); g.drawLine ({ a, b }, 1);
    }
    juce::Path track; track.addCentredArc (centre.x, centre.y, radius - 1, radius - 1, 0, start, end, true);
    g.setColour (steel); g.strokePath (track, juce::PathStrokeType (2));
    const float angle = start + position * (end - start);
    const float anchor = slider.getProperties()["bipolar"]
        ? start + static_cast<float> (slider.valueToProportionOfLength (slider.getDoubleClickReturnValue())) * (end - start)
        : start;
    juce::Path active;
    active.addCentredArc (centre.x, centre.y, radius - 1, radius - 1, 0,
                          juce::jmin (anchor, angle), juce::jmax (anchor, angle), true);
    g.setColour (slider.isEnabled() && ! bypassed ? accent.withAlpha (0.85f) : muted.withAlpha (0.45f));
    g.strokePath (active, juce::PathStrokeType (1.8f));
    auto body = r.reduced (5);
    g.setColour (juce::Colours::black.withAlpha (0.35f)); g.fillEllipse (body.translated (0, 2));
    juce::ColourGradient grad (juce::Colour (0xff435b6b), centre.x, body.getY(),
                              juce::Colour (0xff243642), centre.x, body.getBottom(), false);
    g.setGradientFill (grad); g.fillEllipse (body);
    g.setColour (juce::Colour (0xff61788a)); g.drawEllipse (body, 0.8f);
    g.setColour (juce::Colours::black.withAlpha (0.16f)); g.drawEllipse (body.reduced (2), 0.6f);
    const auto tip = centre.getPointOnCircumference (radius - 9, start + position * (end - start));
    const auto base = centre.getPointOnCircumference (radius * 0.17f, start + position * (end - start));
    g.setColour (bypassed ? muted : lcdInk); g.drawLine ({ base, tip }, 2.2f);
}

JekaborGMAudioProcessorEditor::JekaborGMAudioProcessorEditor (JekaborGMAudioProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    setLookAndFeel (&look);
    addAndMakeVisible (controls);
    controls.setSize (layout::width, layout::height);
    for (auto* c : std::initializer_list<juce::Component*> { &channelSelector, &bankSelector, &instrumentSelector,
         &previous, &next, &selectSoundFont, &panicButton, &filterSwitch }) controls.addAndMakeVisible (c);
    for (int ch = 1; ch <= 16; ++ch)
        channelSelector.addItem (juce::String (ch).paddedLeft ('0', 2) + (ch == 10 ? " DR" : ""), ch);
    channelSelector.setSelectedId (p.selectedChannel.load(), juce::dontSendNotification);
    channelSelector.onChange = [this]
    { processorRef.selectedChannel.store (channelSelector.getSelectedId()); bindChannel(); refreshInstruments (true); };
    bankSelector.onChange = [this]
    {
        const auto bank = bankSelector.getSelectedId() - 1;
        for (const auto& i : instruments)
            if (i.bank == bank && i.program == displayedProgram)
            {
                processorRef.selectInstrument (channelSelector.getSelectedId(), bank, i.program);
                refreshInstruments();
                return;
            }
        for (const auto& i : instruments)
            if (i.bank == bank)
            {
                processorRef.selectInstrument (channelSelector.getSelectedId(), bank, i.program);
                refreshInstruments();
                return;
            }
    };
    instrumentSelector.setTextWhenNothingSelected ("PRESET UNAVAILABLE");
    instrumentSelector.setTextWhenNoChoicesAvailable ("LOAD A SOUNDFONT");
    instrumentSelector.onChange = [this]
    {
        const auto index = instrumentSelector.getSelectedId() - 1;
        if (juce::isPositiveAndBelow (index, instruments.size()))
        {
            const auto& i = instruments.getReference (index);
            processorRef.selectInstrument (channelSelector.getSelectedId(), i.bank, i.program);
            refreshInstruments();
        }
    };
    previous.onClick = [this] { stepProgram (-1); }; next.onClick = [this] { stepProgram (1); };
    previous.setName ("previousPreset"); next.setName ("nextPreset");
    channelSelector.setName ("MIDI channel"); bankSelector.setName ("SoundFont bank");
    instrumentSelector.setName ("Program and instrument");
    previous.setTooltip ("Previous preset in this bank"); next.setTooltip ("Next preset in this bank");
    bankSelector.setTooltip ("SoundFont bank. Channel 10 defaults to percussion bank 128.");
    channelSelector.setTooltip ("Edit one of 16 independent MIDI parts. Incoming MIDI retains its channel.");
    panicButton.onClick = [this] { processorRef.panic(); };
    panicButton.setName ("panic"); panicButton.setButtonText ("PANIC");
    panicButton.setTooltip ("Silence all 16 channels and release sustain pedals");
    selectSoundFont.onClick = [this]
    {
        soundFontChooser = std::make_unique<juce::FileChooser> ("Load SoundFont", juce::File {}, "*.sf2;*.sf3");
        soundFontChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
            [safe = juce::Component::SafePointer<JekaborGMAudioProcessorEditor> (this)] (const auto& chooser)
            {
                if (safe != nullptr && chooser.getResult().existsAsFile())
                { safe->processorRef.loadSoundFont (chooser.getResult()); safe->refreshInstruments (true); }
            });
    };
    constexpr std::array<const char*, 19> ids { "volume", "pan", "expression", "reverb", "chorus",
        "cutoff", "resonance", "bass", "mid", "treble", "attack", "decay", "sustain", "release",
        "bend", "modulation", "velocity", "fine", "master" };
    constexpr std::array<const char*, 19> labels { "VOLUME", "PAN", "EXPR", "REVERB", "CHORUS",
        "CUTOFF", "RESONANCE", "LOW", "MID", "HIGH", "ATTACK", "DECAY", "SUSTAIN", "RELEASE",
        "BEND", "MOD", "VELOCITY", "FINE TUNE", "MASTER" };
    for (size_t i = 0; i < 19; ++i)
    {
        auto& k = knobs[i]; k.id = ids[i]; k.label = labels[i];
        auto& s = k.slider; controls.addAndMakeVisible (s);
        s.setName (k.label); s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f, juce::MathConstants<float>::pi * 2.75f, true);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, i < 5 ? 58 : i == 18 ? 76 : 70, 18);
        s.getProperties().set ("bipolar", i == 1 || (i >= 7 && i <= 13) || i == 17);
        s.setScrollWheelEnabled (true); s.setMouseDragSensitivity (160);
        if (i == 10 || i == 11 || i == 13)
        {
            s.textFromValueFunction = [] (double v) { return juce::String (std::pow (2.0, v / 1200.0), 2) + "x"; };
            s.valueFromTextFunction = [] (const juce::String& t)
            { return 1200.0 * std::log2 (juce::jlimit (0.25, 4.0, t.getDoubleValue())); };
            s.setTooltip ("SoundFont envelope time multiplier. 1.00x preserves the preset; applied as a timecents offset.");
        }
        else if (i == 1)
        {
            s.textFromValueFunction = [] (double v)
            { const int pan = juce::roundToInt (v) - 64; return pan == 0 ? juce::String ("CENTER") : juce::String (std::abs (pan)) + (pan < 0 ? " L" : " R"); };
            s.valueFromTextFunction = [] (const juce::String& t)
            { return t.containsIgnoreCase ("CENTER") ? 64.0 : 64.0 + t.getDoubleValue() * (t.containsIgnoreCase ("L") ? -1.0 : 1.0); };
        }
        else if (i == 5) { s.setTextValueSuffix (" Hz"); s.setTooltip ("Stereo output low-pass filter. Enable the FILTER switch to hear it."); }
        else if (i == 6)
        {
            s.textFromValueFunction = [] (double v) { return juce::String (v, 2); };
            s.valueFromTextFunction = [] (const juce::String& t) { return t.getDoubleValue(); };
            s.setTextValueSuffix (" Q");
        }
        else if ((i >= 7 && i <= 9) || i == 12 || i == 18) s.setTextValueSuffix (" dB");
        else if (i == 14) s.setTextValueSuffix (" st");
        else if (i == 16) { s.setTextValueSuffix (" %"); s.setTooltip ("Velocity curve: 100% is original MIDI velocity; 0% gives full velocity; 200% softens quieter notes."); }
        else if (i == 17) s.setTextValueSuffix (" ct");
        if (i == 12) s.setTooltip ("Sustain level offset in dB. 0 dB preserves the SoundFont envelope; positive values reduce its attenuation.");
        if (i == 3 || i == 4) s.setTooltip ("Channel MIDI effect send (CC " + juce::String (i == 3 ? 91 : 93) + "). Uses FluidSynth's native effect.");
        k.format = s.textFromValueFunction;
        k.parse = s.valueFromTextFunction;
    }
    bindChannel();
    filterSwitch.onStateChange = [this]
    {
        const bool bypassed = ! filterSwitch.getToggleState();
        for (const auto index : { 5u, 6u })
        {
            auto& slider = knobs[index].slider;
            slider.getProperties().set ("bypassed", bypassed);
            slider.setColour (juce::Slider::textBoxTextColourId, bypassed ? muted : accent);
            slider.repaint();
        }
    };
    filterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (p.parameters, "filterOn", filterSwitch);
    filterSwitch.onStateChange();
    refreshInstruments (true);
    setResizable (true, true); setResizeLimits (792, 504, 1320, 840);
    if (auto* sizeConstraint = getConstrainer()) sizeConstraint->setFixedAspectRatio (880.0 / 560.0);
    setSize (layout::width, layout::height);
    startTimerHz (30);
}
JekaborGMAudioProcessorEditor::~JekaborGMAudioProcessorEditor()
{
    stopTimer(); filterAttachment.reset();
    for (auto& k : knobs) k.attachment.reset();
    setLookAndFeel (nullptr);
}
void JekaborGMAudioProcessorEditor::bindChannel()
{
    const int ch = channelSelector.getSelectedId();
    for (size_t i = 0; i < 19; ++i)
    {
        auto& k = knobs[i]; k.slider.onValueChange = nullptr; k.attachment.reset();
        const bool perChannel = i < 5 || (i >= 10 && i <= 17);
        const auto id = perChannel ? JekaborGMAudioProcessor::channelID (k.id, ch) : k.id;
        k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processorRef.parameters, id, k.slider);
        if (k.format) k.slider.textFromValueFunction = k.format;
        if (k.parse) k.slider.valueFromTextFunction = k.parse;
        k.slider.updateText();
        const auto* param = processorRef.parameters.getParameter (id);
        k.slider.setDoubleClickReturnValue (true, param->convertFrom0to1 (param->getDefaultValue()));
        if (i < 5 || i == 15)
            k.slider.onValueChange = [this, ch, i]
            {
                processorRef.setControllerFromEditor (ch, static_cast<int> (i == 15 ? 5 : i),
                    juce::roundToInt (knobs[i].slider.getValue()));
            };
    }
}
void JekaborGMAudioProcessorEditor::stepProgram (int direction)
{
    juce::Array<int> candidates;
    int position = -1;
    for (int i = 0; i < instruments.size(); ++i)
        if (instruments[i].bank == displayedBank)
        {
            if (instruments[i].program == displayedProgram) position = candidates.size();
            candidates.add (i);
        }
    if (candidates.isEmpty()) return;
    const int n = candidates.size(); const auto& p = instruments[candidates[(position + direction + n) % n]];
    processorRef.selectInstrument (channelSelector.getSelectedId(), p.bank, p.program);
}
void JekaborGMAudioProcessorEditor::refreshInstruments (bool rebuild)
{
    const auto revision = processorRef.fontRevision.load();
    if (revision != seenFont) { instruments = processorRef.getInstruments(); seenFont = revision; rebuild = true; }
    fontName = processorRef.getSoundFontName(); status = processorRef.getSoundFontStatus();
    const auto p = processorRef.getChannelInstrument (channelSelector.getSelectedId());
    if (displayedBank != p.bank) rebuild = true;
    displayedBank = p.bank; displayedProgram = p.program;
    instrumentName = p.name.isNotEmpty() ? p.name : "NO PRESET LOADED";
    if (rebuild)
    {
        bankSelector.clear (juce::dontSendNotification);
        int lastBank = -1;
        for (const auto& preset : instruments)
            if (preset.bank != lastBank) { bankSelector.addItem (three (preset.bank), preset.bank + 1); lastBank = preset.bank; }
        instrumentSelector.clear (juce::dontSendNotification);
        int lastFamily = -1;
        for (int i = 0; i < instruments.size(); ++i)
        {
            const auto& preset = instruments[i]; if (preset.bank != p.bank) continue;
            const int family = preset.program / 8;
            if (preset.bank == 0 && family != lastFamily)
            { instrumentSelector.addSectionHeading (families[family]); lastFamily = family; }
            instrumentSelector.addItem (three (preset.program + 1) + "  " + preset.name, i + 1);
        }
    }
    bankSelector.setSelectedId (p.bank + 1, juce::dontSendNotification);
    int id = 0;
    for (int i = 0; i < instruments.size(); ++i)
        if (instruments[i].bank == p.bank && instruments[i].program == p.program) { id = i + 1; break; }
    instrumentSelector.setSelectedId (id, juce::dontSendNotification);
    refreshedSelectionIds = { channelSelector.getSelectedId(), bankSelector.getSelectedId(), instrumentSelector.getSelectedId() };
    for (auto* c : std::initializer_list<juce::Component*> { &instrumentSelector, &bankSelector, &previous, &next }) c->setEnabled (! instruments.isEmpty());
}
void JekaborGMAudioProcessorEditor::timerCallback()
{
    // ComboBox popup changes notify asynchronously. Leave the selected IDs intact
    // until onChange has queued the user's choice and refreshed the selectors.
    const std::array<int, 3> selectionIds { channelSelector.getSelectedId(), bankSelector.getSelectedId(), instrumentSelector.getSelectedId() };
    if (selectionIds == refreshedSelectionIds)
    {
        if (channelSelector.getSelectedId() != processorRef.selectedChannel.load())
        { channelSelector.setSelectedId (processorRef.selectedChannel.load(), juce::dontSendNotification); bindChannel(); }
        refreshInstruments();
    }
    const auto activity = processorRef.midiActivity.load();
    if (activity != seenMidi) { midiHold = 4; seenMidi = activity; } else midiHold = juce::jmax (0, midiHold - 1);
    const auto clip = processorRef.clipActivity.load();
    if (clip != seenClip) { clipHold = 30; seenClip = clip; } else clipHold = juce::jmax (0, clipHold - 1);
    for (size_t i = 0; i < meters.size(); ++i) meters[i] = juce::jmax (meters[i] * 0.87f, processorRef.peaks[i].exchange (0));
    // MIDI CC changes update the visible knob without sending automation back to the host.
    constexpr std::array<int, 6> cc { 7, 10, 11, 91, 93, 1 };
    for (size_t i = 0; i < cc.size(); ++i)
    {
        auto& s = knobs[i == 5 ? 15 : i].slider;
        const int value = processorRef.getControllerValue (channelSelector.getSelectedId(), cc[i]);
        if (value >= 0 && ! s.isMouseButtonDown() && ! s.isMouseOverOrDragging() && ! s.hasKeyboardFocus (true))
            s.setValue (value, juce::dontSendNotification);
    }
    repaint();
}
void JekaborGMAudioProcessorEditor::drawPanel (juce::Graphics& g, juce::Rectangle<int> r,
                                              const juce::String& title, const juce::String& detail)
{
    g.setColour (panel); g.fillRoundedRectangle (r.toFloat(), 3);
    g.setColour (steel); g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 3, 1);
    g.setColour (juce::Colours::white.withAlpha (0.035f)); g.drawHorizontalLine (r.getY() + 1, static_cast<float> (r.getX() + 3), static_cast<float> (r.getRight() - 3));
    text (g, title, { r.getX() + 12, r.getY() + 4, r.getWidth() - 24, 20 }, 13, ink, juce::Font::bold);
    text (g, detail, { r.getX() + 12, r.getY() + 4, r.getWidth() - 24, 20 }, 9.5f, muted, 0, juce::Justification::centredRight);
    if (title.isNotEmpty())
    {
        g.setColour (steel.withAlpha (0.6f));
        g.drawHorizontalLine (r.getY() + 28, static_cast<float> (r.getX() + 1), static_cast<float> (r.getRight() - 1));
    }
}
void JekaborGMAudioProcessorEditor::drawKnobLabels (juce::Graphics& g)
{
    for (size_t i = 0; i < 19; ++i)
    {
        const auto r = knobs[i].slider.getBounds();
        text (g, knobs[i].label, { r.getX() - 4, r.getY() - 13, r.getWidth() + 8, 13 }, 11.5f, muted,
              juce::Font::bold, juce::Justification::centred);
    }
}
void JekaborGMAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (background);
    g.addTransform (juce::AffineTransform::scale (getWidth() / static_cast<float> (layout::width),
                                                 getHeight() / static_cast<float> (layout::height)));
    g.setColour (juce::Colour (0xff203242)); g.fillRect (0, 0, layout::width, 66);
    g.setColour (steel); g.drawHorizontalLine (65, 0, static_cast<float> (layout::width));
    g.setColour (accent); g.fillRect (layout::margin, 19, 4, 30);
    text (g, "JekaborGM", { 40, 10, 270, 36 }, 30, ink, juce::Font::bold);
    text (g, "GM SOUNDFONT MODULE", { 42, 44, 268, 15 }, 10, muted, juce::Font::bold);
    text (g, "JG-GM01", { 734, 12, 110, 19 }, 13, ink, juce::Font::bold, juce::Justification::centredRight);
    text (g, "16 PART / STEREO", { 734, 35, 110, 16 }, 9.5f, muted, 0, juce::Justification::centredRight);
    text (g, "SOUNDFONT", { 330, 13, 300, 14 }, 9.5f, muted);
    text (g, fontName.isEmpty() ? "INSERT SOUNDFONT" : fontName, { 330, 30, 300, 22 }, 13, ink);
    g.setColour (midiHold > 0 ? accent : steel); g.fillEllipse (656, 35, 6, 6);
    text (g, "MIDI IN", { 668, 27, 59, 22 }, 9.5f, muted);

    const auto display = layout::display;
    g.setColour (juce::Colour (0xff080f16)); g.fillRoundedRectangle (display.toFloat(), 3);
    g.setColour (steel); g.drawRoundedRectangle (display.toFloat().reduced (0.5f), 3, 1);
    const auto screen = display.reduced (7);
    juce::ColourGradient lcd (juce::Colour (0xff224640), 32, 85, juce::Colour (0xff17332f), 32, 198, false);
    g.setGradientFill (lcd); g.fillRect (screen);
    g.setColour (juce::Colours::black.withAlpha (0.025f));
    for (int y = 88; y < screen.getBottom(); y += 3)
        g.drawHorizontalLine (y, static_cast<float> (screen.getX()), static_cast<float> (screen.getRight()));
    lcdText (g, "CH", { 44, 91, 24, 24 }, 11, accent);
    lcdText (g, juce::String (channelSelector.getSelectedId()).paddedLeft ('0', 2), { 72, 87, 46, 30 }, 24, lcdInk, juce::Font::bold);
    lcdText (g, "BANK", { 143, 91, 42, 24 }, 11, accent);
    lcdText (g, three (displayedBank), { 189, 89, 76, 28 }, 18, lcdInk);
    lcdText (g, "PRG", { 282, 91, 32, 24 }, 11, accent);
    lcdText (g, three (displayedProgram + 1), { 318, 89, 66, 28 }, 18, lcdInk);
    lcdText (g, "VOICES", { 480, 91, 60, 24 }, 10.5f, accent);
    lcdText (g, three (processorRef.voices.load()), { 549, 89, 61, 28 }, 18, lcdInk, 0, juce::Justification::centredRight);
    g.setColour (accent.withAlpha (0.18f)); g.drawHorizontalLine (121, 44, 610);
    lcdText (g, instrumentName.toUpperCase(), { 44, 125, 566, 39 }, 32, lcdInk, juce::Font::bold);
    const bool ready = status.startsWith ("READY / ");
    lcdText (g, ready ? fontName : status, { 44, 170, 390, 20 }, 11.5f, ready ? accent : amber);
    const auto note = processorRef.lastNote.load();
    lcdText (g, note >= 0 ? "LAST " + juce::MidiMessage::getMidiNoteName (note, true, true, 3) + " / V" + three (processorRef.lastVelocity.load()) : "MIDI READY",
             { 444, 170, 166, 20 }, 10.5f, accent, 0, juce::Justification::centredRight);

    drawPanel (g, layout::output, "OUTPUT", "32-BIT FLOAT");
    for (int ch = 0; ch < 2; ++ch)
    {
        const int y = 142 + ch * 23;
        text (g, ch == 0 ? "L" : "R", { 736, y, 12, 15 }, 10.5f, muted);
        g.setColour (controlFill); g.fillRect (748, y, 91, 15);
        g.setColour (controlBorder); g.drawRect (748, y, 91, 15);
        const float db = juce::Decibels::gainToDecibels (meters[static_cast<size_t> (ch)], -60.0f);
        for (int segment = 0; segment < 14; ++segment)
        {
            const bool active = db > -54.0f + segment * 4.0f;
            g.setColour (active ? (segment > 11 ? amber : accent) : steel.withAlpha (0.45f));
            g.fillRect (752 + segment * 6, y + 4, 4, 7);
        }
    }
    text (g, "-54", { 748, 183, 24, 14 }, 9, muted);
    text (g, "-24", { 783, 183, 24, 14 }, 9, muted, 0, juce::Justification::centred);
    text (g, "0", { 820, 183, 19, 14 }, 9, muted, 0, juce::Justification::centredRight);
    g.setColour (clipHold > 0 ? juce::Colour (0xffe9957e) : steel); g.fillEllipse (833, 118, 5, 5);
    text (g, "CLIP", { 801, 112, 28, 16 }, 9, muted);

    drawPanel (g, layout::selection, {}, {});
    text (g, "CHANNEL", { 36, 221, 74, 16 }, 10, muted);
    text (g, "BANK", { 122, 221, 80, 16 }, 10, muted);
    text (g, "PROGRAM / INSTRUMENT", { 214, 221, 392, 16 }, 10, muted);
    text (g, "PRESET", { 618, 221, 84, 16 }, 10, muted);
    text (g, "SOUNDFONT", { 714, 221, 130, 16 }, 10, muted);

    const auto part = "PART " + juce::String (channelSelector.getSelectedId()).paddedLeft ('0', 2);
    drawPanel (g, layout::mixer, "MIXER / FX", part);
    drawPanel (g, layout::filter, "FILTER", {});
    drawPanel (g, layout::tone, "TONE", "OUTPUT EQ");
    drawPanel (g, layout::envelope, "ENVELOPE", "PRESET OFFSETS");
    drawPanel (g, layout::performance, "PERFORMANCE", {});
    text (g, part, { 612, 430, 100, 20 }, 9.5f, muted, 0, juce::Justification::centredRight);
    text (g, "MIDI / RESET", { 736, 430, 108, 20 }, 10, muted, juce::Font::bold, juce::Justification::centred);
    g.setColour (steel.withAlpha (0.6f)); g.drawVerticalLine (724, 427, 545);
    drawKnobLabels (g);
    text (g, "ALL PARTS", { 738, 460, 104, 16 }, 9.5f, muted, 0, juce::Justification::centred);
    text (g, "ALL NOTES OFF", { 738, 517, 104, 16 }, 9, muted, 0, juce::Justification::centred);
    text (g, "GENERAL MIDI / 16 CHANNELS", { 24, 548, 340, 11 }, 8, muted);
    text (g, "DRAG / WHEEL TO EDIT  /  DOUBLE-CLICK TO RESET", { 390, 548, 466, 11 }, 8, muted, 0, juce::Justification::centredRight);
    for (const auto point : { juce::Point<float> (10, 10), { 870, 10 }, { 10, 550 }, { 870, 550 } })
    {
        g.setColour (steel); g.fillEllipse (point.x - 2, point.y - 2, 4, 4);
        g.setColour (background); g.drawLine (point.x - 1, point.y, point.x + 1, point.y, 1);
    }
}
void JekaborGMAudioProcessorEditor::resized()
{
    controls.setTransform (juce::AffineTransform::scale (getWidth() / static_cast<float> (layout::width),
                                                       getHeight() / static_cast<float> (layout::height)));
    channelSelector.setBounds (36, 242, 74, 28); bankSelector.setBounds (122, 242, 80, 28);
    instrumentSelector.setBounds (214, 242, 392, 28);
    previous.setBounds (618, 242, 40, 28); next.setBounds (662, 242, 40, 28);
    selectSoundFont.setBounds (714, 242, 130, 28);
    filterSwitch.setBounds (484, 297, 56, 22);
    auto placeKnobs = [this] (size_t first, int count, juce::Rectangle<int> section, int controlWidth)
    {
        const auto body = section.reduced (layout::gutter, 0);
        const float cell = static_cast<float> (body.getWidth()) / static_cast<float> (count);
        for (int i = 0; i < count; ++i)
        {
            const int centre = juce::roundToInt (body.getX() + cell * (static_cast<float> (i) + 0.5f));
            knobs[first + static_cast<size_t> (i)].slider.setBounds (centre - controlWidth / 2,
                section.getY() + 48, controlWidth, 68);
        }
    };
    placeKnobs (0, 5, layout::mixer, 60);
    placeKnobs (5, 2, layout::filter, 76);
    placeKnobs (7, 3, layout::tone, 76);
    placeKnobs (10, 4, layout::envelope, 70);
    placeKnobs (14, 4, { 364, 426, 360, 120 }, 70);
    knobs[18].slider.setBounds (656, 123, 78, 78);
    panicButton.setBounds (738, 483, 104, 28);
}
