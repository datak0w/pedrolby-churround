#include "DirectorView.h"
#include "../PluginProcessor.h"
#include "../Presets.h"
#include "../dsp/DeliveryStandards.h"

namespace cinelab
{

DirectorView::DirectorView (CineLabAudioProcessor& proc, Parameters& p)
    : processor (proc), parameters (p)
{
    // ---- labels ----
    detailLabel.setColour (juce::Label::textColourId, theme::textDim);
    detailLabel.setFont (theme::uiFont (11.5f));
    detailLabel.setJustificationType (juce::Justification::topLeft);
    addAndMakeVisible (detailLabel);

    summaryLabel.setColour (juce::Label::textColourId, theme::textFaint);
    summaryLabel.setFont (theme::uiFont (10.5f));
    summaryLabel.setJustificationType (juce::Justification::centredLeft);
    addAndMakeVisible (summaryLabel);

    // ---- scene buttons ----
    const auto* presets = getScenePresets();
    for (int i = 0; i < getNumScenePresets(); ++i)
    {
        auto b = std::make_unique<ChoiceButton> (juce::String (presets[i].humanName),
                                                 juce::String (presets[i].blurb).substring (0, 24));
        b->setRadioGroupId (1001);
        b->setTooltip (juce::String (presets[i].blurb));
        sceneButtons.push_back (std::move (b));
    }

    for (auto& b : sceneButtons)
        addAndMakeVisible (*b);

    // ---- delivery target buttons ----
    for (int i = 0; i < 6; ++i)
    {
        const auto& s = deliveryStandardFor (static_cast<Destination> (i));
        auto b = std::make_unique<ChoiceButton> (juce::String (s.humanName), juce::String(), true);
        b->setRadioGroupId (1002);
        b->setTooltip (juce::String (s.name) + " — " + juce::String (s.blurb));
        destButtons.push_back (std::move (b));
    }

    for (auto& b : destButtons)
        addAndMakeVisible (*b);

    // ---- screening room buttons (X-curve) ----
    struct RoomLabel
    {
        const char* name;
        const char* sub;
        const char* help;
    };
    static const RoomLabel roomLabels[] = {
        { "Large room",  "Full X-curve",      "Full ISO 2969 X-curve: the big-screen auditorium tuning. Compensates strong high-frequency absorption." },
        { "Medium room", "X-curve scaled",    "Medium auditorium: a softened X-curve." },
        { "Small room",  "Re-eq to home",     "Small rooms need less compensation — the curve used to re-eq cinema mixes for home." },
        { "Flat",        "Studio monitoring", "No room compensation: neutral monitoring." },
    };

    for (int i = 0; i < getNumEqPresets(); ++i)
    {
        auto b = std::make_unique<ChoiceButton> (juce::String (roomLabels[i].name), juce::String (roomLabels[i].sub));
        b->setRadioGroupId (1003);
        b->setTooltip (juce::String (roomLabels[i].help));
        roomButtons.push_back (std::move (b));
    }

    for (auto& b : roomButtons)
        addAndMakeVisible (*b);

    // ---- parameter ↔ buttons bridges ----
    std::vector<ChoiceButton*> scenes;
    for (auto& b : sceneButtons) scenes.push_back (b.get());
    sceneChooser = std::make_unique<ChoiceToButtons> (parameters.apvts, IDs::scene, scenes);

    std::vector<ChoiceButton*> dests;
    for (auto& b : destButtons) dests.push_back (b.get());
    destChooser = std::make_unique<ChoiceToButtons> (parameters.apvts, IDs::destination, dests);

    // ---- actions ----
    for (int i = 0; i < (int) sceneButtons.size(); ++i)
    {
        sceneButtons[(size_t) i]->onClick = [this, i]
        {
            if (sceneButtons[(size_t) i]->getToggleState())
                onSceneClicked (i);
        };
    }

    for (int i = 0; i < (int) destButtons.size(); ++i)
    {
        destButtons[(size_t) i]->onClick = [this, i]
        {
            if (destButtons[(size_t) i]->getToggleState())
                onDestinationClicked (i);
        };
    }

    for (int i = 0; i < (int) roomButtons.size(); ++i)
    {
        const int idx = i;
        roomButtons[(size_t) i]->onClick = [this, idx]
        {
            if (roomButtons[(size_t) idx]->getToggleState())
            {
                lastRoomIndex = idx;
                processor.applyEqPreset (idx);
                repaint();
            }
        };
    }

    // ---- intensity ----
    intensityKnob = std::make_unique<CineKnob> ("INTENSITY", parameters.apvts, IDs::intensity,
                                                "How much of the artistic processing to apply: 0 = subtle/flat, 1 = full cinema treatment. Normalization and delivery limits are unaffected.");
    intensityKnob->setRange (0.0, 1.0, 0.01);
    intensityKnob->setValue (1.0, juce::dontSendNotification);
    intensityKnob->setDoubleClickReturnValue (true, 1.0);
    addAndMakeVisible (*intensityKnob);

    // ---- meter ----
    meterPanel = std::make_unique<MeterPanel> (processor.getMeterReadouts(), -24.0);
    addAndMakeVisible (*meterPanel);

    // ---- reset integrated ----
    resetButton = std::make_unique<juce::TextButton> ("Reset integrated measurement");
    resetButton->setColour (juce::TextButton::buttonColourId, theme::panelAlt);
    resetButton->setColour (juce::TextButton::textColourOffId, theme::textDim);
    resetButton->setColour (juce::TextButton::textColourOnId, theme::textDim);
    resetButton->setTooltip ("Restart the integrated loudness measurement from zero.");
    resetButton->onClick = [this] { processor.resetIntegratedMeter(); };
    addAndMakeVisible (*resetButton);

    // ---- monitoring + delivery toggles ----
    simToggle = std::make_unique<juce::ToggleButton> ("Listen in the room");
    simToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    simToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    simToggle->setTooltip ("Preview how the mix will sound inside the destination room: the room absorbs highs and builds up lows. Monitoring aid only — turning it off restores the normal processed output.");
    simAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::simEnable, *simToggle);
    addAndMakeVisible (*simToggle);

