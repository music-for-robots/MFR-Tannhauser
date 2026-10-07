// MFR-Tannhäuser - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginEditor.h"

namespace
{
juce::Font font (float size, bool bold = false)
{
    return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
}
} // namespace

//==============================================================================
TannLookAndFeel::TannLookAndFeel()
{
    setColour (juce::Label::textColourId, colours::ink);
    setColour (juce::TextEditor::textColourId, colours::ink);
    setColour (juce::TextButton::textColourOffId, colours::ink);
}

void TannLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                        float startAngle, float endAngle, juce::Slider&)
{
    const auto bounds = juce::Rectangle<int> (x, y, w, h).toFloat().reduced (4.0f);
    const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const auto centre = bounds.getCentre();
    const float angle = startAngle + pos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, endAngle, true);
    g.setColour (colours::trackOff);
    g.strokePath (track, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    juce::Path value;
    value.addCentredArc (centre.x, centre.y, radius - 2, radius - 2, 0, startAngle, angle, true);
    g.setColour (colours::red);
    g.strokePath (value, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

    // Flat, stark knob: off-white cap, black pointer.
    const float knobR = radius - 8.0f;
    g.setColour (colours::ink);
    g.fillEllipse (juce::Rectangle<float> (knobR * 2, knobR * 2).withCentre (centre));
    juce::Path pointer;
    pointer.addRectangle (-1.5f, -knobR + 2.0f, 3.0f, knobR * 0.6f);
    g.setColour (colours::bg);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre));
}

void TannLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    const bool on = b.getToggleState();
    g.setColour (on ? colours::red : (highlighted ? colours::panel.brighter (0.15f) : colours::panel.darker (0.3f)));
    g.fillRect (r);
    g.setColour (on ? colours::red : colours::edge.brighter (0.2f));
    g.drawRect (r, 1.0f);
    g.setColour (on ? colours::ink : colours::dimInk);
    g.setFont (font (r.getHeight() > 34 ? 18.0f : 12.0f, true));
    g.drawText (b.getButtonText(), r, juce::Justification::centred);
}

void TannLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f);
    g.setColour (down ? colours::panel.brighter (0.2f) : highlighted ? colours::panel.brighter (0.1f) : colours::panel);
    g.fillRect (r);
    g.setColour (colours::ink.withAlpha (0.6f));
    g.drawRect (r, 1.5f);
}

//==============================================================================
void FineSlider::mouseDown (const juce::MouseEvent& e)
{
    fineDrag = e.mods.isCtrlDown() || e.mods.isShiftDown();
    lastPos = e.position;
    Slider::mouseDown (e);
}

void FineSlider::mouseDrag (const juce::MouseEvent& e)
{
    if (! fineDrag || ! isEnabled()) { Slider::mouseDrag (e); return; }
    const float delta = (e.position.x - lastPos.x) + (lastPos.y - e.position.y);
    lastPos = e.position;
    const double pos = juce::jlimit (0.0, 1.0, valueToProportionOfLength (getValue()) + delta / 2000.0);
    setValue (proportionOfLengthToValue (pos), juce::sendNotificationSync);
}

//==============================================================================
Knob::Knob (juce::AudioProcessorValueTreeState& state, const juce::String& id, const juce::String& title)
    : attachment (state, id, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 72, 16);
    slider.setRotaryParameters (juce::degreesToRadians (225.0f), juce::degreesToRadians (495.0f), true);
    slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    slider.setColour (juce::Slider::textBoxBackgroundColourId, colours::bg);
    slider.setColour (juce::Slider::textBoxTextColourId, colours::ink);
    slider.setColour (juce::Slider::textBoxHighlightColourId, colours::red.withAlpha (0.5f));
    if (auto* p = state.getParameter (id))
        slider.setDoubleClickReturnValue (true, p->convertFrom0to1 (p->getDefaultValue()));
    addAndMakeVisible (slider);

    label.setText (title.toUpperCase(), juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (font (12.0f, true));
    label.setColour (juce::Label::textColourId, colours::ink);
    addAndMakeVisible (label);
}

void Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (16));
    slider.setBounds (r);
}

//==============================================================================
TannEditor::TannEditor (TannProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      brightAttachment (p.apvts, "cbright", brightToggle), boostAttachment (p.apvts, "boost", boostToggle)
{
    setLookAndFeel (&lnf);

    addKnob (cleanKnobs, "cvolume", "Volume");
    addKnob (cleanKnobs, "cbass", "Bass");
    addKnob (cleanKnobs, "cmid", "Mid");
    addKnob (cleanKnobs, "ctreble", "Treble");
    addKnob (cleanKnobs, "spring", "Spring");
    addKnob (dirtyKnobs, "preamp", "Preamp");
    addKnob (dirtyKnobs, "dbass", "Bass");
    addKnob (dirtyKnobs, "dmiddle", "Middle");
    addKnob (dirtyKnobs, "dtreble", "Treble");
    addKnob (dirtyKnobs, "presence", "Presence");
    addKnob (dirtyKnobs, "master", "Master");
    addKnob (ioKnobs, "gate", "Gate");
    addKnob (ioKnobs, "output", "Output");
    addKnob (studioKnobs, "room", "Room");
    addKnob (studioKnobs, "tape", "Tape");
    addKnob (studioKnobs, "double", "Double");

    brightToggle.setTooltip ("Extra top end on the clean channel");
    boostToggle.setTooltip ("A mid-hump boost pedal in front of the head: tighter, more gain");
    cleanButton.setTooltip ("Solid-state combo with spring reverb");
    dirtyButton.setTooltip ("British 50 W head into a 4x12");
    cleanButton.setClickingTogglesState (false);
    dirtyButton.setClickingTogglesState (false);
    cleanButton.onClick = [this] { setChannel (false); };
    dirtyButton.onClick = [this] { setChannel (true); };
    for (juce::Component* c : std::initializer_list<juce::Component*> { &brightToggle, &boostToggle, &cleanButton, &dirtyButton })
        addAndMakeVisible (c);

    initButton.onClick = [this] { resetToDefaults(); };
    initButton.setTooltip ("Reset every control to its default");
    addAndMakeVisible (initButton);

    setSize (940, 420);
    timerCallback();
    startTimerHz (30);
}

TannEditor::~TannEditor() { setLookAndFeel (nullptr); }

Knob& TannEditor::addKnob (std::vector<std::unique_ptr<Knob>>& group, const char* id, const char* title)
{
    auto& k = *group.emplace_back (std::make_unique<Knob> (proc.apvts, id, title));
    addAndMakeVisible (k);
    return k;
}

void TannEditor::setChannel (bool dirty)
{
    if (auto* p = proc.apvts.getParameter ("dirty"))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (dirty ? 1.0f : 0.0f);
        p->endChangeGesture();
    }
}

void TannEditor::resetToDefaults()
{
    for (auto* param : proc.getParameters())
        if (auto* p = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->getDefaultValue());
            p->endChangeGesture();
        }
}

void TannEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::bg);

    g.setColour (colours::ink);
    g.setFont (font (32.0f, true));
    g.drawText (juce::String (juce::CharPointer_UTF8 ("MFR-TANNH\xc3\x84USER")), 24, 10, 330, 40, juce::Justification::centredLeft);
    g.setColour (colours::red);
    g.fillRect (24, 50, 330, 3);
    g.setColour (colours::dimInk);
    g.setFont (font (12.0f, true));
    g.drawText ("SOLID-STATE CLEAN  |  BRITISH 50 W DIRT  |  CLOSE + ROOM  |  2-INCH TAPE", 370, 14, 460, 18, juce::Justification::centredLeft);
    g.setFont (font (10.0f));
    g.drawText ("v" JucePlugin_VersionString "  |  musicforrobots.com", 370, 31, 300, 14, juce::Justification::centredLeft);

    const std::pair<juce::Rectangle<int>, const char*> panels[] = {
        { cleanPanel, "CLEAN" }, { dirtyPanel, "DIRTY" }, { ioPanel, "IN / OUT" }, { channelPanel, "CHANNEL" }, { studioPanel, "STUDIO" }
    };
    for (auto& [r, name] : panels)
    {
        g.setColour (colours::panel);
        g.fillRect (r);
        g.setColour (colours::edge);
        g.drawRect (r, 1);
        g.setColour (colours::ink);
        g.setFont (font (13.0f, true));
        g.drawText (name, r.getX() + 12, r.getY() + 6, 200, 16, juce::Justification::centredLeft);
    }

    const float gl = juce::jlimit (0.0f, 1.0f, shownGate);
    g.setColour (colours::trackOff);
    g.fillEllipse (gateLed.toFloat());
    g.setColour (colours::red.withAlpha (gl));
    g.fillEllipse (gateLed.toFloat().reduced (1.0f));

    g.setColour (colours::dimInk);
    g.setFont (font (10.0f));
    g.drawText ("Pick a channel: the other one's knobs are kept for when you switch back. Double-click resets a knob.",
                24, getHeight() - 26, getWidth() - 48, 14, juce::Justification::centredLeft);
}

