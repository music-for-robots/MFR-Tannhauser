// MFR-Tannhäuser - Copyright (C) 2026 Music For Robots
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
using FloatParam = juce::AudioParameterFloat;
using Attrs = juce::AudioParameterFloatAttributes;

std::unique_ptr<FloatParam> tenScale (const juce::String& id, const juce::String& name, float def)
{
    return std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, juce::NormalisableRange<float> (0.0f, 10.0f), def,
                                         Attrs().withStringFromValueFunction ([] (float v, int) { return juce::String (v, 1); }));
}

std::unique_ptr<juce::AudioParameterBool> toggle (const juce::String& id, const juce::String& name, bool def)
{
    return std::make_unique<juce::AudioParameterBool> (juce::ParameterID { id, 1 }, name, def);
}
} // namespace

juce::AudioProcessorValueTreeState::ParameterLayout TannProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    const tann::Params d;

    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "gate", 1 }, "Gate",
                                              juce::NormalisableRange<float> (-90.0f, -30.0f), d.gateDb,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return v <= -89.5f ? juce::String ("Off") : juce::String (v, 0) + " dB"; })));
    layout.add (std::make_unique<juce::AudioParameterBool> (juce::ParameterID { "dirty", 1 }, "Channel", d.dirty,
                                                            juce::AudioParameterBoolAttributes().withStringFromValueFunction (
                                                                [] (bool v, int) { return v ? juce::String ("Dirty") : juce::String ("Clean"); })));
    layout.add (tenScale ("cvolume", "Clean Volume", d.cVolume));
    layout.add (tenScale ("cbass", "Clean Bass", d.cBass));
    layout.add (tenScale ("cmid", "Clean Mid", d.cMid));
    layout.add (tenScale ("ctreble", "Clean Treble", d.cTreble));
    layout.add (toggle ("cbright", "Clean Bright", d.cBright));
    layout.add (tenScale ("spring", "Spring", d.spring * 10.0f));
    layout.add (toggle ("boost", "Boost", d.boost));
    layout.add (tenScale ("preamp", "Preamp", d.preamp));
    layout.add (tenScale ("dbass", "Dirty Bass", d.dBass));
    layout.add (tenScale ("dmiddle", "Dirty Middle", d.dMiddle));
    layout.add (tenScale ("dtreble", "Dirty Treble", d.dTreble));
    layout.add (tenScale ("presence", "Presence", d.presence));
    layout.add (tenScale ("master", "Master", d.master));
    layout.add (tenScale ("room", "Room", d.room * 10.0f));
    layout.add (tenScale ("tape", "Tape", d.tape * 10.0f));
    layout.add (tenScale ("double", "Double", d.width * 10.0f));
    layout.add (std::make_unique<FloatParam> (juce::ParameterID { "output", 1 }, "Output",
                                              juce::NormalisableRange<float> (-36.0f, 12.0f), d.outputDb,
                                              Attrs().withStringFromValueFunction ([] (float v, int)
                                              { return (v > 0.05f ? "+" : "") + juce::String (v, 1) + " dB"; })));
    return layout;
}

TannProcessor::TannProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PARAMS", createLayout())
{
    auto get = [this] (const char* id) { return apvts.getRawParameterValue (id); };
    pGate = get ("gate"); pDirty = get ("dirty");
    pCVolume = get ("cvolume"); pCBass = get ("cbass"); pCMid = get ("cmid"); pCTreble = get ("ctreble");
    pCBright = get ("cbright"); pSpring = get ("spring");
    pBoost = get ("boost"); pPreamp = get ("preamp"); pDBass = get ("dbass"); pDMiddle = get ("dmiddle");
    pDTreble = get ("dtreble"); pPresence = get ("presence"); pMaster = get ("master");
    pRoom = get ("room"); pTape = get ("tape"); pDouble = get ("double"); pOutput = get ("output");
}

bool TannProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto in = layouts.getMainInputChannelSet(), out = layouts.getMainOutputChannelSet();
    const bool inOk = in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
    return inOk && out == juce::AudioChannelSet::stereo();
}

void TannProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    work.setSize (2, samplesPerBlock);
    oversampling.reset();
    oversampling.initProcessing ((size_t) samplesPerBlock);
    chain.prepare (sampleRate * (1 << kOversampleStages));
    setLatencySamples (juce::roundToInt (oversampling.getLatencyInSamples()));
}

void TannProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const int n = buffer.getNumSamples();
    const int numIn = getTotalNumInputChannels();
    if (work.getNumSamples() < n)
        work.setSize (2, n, false, false, true);

    work.clear();
    for (int ch = 0; ch < numIn; ++ch)
        work.addFrom (0, 0, buffer, ch, 0, n, 1.0f / (float) numIn);

    tann::Params p;
    p.gateDb = pGate->load();
    p.dirty = pDirty->load() > 0.5f;
    p.cVolume = pCVolume->load(); p.cBass = pCBass->load(); p.cMid = pCMid->load(); p.cTreble = pCTreble->load();
    p.cBright = pCBright->load() > 0.5f;
    p.spring = pSpring->load() / 10.0f;
    p.boost = pBoost->load() > 0.5f;
    p.preamp = pPreamp->load(); p.dBass = pDBass->load(); p.dMiddle = pDMiddle->load(); p.dTreble = pDTreble->load();
    p.presence = pPresence->load(); p.master = pMaster->load();
    p.room = pRoom->load() / 10.0f; p.tape = pTape->load() / 10.0f; p.width = pDouble->load() / 10.0f;
    p.outputDb = pOutput->load();
    chain.setParams (p);

    juce::dsp::AudioBlock<float> block (work.getArrayOfWritePointers(), 2, (size_t) n);
    auto up = oversampling.processSamplesUp (block);
    float* l = up.getChannelPointer (0);
    float* r = up.getChannelPointer (1);
    for (size_t i = 0; i < up.getNumSamples(); ++i)
        chain.process (l[i], l[i], r[i]);
    oversampling.processSamplesDown (block);

    buffer.copyFrom (0, 0, work, 0, 0, n);
    buffer.copyFrom (1, 0, work, 1, 0, n);
    gateMeter = chain.getGate();
}

void TannProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void TannProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessorEditor* TannProcessor::createEditor() { return new TannEditor (*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new TannProcessor(); }
