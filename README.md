# PeDROLBY Churround — audio de cine en REAPER (VST3 / AU)

[![CI](https://github.com/datak0w/pedrolby-churround/actions/workflows/build.yml/badge.svg)](https://github.com/datak0w/pedrolby-churround/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/datak0w/pedrolby-churround)](https://github.com/datak0w/pedrolby-churround/releases)
[![Licencia: AGPLv3](https://img.shields.io/badge/license-AGPLv3-blue)](LICENSE)

> **El plugin todo-en-uno que convierte el audio de tu película en "audio de
> cine" — con un botón.** Producción y postproducción de audio para video y
> películas, con **surround 5.1/7.1**. Funciona en REAPER y cualquier DAW con
> VST3/AU (y en Logic/GarageBand vía AU en macOS).

![Vista Director](docs/img/director.png)
*Vista Director — para directoras/es, editoras/es y dirección artística.*

![Vista Pro](docs/img/pro.png)
*Vista Pro — control total para quienes mezclan.*

---

## Descarga e instalación (usuarios)

### 📥 Descargar

Todas las versiones están en la página **Releases**:
👉 **https://github.com/datak0w/pedrolby-churround/releases**

| Archivo | Plataforma | Formato | Notas |
|---|---|---|---|
| `PeDROLBY-Churround-vX.Y.Z-Windows.zip` | Windows 10/11 (64-bit) | VST3 | copia la carpeta `.vst3` |
| `PeDROLBY-Churround-vX.Y.Z-macOS.zip` | macOS (Intel + Apple Silicon) | **VST3 + AU** | universal `arm64+x86_64` |
| `PeDROLBY-Churround-vX.Y.Z-Linux.zip` | Linux (x86_64) | VST3 | copia la carpeta `.vst3` |

> Los archivos se generan automáticamente en cada versión etiquetada (CI).
> Si buscas una build intermedia sin release, puedes descargarla desde
> **Actions → último run → Artifacts**.

### 🍎 macOS

1. Descarga `PeDROLBY-Churround-vX.Y.Z-macOS.zip` y descomprímelo.
2. Copia los bundles a las carpetas de plugins:
   ```bash
   cp -r "PeDROLBY Surround.vst3"      ~/Library/Audio/Plug-Ins/VST3/
   cp -r "PeDROLBY Surround.component" ~/Library/Audio/Plug-Ins/Components/
   ```
3. Los builds llevan **firma ad-hoc** (sin Developer ID); la primera vez
   macOS puede bloquearlos. Quita la cuarentena y abre cada bundle una vez:
   ```bash
   xattr -cr ~/Library/Audio/Plug-Ins/VST3/"PeDROLBY Surround.vst3" \
             ~/Library/Audio/Plug-Ins/Components/"PeDROLBY Surround.component"
   ```
   O haz clic derecho → *Abrir* en cada bundle y confirma.
4. **REAPER**: *Options → Preferences → Plug-ins → VST* → **Rescan** (y
   *CLEAR cache and rescan* si no aparece). Encontrarás **PeDROLBY** en el
   menú FX.
5. (Opcional) **Logic / GarageBand**: el AU aparece directamente en *Audio
   Units → PeDROLBY*.

### 🪟 Windows

1. Descarga el zip de Windows, descomprímelo.
2. Copia la carpeta `PeDROLBY Surround.vst3` a:
   ```
   C:\Program Files\Common Files\VST3\
   ```
3. En REAPER: *Options → Preferences → Plug-ins → VST* (añade esa ruta si no
   está) → **Rescan**. Busca **PeDROLBY** en el menú FX.
4. Si SmartScreen pregunta: *Más información → Ejecutar de todas formas*
   (firma ad-hoc).

### 🐧 Linux

1. Descarga el zip de Linux y descomprímelo.
2. Copia la carpeta a la ruta de plugins de usuario:
   ```bash
   mkdir -p ~/.vst3
   cp -r "PeDROLBY Surround.vst3" ~/.vst3/
   ```
3. En REAPER: *Options → Preferences → Plug-ins → VST* → **Rescan**.
   Busca **PeDROLBY** en el menú FX.
4. Requiere un escritorio con X11 y las librerías X/ALSA habituales
   (`libasound2 libx11-6 libxext6 …` — ya presentes en casi cualquier
   escritorio Linux).

### Requisitos

- **macOS** 10.15 o superior (Intel o Apple Silicon; binario universal).
- **Windows** 10/11, 64-bit.
- **Linux** x86_64 (glibc), escritorio con X11.
- **REAPER** 6.x/7.x (o cualquier DAW con VST3; el AU funciona en hosts AU).
- El plugin soporta pistas **mono, estéreo, 5.1 y 7.1**.

### Solución de problemas

| Problema | Solución |
|---|---|
| REAPER no ve el plugin | Comprueba la ruta (tabla de arriba) y haz *Clear cache and rescan*; reinicia REAPER |
| macOS: "no se puede verificar el desarrollador" | `xattr -cr` sobre los bundles, o clic derecho → Abrir |
| macOS: el AU no sale en Logic | Reinstala el `.component` y comprueba *Plug-In Manager* de Logic |
| Windows: SmartScreen | *Más información → Ejecutar de todas formas* |
| La pista no tiene sonido procesado | Revisa que el plugin esté *active* (verde) en la cadena FX |
| Presets de usuario "desaparecen" | Se guardan con el estado del proyecto; guarda el proyecto para conservarlos |

---

## Qué es

PeDROLBY Churround reúne en un solo plugin lo que normalmente exige una
cadena de varios: **EQ Cinema (curva X ISO 2969)** para salas grandes,
**normalización de sonoridad por estándar de entrega**, **módulos por
escena**, **simulador de sala (pre-escucha)**, **A/B**, **downmix 5.1→2.0**,
**true-peak**, **bass management (graves/LFE)** y **presets de usuario**, con
dos caras:

| | **Director** (fácil) | **Pro** (full) |
|---|---|---|
| Para quién | dirección, edición, dirección artística | mezcla y mastering |
| Qué ves | Escena → Destino → Intensidad, pre-escucha | Todos los controles + medidores |
| Jerga | Cero | dB, LUFS, threshold, techo, crossover… |

---

## Manual de uso

### 1. Primeros pasos (60 segundos)

1. Inserta el plugin en el track.
2. **Escena**: pulsa lo que estás tratando (Diálogo, SFX, Foley, Música,
   Ambiente, Boom/LFE, Mix final).
3. **Destino de entrega**: pulsa dónde se escuchará (Cine, TV, Netflix,
   YouTube/Web, Podcast o Manual).
4. **Intensidad**: cuánto efecto artístico (de sutil a mucho cine).
5. Listo: el plugin ecualiza, normaliza y limita según tu elección. El
   medidor te dice en palabras si el nivel está bien.

### 2. Vista Director (detalle)

- **SCENE** — presets creativos que configuran toda la cadena según el
  contenido (Diálogo, SFX, Foley, Música, Ambiente, Boom-LFE, Mix final).
- **DELIVERY TARGET** — el "número legal" de cada destino: *Cinema* −24 /
  *TV* −23 / *Netflix* −27 / *Web* −14 / *Podcast* −16 / *Manual*.
- **SCREENING ROOM** — tamaño de sala para la **curva X**: grande/IMAX →
  curva completa; mediana/pequeña → suavizada; *Flat* = monitoreo neutro.
- **INTENSITY** — escala todo el tratamiento creativo (la normalización y el
  limitador no se tocan: son la "regla", no arte).
- **Listen in the room** — pre-escucha cómo sonará la mezcla *dentro* de la
  sala destino (monitoreo puro).
- **A/B — dry** — compara procesado ↔ original al instante.
- **Downmix 5.1→2.0** / **True peak** — ver vista Pro.
- Medidor: momentáneo / corto / integrado LUFS, pico y **TRUE PEAK** (dBTP),
  con etiqueta humana (SOUNDS GOOD / TOO LOUD…). *Reset integrated
  measurement* reinicia la medida.

### 3. Vista Pro (detalle)

**Cinema EQ — X-curve (ISO 2969)**
- *Room*: escala completa de la curva (0 = plano, 1 = sala grande).
- *Lows / Lows Hz*: shelf de graves (típico −4 dB @ 100 Hz).
- *2k / 4k / 8k*: la subida de ~+3 dB/octava por encima de 2 kHz.
- *Air / Air Hz*: shelf de aire a ~10 kHz. Botones Large/Medium/Small/Flat.

**Scene**
- *HP Hz*, *Body F/G*, *Pres F/G*, *Air G*, *De-ess*, *Comp*, *Width*
  (anchura mid/side **solo estéreo**). Botones de escena = punto de partida.

**Delivery · Monitoring · Surround**
- *Normalize* + *Target LUFS* + *Manual gain* + lectura de ganancia.
- *Limiter* + *Ceiling dBFS* + reducción actual.
- *Listen in the room*, *A/B — dry*.
- *Rears dB* / *LFE dB*: mastering por canal (antes del limitador).

**Bass management** (surround con LFE)
- *Bass mgmt*, *Xover Hz* (Linkwitz-Riley 4º orden, 40–300 Hz), *LFE +dB*,
  *Send to LFE*, *HP mains* (todo el bajo al subwoofer). Auto-off en
  estéreo/mono.

**Downmix 5.1→2.0 y True peak**
- *Downmix*: L' = L + 0.707·C + 0.707·Ls (+ LFE −6 dB), aplicado **antes**
  del limitador; envolventes en silencio (print estéreo).
- *True peak*: limitador sobre el pico reconstruido (oversampling 4×,
  BS.1770/EBU R128).

### 4. Presets de usuario

Barra de la cabecera (independiente de los presets del DAW): nombre →
**Save**; elige en el desplegable → **Load** / **Del**. Viajan con el estado
del proyecto.

### 5. Consejos rápidos

| Problema | Solución |
|---|---|
| "El diálogo no se entiende en la sala" | Escena *Dialogue* + sala *Large* + Intensidad alta |
| "En el cine se oye apagado" | *Listen in the room* + sube *8k/Air* |
| "Mi video de YouTube es bajito" | Destino *Web* (−14 LUFS, techo −1 dBTP) |
| "Necesito entregar a Netflix" | Destino *Netflix* (−27 LKFS) |
| "Quiero subwoofer en el cine" | Pro → *Bass mgmt* + *HP mains* + *Send to LFE* (surround) |
| "Tengo 5.1 y entrego estéreo" | *Downmix 5.1→2.0* y enruta L/R del track |

### 6. Notas técnicas

- **Latencia: 0** muestras (el true-peak usa oversampling solo para análisis).
- **Medición LUFS sobre la entrada** (pre-procesado).
- Ventanas: momentáneo 400 ms, corto ~3 s, integrado con gating (absoluto
  −70 / relativo −10 LUFS) desde el último reset.
- Surround: pesos ITU BS.1770-4 por canal; anchura mid/side desactivada en
  multicanal; hasta 8 canales (discretos aceptados).

---

## Para desarrolladores

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target CineLab_VST3 -j
cp -r "build/install/VST3/PeDROLBY Surround.vst3" ~/.vst3/   # Linux
```

- macOS (universal): `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` + targets
  `CineLab_VST3 CineLab_AU`.
- Windows: `cmake --build build-win --target CineLab_VST3 --config Release -j`.
- **CI** (`.github/workflows/build.yml`): compila Linux/macOS/Windows y
  **publica una Release automática en cada tag `v*`**.
- Dependencias y solución de problemas: **docs/BUILD.md**.

### Tests

```bash
cmake -S . -B build -DCINELAB_BUILD_TESTS=ON
cmake --build build --target CineLabDSPTest CineLabLayoutTest -j
./build/CineLabDSPTest      # 9 secciones (curva X, limitador, LUFS,
                            # normalizador, escena, simulador, downmix,
                            # true-peak, bass management)
DISPLAY=:1 ./build/CineLabLayoutTest   # solapamientos UI + 5.1 + presets
```

### Estructura

```
Source/dsp/      Biquad, KWeighting, LoudnessMeter, CinemaEQ, RoomSimulator,
                 SceneModule, LoudnessNormalizer, SimpleLimiter, Downmixer,
                 BassManager, DeliveryStandards
Source/ui/       Tema, knobs con "?", medidores, vistas Director/Pro, PresetBar
Source/          PluginProcessor, PluginEditor, Parameters, Presets, UserPresets
tools/           Tests de DSP y de layout (+ renders PNG)
docs/            Diseño, build, guía REAPER, capturas
.github/workflows/build.yml   CI + autorelease por tag
juce/            JUCE 9.0.1 (submódulo)
```

## Licencia

**AGPLv3** (requisito del framework JUCE, que se usa bajo su licencia open
source). Ver `LICENSE`. Para distribución comercial cerrada, contacta para
una licencia comercial de JUCE.

## Roadmap

- **v0.1–v0.3 (hecho)**: curva X, normalización por estándar, escenas,
  medidores, simulador de sala, A/B, surround 5.1/7.1, downmix, true-peak,
  bass management, presets de usuario, CI + releases automáticas.
- **v0.4**: gestión de alturas (básico Atmos-upmix), detección automática de
  escena (IA ligera), presets con carpetas.