#include "ProView.h"
#include "../PluginProcessor.h"
#include "../Presets.h"
#include "../dsp/DeliveryStandards.h"

namespace cinelab
{

ProView::ProView (CineLabAudioProcessor& proc, Parameters& p)
    : processor (proc), parameters (p)
{
    // ================= meter =================
    meterPanel = std::make_unique<MeterPanel> (processor.getMeterReadouts(), -24.0);
    addAndMakeVisible (*meterPanel);

    // ================= Cinema EQ =================
    struct EqEntry { const juce::String& id; const char* name; const char* help; };
    const EqEntry eqEntries[] = {
        { IDs::eqRoomSize, "Room",      "Scales the whole ISO 2969 X-curve: 0 = flat studio, 1 = full large-auditorium curve." },
        { IDs::eqLowGain,  "Lows",      "X-curve low shelf below ~100 Hz. Typical cinema value: −4 dB (dry, solid lows without mud)." },
        { IDs::eqLowFreq,  "Lows Hz",   "Corner frequency of the low shelf." },
        { IDs::eqG2k,      "2 kHz",     "X-curve boost at 2 kHz (the tilt starts here at about +3 dB/octave)." },
        { IDs::eqG4k,      "4 kHz",     "X-curve boost at 4 kHz." },
        { IDs::eqG8k,      "8 kHz",     "X-curve boost at 8 kHz." },
        { IDs::eqAir,      "Air",       "High-shelf at 10 kHz: extra air when the auditorium swallows the highs." },
        { IDs::eqAirFreq,  "Air Hz",    "Corner frequency of the air shelf." },
    };

    for (auto& e : eqEntries)
        eqKnobs.push_back (std::make_unique<CineKnob> (juce::String (e.name), parameters.apvts,
                                                       juce::String (e.id), juce::String (e.help)));

    for (auto& k : eqKnobs)
        addAndMakeVisible (*k);

    static const char* roomShort[] = { "Large room", "Medium room", "Small room", "Flat" };
    static const char* roomHelp[] = {
        "Full ISO 2969 X-curve: large-auditorium tuning.",
        "A softened X-curve for medium rooms.",
        "Re-eq to home: the curve for small rooms.",
        "No room compensation (studio monitoring).",
    };
    for (int i = 0; i < getNumEqPresets(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (juce::String (roomShort[i]));
        b->setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        b->setColour (juce::TextButton::textColourOffId, theme::textDim);
        b->setTooltip (juce::String (roomHelp[i]));
        const int idx = i;
        b->onClick = [this, idx] { processor.applyEqPreset (idx); };
        roomButtons.push_back (std::move (b));
    }
    for (auto& b : roomButtons) addAndMakeVisible (*b);

    // ================= Scene =================
    const EqEntry sceneEntries[] = {
        { IDs::sceneHp,       "HP Hz",    "High-pass: removes rumble below this frequency (dialogue ~100 Hz, boom ~22 Hz)." },
        { IDs::sceneBodyFreq, "Body F",   "Center of the 'body' peak — the low-mid presence of the sound." },
        { IDs::sceneBodyGain, "Body G",   "How much body to add at that frequency." },
        { IDs::scenePresFreq, "Pres F",   "Center of the 'presence' peak — the intelligibility zone (2–5 kHz)." },
        { IDs::scenePresGain, "Pres G",   "How much presence to lift." },
        { IDs::sceneAirGain,  "Air G",    "Extra air shelf at 12 kHz for the scene." },
        { IDs::sceneDeEss,    "De-ess",   "Automatic de-essing: turns down sibilance above ~6.5 kHz when it gets loud." },
        { IDs::sceneComp,     "Comp",     "Glue compression: evens out levels for a consistent scene." },
        { IDs::sceneWidth,    "Width",    "Stereo width (mid/side). Only applies in pure stereo; ignored in surround." },
    };

    for (auto& e : sceneEntries)
        sceneKnobs.push_back (std::make_unique<CineKnob> (juce::String (e.name), parameters.apvts,
                                                          juce::String (e.id), juce::String (e.help)));

    for (auto& k : sceneKnobs)
        addAndMakeVisible (*k);

    const auto* presets = getScenePresets();
    for (int i = 0; i < getNumScenePresets(); ++i)
    {
        auto b = std::make_unique<juce::TextButton> (juce::String (presets[i].humanName));
        b->setColour (juce::TextButton::buttonColourId, theme::panelAlt);
        b->setColour (juce::TextButton::textColourOffId, theme::textDim);
        b->setTooltip (juce::String (presets[i].blurb));
        const int idx = i;
        b->onClick = [this, idx] { processor.applyScenePreset (idx); };
        scenePresetButtons.push_back (std::move (b));
    }
    for (auto& b : scenePresetButtons) addAndMakeVisible (*b);

    // ================= Delivery =================
    normToggle = std::make_unique<juce::ToggleButton> ("Normalize");
    normToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    normToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    normToggle->setTooltip ("Match the loudness to the delivery target (automatically, with hysteresis).");
    addAndMakeVisible (*normToggle);
    normAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::normEnable, *normToggle);

    targetKnob = std::make_unique<CineKnob> ("Target LUFS", parameters.apvts, IDs::targetLufs,
                                             "Loudness target when delivery is set to Manual.");
    manualGainKnob = std::make_unique<CineKnob> ("Manual gain", parameters.apvts, IDs::manualGain,
                                                 "Manual trim added on top of the automatic gain.");
    ceilingKnob = std::make_unique<CineKnob> ("Ceiling dBFS", parameters.apvts, IDs::limCeiling,
                                              "Peak ceiling for the limiter (approx. dBTP when True peak is on).");
    addAndMakeVisible (*targetKnob);
    addAndMakeVisible (*manualGainKnob);
    addAndMakeVisible (*ceilingKnob);

    limToggle = std::make_unique<juce::ToggleButton> ("Limiter");
    limToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    limToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    limToggle->setTooltip ("Brickwall peak protection for the delivery.");
    addAndMakeVisible (*limToggle);
    limAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::limEnable, *limToggle);

