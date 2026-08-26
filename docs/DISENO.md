# PeDROLBY Surround — Suite de audio para cine y video (plugin VST3/AU)

> Diseño y visión del producto. Multiplataforma: **macOS, Linux, Windows**.
> Funciona en REAPER (y cualquier DAW con VST3/AU). Estéreo y **surround 5.1/7.1**.

---

## 1. La idea en una frase

> **Un plugin que convierte el audio de tu película en "audio de cine" — con un botón.**

CineLab es una suite todo-en-uno de producción y postproducción de audio para
video y cine: normalización, ecualización de sala, diálogo, limitación y
medición — todo en un solo plugin, con una cara **fácil (Directora)** pensada
para directores/as, editoras y dirección artística, y una cara **Pro**
para quién mezcla.

---

## 2. Dos caras del mismo plugin

### Modo DIRECTOR (fácil — sin experiencia requerida)
- **Sin knob de "ganancia"**: solo **escenas** y **destinos**.
- Pulsas la escena (`Diálogo`, `SFX`, `Música`, `Foley`, `Paisaje`, `Boom/LFE`),
  pulsas el destino (`Cine`, `TV`, `Web`, `Netflix`, `Podcast`) y el plugin
  hace el resto: ecualiza, normaliza a sonoridad legal y protege contra picos.
- Un solo control grande: **"Intensidad"** (cuánto efecto aplica, de *sutil* a
  *brutal de cine*).
- **Medidores sin números**: "Suena bien · Demasiado fuerte · Demasiado bajo"
  para humanos, con los números en pequeño para quién los sepa leer.
- **Etiquetas legibles**: en vez de "threshold −18 dB", "Agarre del diálogo".

### Modo PRO (full — para quién mezcla)
- Todos los controles: EQ Cinema editable (frecuencias, ganancias, tamaño de
  sala), compresor de diálogo, de-esser, normalizador LUFS por estándar
  (EBU R128 / ITU BS.1770), limitador true-peak, medidores K y LUFS
  (momentáneo, corto, integrado), downmix opcional.
- Presets de fábrica y guardado de presets de usuario (JUCE AudioProcessorValueTreeState).

---

## 3. Cadena de procesamiento

```
 IN ─► [Medidor LUFS (pesos ITU)] ─► [A/B] ─► [EQ Cinema (curva X)]
       ─► [Módulo de escena] ─► [Ganancia surround: traseros/LFE]
       ─► [Normalizador de sonoridad] ─► [Limitador] ─► [Simulador de sala] ─► OUT
```

| Etapa | Qué hace | Especificación |
|---|---|---|
| **Medidor** | Mide sonoridad y picos en tiempo real | LUFS momentáneo/short-term/integrado (K-weighting, ITU BS.1770), picos, *true peak* opcional vía oversampling |
| **EQ Cinema** | Ecualiza para salas grandes | Curva X ISO 2969: +3 dB/oct por encima de 2 kHz, atenuación de graves; ajuste por **tamaño de sala** |
| **Escena** | Acondiciona según contenido | Diálogo (presencia 2–5 kHz, de-esser, compresión suave), Música (ancho, aire), SFX (impacto), Foley (cuerpo), Boom/LFE (graves controlados) |
| **Normalizador** | Lleva la sonoridad al estándar del destino | Targets: Cine −24 LUFS (EBU R128 film), TV −23, Netflix −27 LKFS, YouTube/Web −14, Podcast −16, Spotify −14 |
| **Limitador** | Protege picos de distribución | Brickwall a −1 dBTP (streaming) / −3 dBTP (cinema DCP) con lookahead |

---

## 4. EQ Cinema — curva X (el corazón "cine")

Las salas de cine **absorben agudos** (público, aire acondicionado, alfombras)
y **acumulan graves**. La **curva X** (ISO 2969) es la ecualización estándar
que compensa eso en salas grandes.

CineLab la implementa como filtros reales (no una curva "dibujada"):

- **Bajos**: shelf por debajo de ~250 Hz, atenuación típica −4 dB @ 100 Hz
  (este es el sonido "seco y sólido" de cine, sin boom sucio).
- **Agudos**: subida de +3 dB/octava desde 2 kHz hasta 10 kHz.
- **Tamaño de sala** (innovación): un solo control que escala TODA la curva
  entre *sala pequeña / sala mediana / sala grande / IMAX*:
  - Sala pequeña → curva X suavizada (la que usa Dolby para re-ecualizar la
    mezcla de cine a casa).
  - Sala grande / IMAX → curva X completa.
