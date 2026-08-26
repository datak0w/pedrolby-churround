// ============================================================================
// CineLab DSP — test de consola (sin DAW).
// Compila como CineLabDSPTest (cmake -DCINELAB_BUILD_TESTS=ON).
// ============================================================================

#include <juce_audio_basics/juce_audio_basics.h>

#include "dsp/Biquad.h"
#include "dsp/BassManager.h"
#include "dsp/CinemaEQ.h"
#include "dsp/Downmixer.h"
#include "dsp/LoudnessMeter.h"
#include "dsp/LoudnessNormalizer.h"
#include "dsp/RoomSimulator.h"
#include "dsp/SceneModule.h"
#include "dsp/SimpleLimiter.h"

#include <juce_dsp/juce_dsp.h>

#include <cmath>
#include <cstdio>

using namespace cinelab;

static int failures = 0;

#define CHECK(cond, msg)                                                        \
    do {                                                                        \
        if (! (cond)) {                                                         \
            std::printf ("  [FAIL] %s\n", msg);                                 \
            ++failures;                                                         \
        } else {                                                                \
            std::printf ("  [ok]   %s\n", msg);                                 \
        }                                                                       \
    } while (0)

static double rms (const float* d, int n)
{
    double s = 0.0;
    for (int i = 0; i < n; ++i)
        s += (double) d[i] * (double) d[i];
    return 10.0 * std::log10 (s / (double) n + 1.0e-20);
}

static float maxAbs (const float* d, int n)
{
    float m = 0.0f;
    for (int i = 0; i < n; ++i)
        m = juce::jmax (m, std::abs (d[i]));
    return m;
}

