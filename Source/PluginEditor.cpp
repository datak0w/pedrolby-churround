#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "ui/DirectorView.h"
#include "ui/ProView.h"

using namespace cinelab;

static constexpr int kHeaderHeight = 52;

CineLabAudioProcessorEditor::CineLabAudioProcessorEditor (CineLabAudioProcessor& proc, Parameters& p)
    : AudioProcessorEditor (&proc), processor (proc), parameters (p)
{
    lookAndFeel = std::make_unique<CineLookAndFeel>();
    setLookAndFeel (lookAndFeel.get());

    // ---- cabecera ----
    logoLabel.setText ("PeDROLBY Churround", juce::dontSendNotification);
    logoLabel.setColour (juce::Label::textColourId, theme::accent);
    logoLabel.setFont (theme::uiFont (20.0f, juce::Font::bold));
    addAndMakeVisible (logoLabel);

    // ---- presets de usuario (cabecera) ----
    presetBar = std::make_unique<cinelab::PresetBar> (processor, parameters);
    addAndMakeVisible (*presetBar);

    // ---- conmutador de modo ----
    directorButton = std::make_unique<ChoiceButton> ("Director", "fácil", true);
    proButton      = std::make_unique<ChoiceButton> ("Pro", "todo el control");
    directorButton->setRadioGroupId (2001);
    proButton->setRadioGroupId (2001);
    addAndMakeVisible (*directorButton);
    addAndMakeVisible (*proButton);

    std::vector<ChoiceButton*> modes = { directorButton.get(), proButton.get() };
    modeChooser = std::make_unique<ChoiceToButtons> (parameters.apvts, IDs::mode, modes);

    directorButton->onClick = [this] { if (directorButton->getToggleState()) setMode (0); };
    proButton->onClick      = [this] { if (proButton->getToggleState())      setMode (1); };

    // ---- vistas ----
    directorView = std::make_unique<DirectorView> (processor, parameters);
    proView      = std::make_unique<ProView> (processor, parameters);
    addAndMakeVisible (*directorView);
    addAndMakeVisible (*proView);

    setMode (modeChooser->getSelectedIndex() < 0 ? 0 : modeChooser->getSelectedIndex());

    // Importante: setSize al FINAL, cuando todos los hijos ya existen.
    // Si un host (REAPER VST3) crea el editor antes de que terminen los
    // children, un setSize temprano dispara resized() con miembros sin crear
    // (nullptr) → crash en Component::setBounds.
    setSize (980, 660);

    // --- tooltips de ayuda ("?") ------------------------------------------
    tooltipWindow = std::make_unique<juce::TooltipWindow> (this, 600);
    tooltipWindow->setColour (juce::TooltipWindow::backgroundColourId, theme::panelAlt);
    tooltipWindow->setColour (juce::TooltipWindow::textColourId, theme::text);
    tooltipWindow->setColour (juce::TooltipWindow::outlineColourId, theme::accent);
    tooltipWindow->setOpaque (false);

    startTimerHz (30);
}

CineLabAudioProcessorEditor::~CineLabAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

// ============================================================================

void CineLabAudioProcessorEditor::setMode (int index)
{
    if (index == currentMode)
        return;

    currentMode = index;
    const bool director = index == 0;
    directorView->setVisible (director);
    proView->setVisible (!director);
    modeChooser->setSelected (index);
    resized();
    repaint();
}

// ============================================================================

void CineLabAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (theme::bg);

    // línea de separación de la cabecera
    g.setColour (theme::border);
    g.fillRect (0, kHeaderHeight - 1, getWidth(), 1);
}

void CineLabAudioProcessorEditor::resized()
{
    // Guardia: si un host despacha resize antes de que el constructor haya
    // terminado de crear los hijos, no hacemos nada (evita nullptr).
    if (directorButton == nullptr || directorView == nullptr || proView == nullptr)
        return;

    const int w = getWidth();
    const int h = getHeight();

    logoLabel.setBounds (20, 12, 240, 28);

    // barra de presets de usuario (antes del conmutador de modo)
    presetBar->setBounds (270, 9, 460, 28);

    directorButton->setBounds (w - 240, 11, 108, 30);
    proButton->setBounds (w - 124, 11, 104, 30);

    auto content = juce::Rectangle<int> (0, kHeaderHeight, w, h - kHeaderHeight);
    directorView->setBounds (content);
    proView->setBounds (content);
}

// ============================================================================

void CineLabAudioProcessorEditor::timerCallback()
{
    presetBar->refresh();

    if (currentMode == 0)
    {
        directorView->refresh();
        directorView->repaint();
    }
    else
    {
        proView->refresh();
        proView->repaint();
    }
}