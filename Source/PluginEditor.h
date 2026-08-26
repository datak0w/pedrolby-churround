#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters.h"
#include "ui/CineTheme.h"
#include "ui/CineKnob.h"
#include "ui/DirectorView.h"
#include "ui/ProView.h"
#include "ui/PresetBar.h"

class CineLabAudioProcessor;

// ============================================================================
// Editor de CineLab: cabecera con logo + conmutador Director/Pro, y la vista
// activa. Un temporizador de 30 Hz alimenta los medidores.
// ============================================================================
class CineLabAudioProcessorEditor : public juce::AudioProcessorEditor,
                                    public juce::Timer
{
public:
    CineLabAudioProcessorEditor (CineLabAudioProcessor&, cinelab::Parameters&);
    ~CineLabAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    void setMode (int index);

    CineLabAudioProcessor& processor;
    cinelab::Parameters& parameters;

    std::unique_ptr<cinelab::CineLookAndFeel> lookAndFeel;

    std::unique_ptr<cinelab::ChoiceButton> directorButton;
    std::unique_ptr<cinelab::ChoiceButton> proButton;
    std::unique_ptr<cinelab::ChoiceToButtons> modeChooser;

    juce::Label logoLabel;

    std::unique_ptr<cinelab::PresetBar> presetBar;

    std::unique_ptr<juce::TooltipWindow> tooltipWindow;

    std::unique_ptr<cinelab::DirectorView> directorView;
    std::unique_ptr<cinelab::ProView> proView;

    int currentMode = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CineLabAudioProcessorEditor)
};