int main()
{
    std::printf ("CineLab DSP tests\n-----------------\n");

    const double sr = 48000.0;

    // ---------------------------------------------------------------- EQ cine
    {
        std::printf ("[1] EQ Cinema (curva X ISO 2969) — respuestas a 1kHz vs 8kHz\n");
        const int n = 48000;
        juce::AudioBuffer<float> buf (1, n);
        auto fill = [&] (double freq, float amp)
        {
            for (int i = 0; i < n; ++i)
                buf.getWritePointer (0)[i] = amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
        };

        // Medir un canal de test sin EQ (referencia de amplitud)
        float* data[1] = { buf.getWritePointer (0) };

        fill (1000.0, 0.5f);
        double ref1k = rms (data[0], n);
        fill (8000.0, 0.5f);
        double ref8k = rms (data[0], n);

        CinemaEQ eq;
        eq.prepare (sr, 1);
        CinemaEQ::Params p; // defaults: curva X a tope
        eq.setParams (p);

        fill (1000.0, 0.5f);
        eq.process (data, 1, n);
        double p1k = rms (data[0], n);

        fill (8000.0, 0.5f);
        eq.process (data, 1, n);
        double p8k = rms (data[0], n);

        const double g1k = p1k - ref1k;
        const double g8k = p8k - ref8k;
        std::printf ("    ganancia 1 kHz: %+.2f dB · 8 kHz: %+.2f dB\n", g1k, g8k);

        CHECK (g8k > g1k + 3.0,  "8 kHz se sube sensiblemente más que 1 kHz (+3 dB/oct)");
        CHECK (g1k > -2.0 && g1k < 2.0, "1 kHz queda cerca de la referencia (zona plana)");

        // roomSize = 0 → EQ inactivo
        eq.setParams (CinemaEQ::Params());
        CinemaEQ::Params flat;
        flat.roomSize = 0.0;
        eq.setParams (flat);
        fill (8000.0, 0.5f);
        eq.process (data, 1, n);
        CHECK (std::abs (rms (data[0], n) - ref8k) < 0.5, "roomSize=0 → pasa igual (curva apagada)");
    }

    // ------------------------------------------------------------- limitador
    {
        std::printf ("[2] Limitador de picos — techo a −1 dBFS\n");
        const int n = 48000;
        juce::AudioBuffer<float> buf (2, n);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                buf.getWritePointer (c)[i] = 1.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 220.0 * (double) i / sr);

        SimpleLimiter lim;
        lim.prepare (sr, 512);
        lim.setCeiling (-1.0);   // 10^(-1/20) = 0.891
        lim.setReleaseMs (180.0);

        float* d[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };
        lim.processBlock (d, 2, n);

        const float ceilLin = std::pow (10.0, -1.0 / 20.0);
        CHECK (maxAbs (d[0], n) <= ceilLin * 1.001 + 1.0e-6f, "pico del canal IZQ ≤ techo");
        CHECK (maxAbs (d[1], n) <= ceilLin * 1.001 + 1.0e-6f, "pico del canal DER ≤ techo");
        CHECK (lim.getGainReductionDb() <= 0.0f,              "GR ≤ 0 dB");
    }

    // -------------------------------------------------------------- medidor
    {
        std::printf ("[3] Medidor LUFS — tono de referencia y silencio\n");
        const int n = sr; // 1 segundo
        juce::AudioBuffer<float> buf (1, n);
        for (int i = 0; i < n; ++i)
            buf.getWritePointer (0)[i] = (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) i / sr);

        LoudnessMeter meter;
        meter.prepare (sr, 1);
        meter.processBlock (buf);
        meter.processBlock (buf); // 2 s → varias ventanas

        const float m = meter.readouts.momentary.load();
        const float st = meter.readouts.shortTerm.load();
        const float pk = meter.readouts.peakDb.load();

        std::printf ("    momentáneo: %.2f LUFS · corto: %.2f LUFS · pico: %.2f dBFS\n", m, st, pk);

        // Un seno full-scale de 1 kHz ≈ −3.7 LUFS (ponderación K ≈ 0 dB @1kHz)
        CHECK (m > -6.0f && m < -1.0f,        "momentáneo en rango esperado (−6..−1 LUFS)");
        CHECK (st > -6.0f && st < -1.0f,      "short-term en rango esperado");
        CHECK (pk > -1.5f,                    "pico ≈ 0 dBFS");

        LoudnessMeter sil;
        sil.prepare (sr, 1);
        juce::AudioBuffer<float> zero (1, n);
        zero.clear();
        sil.processBlock (zero);
        sil.processBlock (zero);
        CHECK (sil.readouts.momentary.load() < -900.0f, "silencio → sin medición (−inf)");
    }

    // ---------------------------------------------------------- normalizador
    {
        std::printf ("[4] Normalizador — persigue el target\n");
        LoudnessNormalizer norm;
        norm.reset (sr);
        norm.setEnabled (true);
        norm.setTarget (-16.0);
        norm.setManualGain (0.0);

        // material que mide −22 LUFS → el auto-gain debe subir ~+6 dB
        for (int i = 0; i < 60; ++i)
            norm.update (-22.0f);
        const double g = norm.getAutoGainDb();
        std::printf ("    auto gain tras 60 ventanas: %+.2f dB (esperado ≈ +6)\n", g);
        CHECK (g > 4.5 && g < 7.5, "auto gain ≈ target − nivel (−22 → −16)");

        // con el target ya alcanzado, el gain se estabiliza
        norm.update (-16.0f);
        norm.update (-16.0f);
        norm.update (-16.0f);
        const double g2 = norm.getAutoGainDb();
        CHECK (g2 > -0.8 && g2 < 0.8, "nivel = target → gain ≈ 0");
    }

    // ------------------------------------------------------ módulo de escena
    {
        std::printf ("[5] Módulo de escena — passthrough y presencia de voz\n");
        const int n = 48000;
        juce::AudioBuffer<float> buf (1, n);
        for (int i = 0; i < n; ++i)
            buf.getWritePointer (0)[i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 1000.0 * (double) i / sr);

        SceneModule mod;
        mod.prepare (sr, 1);
        mod.setParams (SceneModule::Params()); // todo plano
        float* d[1] = { buf.getWritePointer (0) };

        juce::AudioBuffer<float> copy (buf);
        mod.processBlock (buf);
        const double diff = std::abs (rms (d[0], n) - rms (copy.getReadPointer (0), n));
        CHECK (diff < 0.01, "sin procesar → passthrough");

        // escena diálogo: la presencia sube las frecuencias de la voz
        SceneModule::Params dl;
        dl.presenceFreq = 3200.0;  dl.presenceGainDb = 4.0;
        dl.deEss = 0.3;            dl.compAmount = 0.0; // compresión aislada aparte
        mod.setParams (dl);

        auto measureAt = [&] (double freq)
        {
            for (int i = 0; i < n; ++i)
                buf.getWritePointer (0)[i] = 0.2f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
            juce::AudioBuffer<float> before (buf);
            mod.processBlock (buf);
            return rms (d[0], n) - rms (before.getReadPointer (0), n);
        };

        const double g3k = measureAt (3200.0);
        const double g200 = measureAt (200.0);
        std::printf ("    ganancia 3.2 kHz: %+.2f dB · 200 Hz: %+.2f dB\n", g3k, g200);
        CHECK (g3k > 1.0,  "presencia (3.2 kHz) sube con la escena de diálogo");
        CHECK (g3k > g200, "presencia sube más que los graves");

        // con compresión glue activa, el nivel medio baja (pegamento)
        dl.compAmount = 0.5;
        mod.setParams (dl);
        const double g3kC = measureAt (3200.0);
        CHECK (g3kC < g3k, "la compresión glue reduce el nivel medio (pegamento)");
    }

    // -------------------------------------------------------------- downmix
    {
        std::printf ("[7] Downmix 5.1→2.0 — ganancias ITU-R BS.775\n");
        const int n = 4800;
        Downmixer dm;

        // centro solo en el canal C (índice 2)
        juce::AudioBuffer<float> buf (6, n);
        buf.clear();
        for (int i = 0; i < n; ++i) buf.setSample (2, i, 1.0f);

        float* d[6];
        for (int c = 0; c < 6; ++c) d[c] = buf.getWritePointer (c);
        dm.process (d, 6, n);

        CHECK (std::abs (buf.getSample (0, 100) - 0.707f) < 0.01f, "C se pliega a L/R con 0.707");
        CHECK (std::abs (buf.getSample (1, 100) - 0.707f) < 0.01f, "C se pliega a L/R con 0.707");
        CHECK (std::abs (buf.getSample (2, 100)) < 1.0e-6f,        "canales envolventes silenciados");

        // LFE (índice 3) al 0.5 en ambos
        juce::AudioBuffer<float> buf2 (6, n);
        buf2.clear();
        for (int i = 0; i < n; ++i) buf2.setSample (3, i, 1.0f);
        for (int c = 0; c < 6; ++c) d[c] = buf2.getWritePointer (c);
        dm.process (d, 6, n);
        CHECK (std::abs (buf2.getSample (0, 100) - 0.5f) < 0.01f, "LFE a −6 dB en L");
        CHECK (std::abs (buf2.getSample (1, 100) - 0.5f) < 0.01f, "LFE a −6 dB en R");
    }

    // ---------------------------------------------------------- true peak
    {
        std::printf ("[8] Limitador TRUE PEAK — techo a −1 dBTP\n");
        const int n = 48000;
        const double f = 19000.0; // cerca de Nyquist → las curvas de reconstrucción suben
        juce::AudioBuffer<float> buf (2, n);
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < n; ++i)
                buf.setSample (c, i, 1.4f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * f * (double) i / sr));

        SimpleLimiter lim;
        lim.prepare (sr, 512);
        lim.setCeiling (-1.0);
        lim.setTruePeakEnabled (true);
        lim.setReleaseMs (180.0);

        float* d[2] = { buf.getWritePointer (0), buf.getWritePointer (1) };
        lim.processBlock (d, 2, n);

        // medimos el TRUE PEAK de la salida (4× oversampling)
        juce::dsp::Oversampling<float> os (1, 2, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true);
        os.initProcessing ((size_t) n);
        float tp = 0.0f;
        for (int c = 0; c < 2; ++c)
        {
            const float* chans[1] = { buf.getReadPointer (c) };
            auto up = os.processSamplesUp (juce::dsp::AudioBlock<const float> (chans, 1, (size_t) n));
            const float* ud = up.getChannelPointer (0);
            for (size_t i = 0; i < up.getNumSamples(); ++i)
                tp = juce::jmax (tp, std::abs (ud[i]));
            os.reset();
        }

        const float tpDb = 20.0f * std::log10 (tp);
        std::printf ("    true peak de la salida: %.3f dBTP (techo −1.0)\n", tpDb);
        CHECK (tpDb <= -0.98f, "true peak tras el limitador ≤ techo (−1 dBTP)");
    }

    // ---------------------------------------------------- simulador de sala
    {
        std::printf ("[6] Simulador de sala — absorción de agudos vs graves\n");
        const int n = 48000;
        juce::AudioBuffer<float> buf (1, n);

        auto measure = [&] (double freq, double room)
        {
            for (int i = 0; i < n; ++i)
                buf.getWritePointer (0)[i] = 0.3f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr);
            juce::AudioBuffer<float> before (buf);

            RoomSimulator sim;
            sim.prepare (sr, 1);
            RoomSimulator::Params p;
            p.roomAmount = room;
            sim.setParams (p);

            float* d[1] = { buf.getWritePointer (0) };
            sim.process (d, 1, n);
            return rms (d[0], n) - rms (before.getReadPointer (0), n);
        };

        const double g8k  = measure (8000.0, 1.0);
        const double g200 = measure (200.0, 1.0);
        const double g8kSmall = measure (8000.0, 0.45);

        std::printf ("    sala grande: 8 kHz %+.2f dB · 200 Hz %+.2f dB · sala pequeña 8 kHz %+.2f dB\n",
                     g8k, g200, g8kSmall);

        CHECK (g8k < -1.0,            "sala grande absorbe agudos (8 kHz)");
        CHECK (g200 > g8k,            "los graves se conservan/acentúan frente a los agudos");
        CHECK (g8kSmall > g8k,        "sala pequeña absorbe menos que sala grande");
        CHECK (measure (8000.0, 0.0) == 0.0, "room=0 → sin efecto (passthrough)");
    }

    // ----------------------------------------------------- bass management
    {
        std::printf ("[9] Bass management — crossover hacia LFE\n");
        const int n = 48000;
        juce::AudioBuffer<float> buf (6, n); // L,R,C,LFE,Ls,Rs

        auto fillAll = [&] (double freq, float amp)
        {
            for (int c = 0; c < 6; ++c)
                for (int i = 0; i < n; ++i)
                    buf.setSample (c, i, amp * (float) std::sin (2.0 * juce::MathConstants<double>::pi * freq * (double) i / sr));
        };

        auto rmsCh = [&] (int c) { return rms (buf.getReadPointer (c), n); };

        BassManager bm;
        bm.prepare (sr, 6, 3); // LFE en índice 3

        BassManager::Params p;
        p.crossoverHz = 80.0;
        p.lfeGainDb   = 0.0;
        p.sendToLfe   = true;
        p.hpMain      = false;
        bm.setParams (p);

        float* d[6];
        for (int c = 0; c < 6; ++c) d[c] = buf.getWritePointer (c);

        // 40 Hz con hpMain=false: los canales principales quedan intactos
        fillAll (40.0, 0.5f);
        const double before0 = rmsCh (0);
        bm.process (d, n);
        CHECK (std::abs (rmsCh (0) - before0) < 0.02, "sin hpMain el canal principal se conserva");

        // el LFE ha recibido parte de los graves de los canales principales
        const double lfeAmp = rmsCh (3);
        const double mainAmp = rmsCh (0);
        std::printf ("    tras crossover → LFE: LFE rms %.3f · main rms %.3f\n", lfeAmp, mainAmp);
        CHECK (lfeAmp > mainAmp * 0.5, "el LFE recibe graves de los canales principales");

        // hpMain=true: los principales pierden graves por debajo del crossover
        p.hpMain = true;
        bm.setParams (p);
        fillAll (40.0, 0.5f);
        const double beforeHp = rmsCh (0);
        bm.process (d, n);
        const double mainHp = rmsCh (0);
        std::printf ("    hpMain: before %.3f dB → after %.3f dB\n", beforeHp, mainHp);
        std::printf ("    muestras ch0 tras hpMain: %+.3f %+.3f %+.3f %+.3f %+.3f (esperadas ~0)\n",
                     (double) buf.getSample (0, 100), (double) buf.getSample (0, 1000),
                     (double) buf.getSample (0, 5000), (double) buf.getSample (0, 20000),
                     (double) buf.getSample (0, 40000));
        CHECK (mainHp < beforeHp * 0.9, "con hpMain el canal principal pierde el bajo");

        // sendToLfe=false → sin cambios
        p.hpMain = false; p.sendToLfe = false;
        bm.setParams (p);
        fillAll (40.0, 0.5f);
        const double beforeOff = rmsCh (0);
        bm.process (d, n);
        CHECK (std::abs (rmsCh (0) - beforeOff) < 0.02, "sendToLfe=false → sin cambios apreciables");

        // estéreo (sin LFE) → inactivo automático
        BassManager bm2;
        bm2.prepare (sr, 2, -1);
        juce::AudioBuffer<float> buf2 (2, n);
        for (int i = 0; i < n; ++i)
            buf2.setSample (0, i, 0.5f * (float) std::sin (2.0 * juce::MathConstants<double>::pi * 40.0 * (double) i / sr));
        float* d2[2] = { buf2.getWritePointer (0), buf2.getWritePointer (1) };
        const double beforeStereo = rms (buf2.getReadPointer (0), n);
        bm2.process (d2, n);
        CHECK (std::abs (rms (d2[0], n) - beforeStereo) < 0.001, "estéreo/mono → bass management inactivo");
    }

    // ------------------------------------------------------------------ fin
    std::printf ("\n%s: %d fallo(s)\n", failures == 0 ? "ALL TESTS PASSED" : "TESTS FAILED", failures);
    return failures == 0 ? 0 : 1;
}