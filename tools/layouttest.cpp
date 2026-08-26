// ============================================================================
// CineLab — test de layout del editor (sin DAW).
//
// Construye el editor real (Processor + Editor + vistas), comprueba que
// NINGÚN par de controles hermanos se solape, y renderiza capturas PNG de
// las vistas Director y Pro.
//
//   cmake -DCINELAB_BUILD_TESTS=ON && cmake --build build --target CineLabLayoutTest -j
//   ./build/tools/CineLabLayoutTest
// ============================================================================

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

static int g_overlaps = 0;

static void checkSiblingOverlaps (const Component& parent, const String& context)
{
    const auto& children = parent.getChildren();
    for (int i = 0; i < (int) children.size(); ++i)
    {
        for (int j = i + 1; j < (int) children.size(); ++j)
        {
            // solo importan los solapamientos de controles VISIBLES
            if (! children[(size_t) i]->isVisible() || ! children[(size_t) j]->isVisible())
                continue;

            const auto a = children[(size_t) i]->getBounds();
            const auto b = children[(size_t) j]->getBounds();
            if (a.intersects (b))
            {
                ++g_overlaps;
                std::printf ("  [OVERLAP] %s: <%s> %s  ∩  <%s> %s\n",
                             context.toRawUTF8(),
                             children[(size_t) i]->getName().isNotEmpty() ? children[(size_t) i]->getName().toRawUTF8() : "sin-nombre",
                             a.toString().toRawUTF8(),
                             children[(size_t) j]->getName().isNotEmpty() ? children[(size_t) j]->getName().toRawUTF8() : "sin-nombre",
                             b.toString().toRawUTF8());
            }
        }

        checkSiblingOverlaps (*children[(size_t) i], context);
    }
}

static void renderSnapshot (Component& c, const String& outPath)
{
    const Image img = c.createComponentSnapshot (c.getLocalBounds());
    if (img.isNull())
    {
        std::printf ("  [PNG] FALLÓ render de %s\n", outPath.toRawUTF8());
        return;
    }

    File f (outPath);
    if (auto fos = f.createOutputStream())
    {
        PNGImageFormat png;
        png.writeImageToStream (img, *fos);
        std::printf ("  [PNG] %s (%dx%d)\n", outPath.toRawUTF8(), img.getWidth(), img.getHeight());
    }
}

