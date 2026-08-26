#pragma once

#include "CineKnob.h"
#include "MeterComponents.h"

class CineLabAudioProcessor; // clase del procesador, en el namespace global

namespace cinelab
{

// ============================================================================
// Vista PRO — todo el control, en secciones: EQ Cinema / Escena / Entrega.
// ============================================================================
class ProView : public juce::Component
{
public:
    ProView (CineLabAudioProcessor& processor, Parameters& params);
    ~ProView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    void refresh();

private:
    CineLabAudioProcessor& processor;
    Parameters& parameters;

    std::unique_ptr<MeterPanel> meterPanel;

    // EQ Cinema
    std::vector<std::unique_ptr<CineKnob>> eqKnobs;
    std::vector<std::unique_ptr<juce::TextButton>> roomButtons;

    // Escena
    std::vector<std::unique_ptr<CineKnob>> sceneKnobs;
    std::vector<std::unique_ptr<juce::TextButton>> scenePresetButtons;

    // Entrega
    std::unique_ptr<juce::ToggleButton> normToggle;
    std::unique_ptr<CineKnob> targetKnob;
    std::unique_ptr<CineKnob> manualGainKnob;
    std::unique_ptr<CineKnob> ceilingKnob;
    std::unique_ptr<juce::ToggleButton> limToggle;
    std::unique_ptr<juce::TextButton> resetButton;

    // Monitoring (preview) + Surround + Delivery toggles
    std::unique_ptr<juce::ToggleButton> simToggle;
    std::unique_ptr<juce::ToggleButton> abToggle;
    std::unique_ptr<juce::ToggleButton> downmixToggle;
    std::unique_ptr<juce::ToggleButton> tpToggle;
    std::unique_ptr<CineKnob> rearKnob;
    std::unique_ptr<CineKnob> lfeKnob;

    // Bass management
    std::unique_ptr<juce::ToggleButton> bmToggle;
    std::unique_ptr<CineKnob> bmXoverKnob;
    std::unique_ptr<CineKnob> bmLfeKnob;
    std::unique_ptr<juce::ToggleButton> bmSendToggle;
    std::unique_ptr<juce::ToggleButton> bmHpToggle;

    juce::Label autoGainLabel;
    juce::Label grLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> normAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> limAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> simAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> downmixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tpAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bmAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bmSendAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bmHpAttach;
};

} // namespace cinelab