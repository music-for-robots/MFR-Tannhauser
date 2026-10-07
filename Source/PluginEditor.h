// MFR-Tannhäuser - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

namespace colours
{
const juce::Colour bg       { 0xff0c0c0c };   // black
const juce::Colour panel    { 0xff181818 };
const juce::Colour edge     { 0xff3a3a3a };
const juce::Colour ink      { 0xfff2efe8 };   // off-white
const juce::Colour dimInk   { 0xff9a968e };
const juce::Colour trackOff { 0xff2c2c2c };
const juce::Colour red      { 0xffe0322b };   // punk red
} // namespace colours

class TannLookAndFeel : public juce::LookAndFeel_V4
{
public:
    TannLookAndFeel();
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&, bool highlighted, bool down) override;
};

class FineSlider : public juce::Slider
{
public:
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
private:
    bool fineDrag = false;
    juce::Point<float> lastPos;
};

class Knob : public juce::Component
{
public:
    Knob (juce::AudioProcessorValueTreeState&, const juce::String& paramId, const juce::String& title);
    void resized() override;
private:
    FineSlider slider;
    juce::Label label;
    juce::AudioProcessorValueTreeState::SliderAttachment attachment;
};

class TannEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit TannEditor (TannProcessor&);
    ~TannEditor() override;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    Knob& addKnob (std::vector<std::unique_ptr<Knob>>&, const char* id, const char* title);
    void setChannel (bool dirty);
    void resetToDefaults();

    TannProcessor& proc;
    TannLookAndFeel lnf;

    std::vector<std::unique_ptr<Knob>> cleanKnobs, dirtyKnobs, ioKnobs, studioKnobs;
    juce::ToggleButton brightToggle { "BRIGHT" }, boostToggle { "BOOST" };
    juce::ToggleButton cleanButton { "CLEAN" }, dirtyButton { "DIRTY" };
    juce::AudioProcessorValueTreeState::ButtonAttachment brightAttachment, boostAttachment;
    juce::TextButton initButton { "INIT" };
    juce::Rectangle<int> cleanPanel, dirtyPanel, ioPanel, channelPanel, studioPanel, gateLed;
    float shownGate = -1.0f;
    bool shownDirty = true;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TannEditor)
};