int main()
{
    ScopedJuceInitialiser_GUI init;

    std::printf ("CineLab layout test\n-------------------\n");

    CineLabAudioProcessor processor;
    auto& params = processor.getParameters();

    CineLabAudioProcessorEditor editor (processor, params);
    editor.setSize (980, 600);
    processor.prepareToPlay (48000.0, 512);

    // Las dos vistas grandes (DirectorView y ProView) entre los hijos del editor
    std::vector<Component*> views;
    for (auto* child : editor.getChildren())
        if (child->getWidth() > 300 && child->getHeight() > 200)
            views.push_back (child);

    std::printf ("Vistas encontradas: %d\n", (int) views.size());

    std::printf ("\n[Recorrido completo de solapamientos (todos los niveles)]\n");
    checkSiblingOverlaps (editor, "Editor");

    std::printf ("\n[Render de vistas]\n");
    if (views.size() >= 2)
    {
        // Director (visible por defecto)
        renderSnapshot (*views[0], "/home/kali/code/cinemaVST/tools/layout_director.png");
        // Pro
        views[0]->setVisible (false);
        views[1]->setVisible (true);
        renderSnapshot (*views[1], "/home/kali/code/cinemaVST/tools/layout_pro.png");
    }

    // --- surround 5.1: negociación de buses + processBlock multicanal --------
    std::printf ("\n[Surround 5.1 — smoke test]\n");
    {
        std::printf ("  buses: in=%d out=%d canales actuales=%d\n",
                     processor.getBusCount (true), processor.getBusCount (false),
                     processor.getTotalNumInputChannels());

        AudioProcessor::BusesLayout fiveOne;
        fiveOne.inputBuses.add (AudioChannelSet::create5point1());
        fiveOne.outputBuses.add (AudioChannelSet::create5point1());

        const bool layoutOk = processor.setBusesLayout (fiveOne);
        const bool layoutOk2 = ! layoutOk ? processor.setBusesLayoutWithoutEnabling (fiveOne) : true;
        std::printf ("  layout 5.1: setBusesLayout=%s · setWithoutEnabling=%s · canales=%d\n",
                     layoutOk ? "sí" : "no",
                     layoutOk2 ? "sí" : "no",
                     processor.getTotalNumInputChannels());

        if (layoutOk || layoutOk2)
        {
            processor.prepareToPlay (48000.0, 512);

            AudioBuffer<float> buf (6, 512);
            Random rng (1234);
            for (int c = 0; c < 6; ++c)
                for (int i = 0; i < 512; ++i)
                    buf.setSample (c, i, rng.nextFloat() * 0.2f - 0.1f);

            MidiBuffer midi;
            processor.processBlock (buf, midi); // 2 bloques → frames de medición
            processor.processBlock (buf, midi);

            bool finite = true;
            for (int c = 0; c < 6 && finite; ++c)
                for (int i = 0; i < 512 && finite; ++i)
                    finite = std::isfinite (buf.getSample (c, i));

            std::printf ("  salida 6 canales finita: %s\n", finite ? "sí" : "NO");
            if (! finite) ++g_overlaps;
        }
        else
        {
            ++g_overlaps;
        }

        // volver a estéreo para no dejar el estado de prueba
        AudioProcessor::BusesLayout stereo;
        stereo.inputBuses.add (AudioChannelSet::stereo());
        stereo.outputBuses.add (AudioChannelSet::stereo());
        processor.setBusesLayout (stereo);
        processor.prepareToPlay (48000.0, 512);
    }

    // --- presets de usuario --------------------------------------------------
    std::printf ("\n[User presets]\n");
    {
        auto& up = processor.getUserPresets();
        auto& apvts = params.apvts;

        up.savePreset ("TestA");
        up.savePreset ("TestB");
        const auto names = up.getPresetNames();
        std::printf ("  guardados: %s\n", names.joinIntoString (", ").toRawUTF8());
        if (names.size() < 2 || names[0] != "TestA" || names[1] != "TestB") ++g_overlaps;

        // cambiar un parámetro y cargar el preset TestA → se restaura
        auto* intensity = apvts.getParameter (cinelab::IDs::intensity);
        const auto range = apvts.getParameterRange (cinelab::IDs::intensity);
        intensity->beginChangeGesture();
        intensity->setValueNotifyingHost (range.convertTo0to1 (0.25f));
        intensity->endChangeGesture();

        up.loadPreset ("TestA");
        const float restored = apvts.getRawParameterValue (cinelab::IDs::intensity)->load();
        std::printf ("  intensidad tras load: %.2f (original guardado):\n", restored);
        if (std::abs (restored - 1.0f) > 0.01f) ++g_overlaps; // default intensity = 1.0

        // roundtrip estado (guardar/recargar) conserva presets
        MemoryBlock block;
        processor.getStateInformation (block);
        if (block.getSize() > 0)
        {
            processor.setStateInformation (block.getData(), (int) block.getSize());
            const auto restoredNames = processor.getUserPresets().getPresetNames();
            std::printf ("  presets tras roundtrip: %d (esperado ≥ 2)\n", restoredNames.size());
            if (restoredNames.size() < 2) ++g_overlaps;
        }

        up.deletePreset ("TestA");
        up.deletePreset ("TestB");
        std::printf ("  tras borrar: %d presets\n", up.getPresetNames().size());
        if (up.getPresetNames().size() != 0) ++g_overlaps;
    }

    std::printf ("\n%s: %d solapamientos\n", g_overlaps == 0 ? "LAYOUT OK" : "LAYOUT CON PROBLEMAS", g_overlaps);
    return g_overlaps == 0 ? 0 : 1;
}