    resetButton = std::make_unique<juce::TextButton> ("Reset measurement");
    resetButton->setColour (juce::TextButton::buttonColourId, theme::panelAlt);
    resetButton->setColour (juce::TextButton::textColourOffId, theme::textDim);
    resetButton->setTooltip ("Restart the integrated loudness measurement from zero.");
    resetButton->onClick = [this] { processor.resetIntegratedMeter(); };
    addAndMakeVisible (*resetButton);

    // ================= Monitoring (preview) + Surround + Delivery ==========
    simToggle = std::make_unique<juce::ToggleButton> ("Listen in the room");
    simToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    simToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    simToggle->setTooltip ("Preview how the mix sounds inside the destination room (real acoustic loss scaled by room size). Monitoring aid only — turning it off restores the normal processed output.");
    simAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::simEnable, *simToggle);
    addAndMakeVisible (*simToggle);

    abToggle = std::make_unique<juce::ToggleButton> ("A/B — dry");
    abToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    abToggle->setColour (juce::ToggleButton::tickColourId, theme::cyan);
    abToggle->setTooltip ("Compare instantly: B = fully processed, A = dry input (only the meters stay active).");
    abAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::abBypass, *abToggle);
    addAndMakeVisible (*abToggle);

    downmixToggle = std::make_unique<juce::ToggleButton> ("Downmix 5.1→2.0");
    downmixToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    downmixToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    downmixToggle->setTooltip ("Fold surround to stereo (ITU-R BS.775). Applied before the limiter so the summed signal is protected too.");
    downmixAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::downmixEnable, *downmixToggle);
    addAndMakeVisible (*downmixToggle);

    tpToggle = std::make_unique<juce::ToggleButton> ("True peak");
    tpToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    tpToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    tpToggle->setTooltip ("Limit to the TRUE peak (BS.1770/EBU R128) analysed with 4× oversampling — what codecs and DA converters really reproduce.");
    tpAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::truePeakEnable, *tpToggle);
    addAndMakeVisible (*tpToggle);

    rearKnob = std::make_unique<CineKnob> ("Rears dB", parameters.apvts, IDs::surroundRear,
                                       "Gain for the rear/side surround channels (mastering). Applied before the limiter.");
    lfeKnob  = std::make_unique<CineKnob> ("LFE dB",   parameters.apvts, IDs::lfeLevel,
                                           "Gain for the LFE channel. The loudness meter excludes LFE (BS.1770).");
    addAndMakeVisible (*rearKnob);
    addAndMakeVisible (*lfeKnob);

    // ================= Bass management =================
    bmToggle = std::make_unique<juce::ToggleButton> ("Bass mgmt");
    bmToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    bmToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    bmToggle->setTooltip ("Bass management: routes the low end of the main channels to the LFE through a Linkwitz-Riley crossover (only in surround with an LFE channel).");
    bmAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::bmEnable, *bmToggle);
    addAndMakeVisible (*bmToggle);

    bmXoverKnob = std::make_unique<CineKnob> ("Xover Hz", parameters.apvts, IDs::bmCrossover,
                                              "Crossover frequency (Linkwitz-Riley 4th order) for the bass routing.");
    bmLfeKnob  = std::make_unique<CineKnob> ("LFE +dB", parameters.apvts, IDs::bmLfeGain,
                                             "Extra gain applied to the LFE channel by the bass manager.");
    addAndMakeVisible (*bmXoverKnob);
    addAndMakeVisible (*bmLfeKnob);

    bmSendToggle = std::make_unique<juce::ToggleButton> ("Send to LFE");
    bmSendToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    bmSendToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    bmSendToggle->setTooltip ("Sum the low end of all main channels into the LFE channel.");
    bmSendAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::bmSendToLfe, *bmSendToggle);
    addAndMakeVisible (*bmSendToggle);

    bmHpToggle = std::make_unique<juce::ToggleButton> ("HP mains");
    bmHpToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    bmHpToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    bmHpToggle->setTooltip ("High-pass the main channels below the crossover, so all the low end runs through the LFE (subwoofer routing).");
    bmHpAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::bmHpMain, *bmHpToggle);
    addAndMakeVisible (*bmHpToggle);

    refresh();
    resized();
}