    abToggle = std::make_unique<juce::ToggleButton> ("A/B — dry");
    abToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    abToggle->setColour (juce::ToggleButton::tickColourId, theme::cyan);
    abToggle->setTooltip ("Compare instantly: B = fully processed, A = dry input (everything bypassed except the meters).");
    abAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::abBypass, *abToggle);
    addAndMakeVisible (*abToggle);

    downmixToggle = std::make_unique<juce::ToggleButton> ("Downmix 5.1→2.0");
    downmixToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    downmixToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    downmixToggle->setTooltip ("Fold surround to stereo (ITU-R BS.775): L' = L + 0.707·C + 0.707·Ls, with LFE at −6 dB. The other channels are silenced for a stereo print.");
    downmixAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::downmixEnable, *downmixToggle);
    addAndMakeVisible (*downmixToggle);

    tpToggle = std::make_unique<juce::ToggleButton> ("True peak");
    tpToggle->setColour (juce::ToggleButton::textColourId, theme::textDim);
    tpToggle->setColour (juce::ToggleButton::tickColourId, theme::accent);
    tpToggle->setTooltip ("Limit to the TRUE peak (BS.1770/EBU R128), analysed with 4× oversampling — the level the DA converter and codecs actually see.");
    tpAttach = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (parameters.apvts, IDs::truePeakEnable, *tpToggle);
    addAndMakeVisible (*tpToggle);

    refresh();
    resized();
}

// ============================================================================

void DirectorView::onSceneClicked (int index)
{
    sceneChooser->setSelected (index);
    processor.applyScenePreset (index);
    refresh();
}

void DirectorView::onDestinationClicked (int index)
{
    destChooser->setSelected (index);
    refresh();
}