void TannEditor::paintOverChildren (juce::Graphics& g)
{
    // Dim whichever channel isn't playing.
    g.setColour (colours::bg.withAlpha (0.55f));
    g.fillRect (shownDirty ? cleanPanel : dirtyPanel);
}

void TannEditor::resized()
{
    const int kw = 78, kh = 106, top = 66, rowH = 150, side = 110;
    auto row = [&] (std::vector<std::unique_ptr<Knob>>& knobs, juce::Rectangle<int> panel)
    {
        const int used = kw * (int) knobs.size();
        auto area = panel.withTrimmedTop (30).withTrimmedLeft ((panel.getWidth() - used) / 2);
        for (auto& k : knobs)
            k->setBounds (area.removeFromLeft (kw).withHeight (kh));
    };

    initButton.setBounds (getWidth() - 96, 18, 80, 26);

    cleanPanel   = { 16, top, 5 * kw + 24 + side, rowH };
    ioPanel      = { cleanPanel.getRight() + 10, top, 2 * kw + 24, rowH };
    channelPanel = { ioPanel.getRight() + 10, top, getWidth() - ioPanel.getRight() - 26, rowH };

    const int top2 = top + rowH + 10;
    dirtyPanel  = { 16, top2, 6 * kw + 24 + side, rowH };
    studioPanel = { dirtyPanel.getRight() + 10, top2, getWidth() - dirtyPanel.getRight() - 26, rowH };

    row (cleanKnobs, cleanPanel.withTrimmedRight (side));
    brightToggle.setBounds (cleanPanel.getRight() - side + 4, cleanPanel.getY() + 62, side - 20, 28);
    row (dirtyKnobs, dirtyPanel.withTrimmedRight (side));
    boostToggle.setBounds (dirtyPanel.getRight() - side + 4, dirtyPanel.getY() + 62, side - 20, 28);
    row (ioKnobs, ioPanel);
    gateLed = { ioPanel.getX() + kw / 2 + 12 + 30, ioPanel.getY() + 30, 8, 8 };

    const auto ch = channelPanel.withTrimmedTop (30).reduced (14, 8);
    cleanButton.setBounds (ch.withHeight (ch.getHeight() / 2 - 4));
    dirtyButton.setBounds (ch.withTrimmedTop (ch.getHeight() / 2 + 4));

    row (studioKnobs, studioPanel);
}

void TannEditor::timerCallback()
{
    const bool dirty = proc.apvts.getRawParameterValue ("dirty")->load() > 0.5f;
    if (dirty != shownDirty || cleanButton.getToggleState() == dirtyButton.getToggleState())
    {
        shownDirty = dirty;
        cleanButton.setToggleState (! dirty, juce::dontSendNotification);
        dirtyButton.setToggleState (dirty, juce::dontSendNotification);
        repaint();
    }
    const float g = proc.gateMeter.load();
    if (std::abs (g - shownGate) > 0.02f)
    {
        shownGate = g;
        repaint (gateLed.expanded (2));
    }
}