// ============================================================================

void ProView::refresh()
{
    const double autoDb = processor.getNormAutoDb();
    const int destIdx = juce::jlimit (0, 5, (int) parameters.apvts.getRawParameterValue (IDs::destination)->load());
    const auto& dest = deliveryStandardFor (static_cast<Destination> (destIdx));

    const double target = destIdx == 5 ? parameters.getFloat (IDs::targetLufs) : dest.targetLufs;
    meterPanel->setTarget (target);

    autoGainLabel.setText ("Automatic gain: " + juce::String (autoDb >= 0.0 ? "+" : "") + juce::String (autoDb, 1) + " dB"
                             + "   (target " + juce::String (dest.humanName) + " · " + juce::String (target, 0) + " LUFS)",
                           juce::dontSendNotification);

    grLabel.setText ("Limiter reduction: " + juce::String (processor.getLimiterReductionDb(), 1) + " dB"
                       + "   ·   Total norm: " + juce::String (processor.getNormGainDb(), 1) + " dB",
                     juce::dontSendNotification);
}

// ============================================================================

void ProView::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    theme::drawSectionTitle (g, "Cinema EQ — X-curve (ISO 2969)", juce::Rectangle<int> (20, 174, 280, 18));
    theme::drawSectionTitle (g, "Scene", juce::Rectangle<int> (360, 174, 200, 18));
    theme::drawSectionTitle (g, "Delivery · Monitoring · Surround", juce::Rectangle<int> (700, 174, 280, 18));
}

void ProView::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    meterPanel->setBounds (20, 16, w - 40, 150);

    const int knobSize = 56;

    // EQ: 4 columns x 2 rows
    for (int i = 0; i < (int) eqKnobs.size(); ++i)
        eqKnobs[(size_t) i]->setBounds (20 + (i % 4) * 78, 196 + (i / 4) * (knobSize + 26), knobSize, knobSize);

    for (int i = 0; i < (int) roomButtons.size(); ++i)
        roomButtons[(size_t) i]->setBounds (20 + i * 78, 340, 74, 24);

    // bass management (left column, under the EQ knobs/presets)
    bmToggle->setBounds     (20, 384, 130, 24);
    bmXoverKnob->setBounds  (20, 414, knobSize, knobSize);
    bmLfeKnob->setBounds    (82, 414, knobSize, knobSize);
    bmSendToggle->setBounds (20, 478, 130, 24);
    bmHpToggle->setBounds   (20, 508, 130, 24);

    // Scene: 5 columns x 2 rows
    for (int i = 0; i < (int) sceneKnobs.size(); ++i)
        sceneKnobs[(size_t) i]->setBounds (360 + (i % 5) * 64, 196 + (i / 5) * (knobSize + 26), knobSize, knobSize);

    // Scene presets: 2 compact rows, always LEFT of x=700
    for (int i = 0; i < (int) scenePresetButtons.size(); ++i)
        scenePresetButtons[(size_t) i]->setBounds (360 + (i % 4) * 76, 340 + (i / 4) * 28, 72, 24);

    // ---- right column: Delivery · Monitoring · Surround ----
    normToggle->setBounds (700, 196, 150, 22);
    targetKnob->setBounds   (700, 222, knobSize, knobSize);
    manualGainKnob->setBounds (760, 222, knobSize, knobSize);

    autoGainLabel.setBounds (700, 284, 256, 18);

    ceilingKnob->setBounds (700, 304, knobSize, knobSize);
    limToggle->setBounds   (760, 310, 150, 22);

    grLabel.setBounds (700, 366, 256, 18);

    simToggle->setBounds (700, 388, 190, 24);
    abToggle->setBounds  (700, 412, 190, 24);

    rearKnob->setBounds (700, 436, knobSize, knobSize);
    lfeKnob->setBounds  (760, 436, knobSize, knobSize);

    // downmix + true-peak (bottom-left free zone, never touching x=700)
    downmixToggle->setBounds (360, 470, 190, 24);
    tpToggle->setBounds      (360, 500, 190, 24);

    resetButton->setBounds (700, h - 30, 170, 24);
}

} // namespace cinelab