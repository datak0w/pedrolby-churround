#pragma once

#include "CineKnob.h"
#include "MeterComponents.h"

class CineLabAudioProcessor; // clase del procesador, en el namespace global

namespace cinelab
{

// ============================================================================
// Vista DIRECTOR — la cara sin jerga.
// Escena → Destino → Intensidad. Los números salen solos.
// ============================================================================
class DirectorView : public juce::Component
{
public:
    DirectorView (CineLabAudioProcessor& processor, Parameters& params);
    ~DirectorView() override = default;

    void paint (juce::Graphics&) override;
    void resized() override;

    // llamado por el temporizador del editor
    void refresh();

private:
    void onSceneClicked (int index);
    void onDestinationClicked (int index);

    CineLabAudioProcessor& processor;
    Parameters& parameters;

    std::vector<std::unique_ptr<ChoiceButton>> sceneButtons;
    std::vector<std::unique_ptr<ChoiceButton>> destButtons;
    std::vector<std::unique_ptr<ChoiceButton>> roomButtons;

    std::unique_ptr<ChoiceToButtons> sceneChooser;
    std::unique_ptr<ChoiceToButtons> destChooser;

    std::unique_ptr<CineKnob> intensityKnob;
    std::unique_ptr<MeterPanel> meterPanel;

    std::unique_ptr<juce::ToggleButton> simToggle;   // listen in the room
    std::unique_ptr<juce::ToggleButton> abToggle;    // A/B dry
    std::unique_ptr<juce::ToggleButton> downmixToggle; // 5.1/7.1 → stereo
    std::unique_ptr<juce::ToggleButton> tpToggle;     // true peak (oversampled)
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> simAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> abAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> downmixAttach;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> tpAttach;

    juce::Label detailLabel;      // blurb de la escena/destino seleccionados
    juce::Label summaryLabel;     // línea resumen inferior
    std::unique_ptr<juce::TextButton> resetButton;

    int lastDestIndex = -1;
    int lastRoomIndex = 0;
};

} // namespace cinelab