# PeDROLBY Surround en REAPER — guía de uso

## Instalación

1. Compila (docs/BUILD.md) o copia el artefacto:
   - Linux: `~/.vst3/PeDROLBY Surround.vst3`
   - macOS: `~/Library/Audio/Plug-Ins/VST3` (y `Components` para AU)
   - Windows: `C:\Program Files\Common Files\VST3`
2. Abre REAPER → *Options → Preferences → Plug-ins → VST* y añade la carpeta
   correspondiente si no está.
3. `*Options → Preferences → Plug-ins → Re-scan*` (o reinicia REAPER).

## Flujo "Director" (sin experiencia técnica)

1. Inserta `PeDROLBY Surround` en el track (busca "PeDROLBY" en el menú FX).
2. **Escena** — qué sonido estás tratando:
   - *Diálogo*: voz clara al frente, presencia y de-esser.
   - *SFX / Foley / Música / Paisaje / Boom-LFE / Mix final*: cada uno
     configura toda la cadena por vos.
3. **Destino** — dónde se va a escuchar:
   - *Cine* (−24 LUFS, DCP) · *TV* (−23) · *Netflix* (−27) · *YouTube/Web*
     (−14) · *Podcast* (−16) · *Manual*.
4. **Intensidad** — cuánto efecto artístico (sutil → mucho cine).
5. **"Escuchar en la sala"** — pre-escucha cómo sonará la mezcla dentro de la
   sala destino (absorción de agudos real de una sala, escalada por el tamaño
   de sala elegido). Es una ayuda de monitoreo: al apagarlo, la salida vuelve
   al procesado normal.
6. **"A/B — original"** — compara al instante la mezcla procesada con la
   original (sin tocar nada).
7. El medidor te dice en palabras: `SUENA BIEN`, `UN POCO FUERTE`… El plugin
   iguala la sonoridad solo.
8. **(Reiniciar medición)** reinicia la medición integrada cuando quieras
   medir desde cero.

Consejos de director/a:
- "No se entiende el diálogo en la sala grande" → escena *Diálogo* + sala
  *grande* + intensidad alta.
- "Se oye piano en el celular pero no en el cine" → destino *Cine* para que
  el plugin compense los agudos de la sala.
- "El video de YouTube explota vs. otros" → destino *YouTube/Web*: normaliza
  a −14 LUFS y limita a −1 dB.

## Flujo "Pro"

En la pestaña **Pro** tenés el control completo:

- **EQ Cinema (curva X)**: *Sala* (escala total de la curva),
  *Graves* (shelf, −4 dB típico), *2k/4k/8k* (la subida de +3 dB/oct), *Aire*
  (shelf 10 kHz), y botones de **preset de sala** (grande → pequeña/plano).
- **Escena**: high-pass, cuerpo (frec/gain), presencia (frec/gain), aire,
  de-esser, compresión y anchura (mid/side; solo en estéreo). Botones de
  escena = punto de partida.
- **Entrega · Monitoreo · Surround**:
  - *Normalizar* (on/off), *Target LUFS* (manual), *Gan. manual*, lectura de
    ganancia automática.
  - *Limitador* + *Techo dBFS* + reducción actual.
  - *Escuchar en la sala* y *A/B*.
  - *Traseros dB* y *LFE dB*: ganancia de mastering por canal en surround.

## Surround (5.1 / 7.1)

- La pista en REAPER puede ser **5.1 o 7.1**; el plugin negocia el layout
  solo (estéreo por defecto; acepta hasta 8 canales).
- La medición LUFS usa los **pesos ITU BS.1770-4** por canal: surrounds 1.41,
  LFE excluido.
- La anchura mid/side se desactiva automáticamente en surround para no tocar
  la imagen; los controles de escena, normalización y limitador trabajan por
  canal de forma seguro.

## Automatización y presets

Todos los controles son parámetros estándar del DAW: podés automatizar
cualquier knob y guardar presets de REAPER (`Ctrl+S` en la ventana FX) que
guardan la configuración completa, incluida la elección de escena y destino.

## Notas técnicas

- **Latencia**: 0 muestras. El limitador protege el pico de muestra; el
  *true peak* (oversampling) llega en v0.2.
- La medición LUFS se hace siempre **sobre la entrada** (pre-procesamiento)
  para que la normalización apunte al material real.
- Ventanas de medición: momentáneo 400 ms, corto plazo ~3 s, integrado con
  gating (absoluto −70 / relativo −10 LUFS) desde el último ⟳.