// ============================================================================

void DirectorView::refresh()
{
    const int sceneIdx  = sceneChooser->getSelectedIndex();
    const int destIdx   = destChooser->getSelectedIndex();

    const auto* presets = getScenePresets();
    const auto& dest = deliveryStandardFor (static_cast<Destination> (destIdx));

    juce::String detail;
    if (sceneIdx >= 0)
        detail << presets[sceneIdx].humanName << " — " << presets[sceneIdx].blurb << "\n\n";

    detail << "TARGET: " << dest.humanName << "\n"
           << dest.blurb << "\n"
           << juce::String (dest.targetLufs, 0) << " LUFS · ceiling " << juce::String (dest.ceilingDb, 0) << " dB";
    detailLabel.setText (detail, juce::dontSendNotification);

    const double autoDb = processor.getNormAutoDb();
    const double totalDb = processor.getNormGainDb();

    summaryLabel.setText ("Scene: " + juce::String (sceneIdx >= 0 ? presets[sceneIdx].humanName : "--")
                            + "   ·   Normalization: "
                            + juce::String (autoDb >= 0.0 ? "+" : "") + juce::String (autoDb, 1) + " dB auto"
                            + "   ·   Total: " + juce::String (totalDb >= 0.0 ? "+" : "") + juce::String (totalDb, 1) + " dB"
                            + "   ·   Limiter: " + juce::String (processor.getLimiterReductionDb(), 1) + " dB GR",
                          juce::dontSendNotification);

    meterPanel->setTarget (destIdx == 5 ? parameters.getFloat (IDs::targetLufs) : dest.targetLufs);

    if (lastDestIndex != destIdx)
    {
        lastDestIndex = destIdx;
        repaint();
    }
}

// ============================================================================

void DirectorView::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    theme::drawSectionTitle (g, "Scene", juce::Rectangle<int> (20, 8, 200, 18));
    theme::drawSectionTitle (g, "Delivery target", juce::Rectangle<int> (350, 8, 220, 18));
    theme::drawSectionTitle (g, "Screening room", juce::Rectangle<int> (640, 8, 200, 18));
}

void DirectorView::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // scenes: 3 columns x 3 rows
    const int sceneW = 100;
    const int sceneH = 50;
    for (int i = 0; i < (int) sceneButtons.size(); ++i)
        sceneButtons[(size_t) i]->setBounds (20 + (i % 3) * (sceneW + 8), 30 + (i / 3) * (sceneH + 8), sceneW, sceneH);

    // delivery targets: 3 columns x 2 rows (width 90 → right edge at 632,
    // never touching the room column that starts at x=640)
    const int destW = 90;
    for (int i = 0; i < (int) destButtons.size(); ++i)
        destButtons[(size_t) i]->setBounds (350 + (i % 3) * (destW + 6), 30 + (i / 3) * 38, destW, 32);

    // rooms: 2 columns x 2 rows
    const int roomW = 126;
    for (int i = 0; i < (int) roomButtons.size(); ++i)
        roomButtons[(size_t) i]->setBounds (640 + (i % 2) * (roomW + 8), 30 + (i / 2) * 40, roomW, 34);

    // scene/target blurb
    detailLabel.setBounds (20, 30 + 3 * (sceneH + 8), 310, 100);

    // intensity (the star of Director mode)
    intensityKnob->setBounds (350, 118, 190, 190);

    // monitoring + delivery toggles (below intensity)
    simToggle->setBounds     (350, 322, 210, 26);
    abToggle->setBounds      (350, 352, 210, 26);
    downmixToggle->setBounds (350, 382, 210, 26);
    tpToggle->setBounds      (350, 412, 210, 26);

    // room + meter on the right
    meterPanel->setBounds (640, 140, w - 660, 330);

    resetButton->setBounds (640, h - 70, 170, 26);

    const int summaryY = h - 36;
    summaryLabel.setBounds (20, summaryY, w - 40, 22);
}

} // namespace cinelab