- Corte por encima de 10 kHz regulable (el aire de una sala real se apaga).

Esto responde a: *"¿por qué en mi mezcla suena brillante pero en la sala
de cine suena apagado?"* — la curva X y el tamaño de sala lo arreglan al vuelo.

---

## 5. Normalización — "regla" (los números legales)

El plugin conoce los **estándares de entrega** de la industria:

| Destino | Estándar | Target |
|---|---|---|
| Cine (DCP) | R128 / SMPTE ST 202 | −24 LUFS, pico ≤ −3 dBTP |
| TV broadcast | EBU R128 / ATSC | −23 LUFS (±0.5) |
| Netflix | Netflix spec | −27 LKFS |
| YouTube / Web | YouTube spec | −14 LUFS (−1 dBTP) |
| Podcast | variable | −16 LUFS |

Dos modos de trabajo:
- **Automático (en vivo)**: el plugin mide sonoridad short-term con ventana
  deslizante e iguala el target con suavizado (ataque ~1 s, histéresis) —
  para iterar rápido mientras se mira la escena.
- **Analizador + "+X dB"**: botón *"Medir y ajustar"* que calcula el gain
  sugerido sobre un rango y lo aplica a mano si prefieres control total.

---

## 6. Innovación y facilidad (lo que pide el brief)

1. **La cara Directora**: cero jerga. Escena → Destino → Listo. Diseñado con
   directores/as y editoras en mente; no hace falta saber qué es un "threshold".
2. **"¿Esto sonará bien en una sala grande?"** — **Simulador de sala**:
   pre-escucha la mezcla *con la acústica de la sala destino* (pérdida de
   agudos + acumulación de graves reales de una sala), escalada por el tamaño
   de sala elegido. Complementa a la curva X: la X *compensa* la sala; el
   simulador *es* la sala. Juntos responden "¿cuánto compenso?".
3. **A/B original ↔ procesado**: comparación instantánea con un botón, para
   decidir "más cine" o "menos cine" sin adivinar.
4. **Intensidad**: un solo knob maestro que de-forma el efecto completo, y
   medidores en lenguaje humano ("Demasiado fuerte / Suena bien / Bajo").
5. **Presets por escena descritos en lenguaje humano** ("Diálogo: que se
   entienda en la última fila").
6. **Surround 5.1/7.1 y masterización multicanal**: medición LUFS con pesos
   ITU BS.1770-4 por canal (surrounds 1.41, LFE excluido), ganancia
   independiente para traseros y LFE, y anchura mid/side confinada al estéreo
   (en surround la imagen queda intacta). Listo para mezclas y entregas de
   cine en multicanales.
7. **Color de película**: paleta oscura tipo sala de proyección; el UI
   comunica estado con color (verde = listo, ámbar = ajustando, rojo = picos).
8. **Roadmap IA (post v1)**: detección automática de escena (diálogo/música)
   y sugerencia de preset — primero presets manuales e inteligentes.

---

## 7. Especificación técnica

- **Framework**: JUCE 9 (C++17/20), CMake ≥ 3.20.
- **Formatos**: VST3 (macOS/Linux/Windows) + AU (macOS). REAPER los carga
  nativamente en las tres plataformas.
- **DSP**: doble precisión, sin bloqueos en el hilo de audio, cálculo de
  metering en ventana de 400 ms (short-term) con solapamiento, K-weighting
  (bessel high-pass 38 Hz + high-shelf ~1.5 kHz) conforme a BS.1770-4.
- **Estado**: AudioProcessorValueTreeState → parámetros automatizables y
  presets guardables; UI en `paint()`/`resized()` (drawables, no imágenes).
- **Latencia**: ~0 en el modo Director puro (EQ+gain+limiter); la medición
  nunca introduce latencia en la ruta de audio.

---

## 8. Roadmap

- **v0.1–v0.2 (hecho)**: esqueleto JUCE compilable en Linux, EQ Cinema +
  normalizador + limitador + modos y presets + medidores; simulador de sala
  (pre-escucha), A/B, y surround 5.1/7.1 con medición ITU. Verificable en
  REAPER Linux.
- **v0.3**: true-peak (oversampling), downmix 5.1→2.0, gestión LFE/graves,
  builds macOS (AU+VST3), Windows e instaladores.
- **v0.4**: detección automática de escena (IA ligera), sugerencia de preset.