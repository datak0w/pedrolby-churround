# Build — CineLab (macOS · Linux · Windows)

Requiere **CMake ≥ 3.22**, **compilador C++17** y **JUCE 9** (ya incluido en
`juce/` como checkout en el repo; o `git clone --depth 1 --branch 9.0.1
https://github.com/juce-framework/JUCE.git juce`).

## Linux (Debian/Ubuntu/Kali y derivados)

Dependencias de sistema:

```bash
sudo apt install build-essential cmake \
  libasound2-dev libx11-dev libxext-dev libxinerama-dev libxrandr-dev \
  libxcursor-dev libfreetype-dev libfontconfig1-dev \
  mesa-common-dev libgl1-mesa-dev libegl1-mesa-dev     # OpenGL (opcional)
```

> Sin `libgl`/EGL el build funciona igual para VST3 (la UI usa software
> rendering de JUCE); instala `mesa-common-dev` para aceleración opcional.

Compilar:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target CineLab_VST3 -j$(nproc)
```

Resultado y destino:

- `build/CineLab_artefacts/Release/VST3/"PeDROLBY Surround.vst3"`
- Copiado a `build/install/VST3/"PeDROLBY Surround.vst3"` (por defecto, dentro del build).

Para instalarlo en REAPER, copia el bundle a `~/.vst3`:

```bash
cp -r build/install/VST3/"PeDROLBY Surround.vst3" ~/.vst3/
```

O configura la carpeta de instalación en el configure:

```bash
cmake -S . -B build -DCINELAB_VST3_INSTALL_DIR="$HOME/.vst3" -DCMAKE_BUILD_TYPE=Release
```

Otros targets: `CineLab_Standalone` (probador independiente),
`CineLab_LV2` (Ardour/Carla).

## macOS

Dependencias: Xcode Command Line Tools (`xcode-select --install`) o Xcode
completo. Sin dependencias externas.

```bash
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release
cmake --build build-mac --target CineLab_VST3 -j
cmake --build build-mac --target CineLab_AU    -j
```

Resultados:

- `~/Library/Audio/Plug-Ins/VST3/PeDROLBY Surround.vst3`
- `~/Library/Audio/Plug-Ins/Components/PeDROLBY Surround.component` (AU)

Universal (Apple Silicon + Intel) y firma ad-hoc:

```bash
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build-mac --target CineLab_VST3 CineLab_AU -j
codesign --force --deep --sign - "build-mac/CineLab_artefacts/Release/VST3/PeDROLBY Surround.vst3"
codesign --force --deep --sign - "build-mac/CineLab_artefacts/Release/AU/PeDROLBY Surround.component"
```

> Para distribución: sustituye la firma ad-hoc (`-`) por tu **Developer ID**.
> También puedes compilar los tres SO desde GitHub Actions
> (`.github/workflows/build.yml`, runner `macos-latest` con Xcode incluido)
> y descargar los artefactos compilados.

## Windows

Requiere Visual Studio 2022 (C++ workload) o MSVC en la path.

```powershell
cmake -S . -B build-win -DCMAKE_BUILD_TYPE=Release
cmake --build build-win --target CineLab_VST3 --config Release -j
```

Resultado: `build-win/CineLab_artefacts/Release/VST3/"PeDROLBY Surround.vst3"` (cópialo a
`C:\Program Files\Common Files\VST3\`).

## Tests

```bash
cmake -S . -B build -DCINELAB_BUILD_TESTS=ON
cmake --build build --target CineLabDSPTest -j
./build/tools/CineLabDSPTest
```

Verifica: ganancia de la curva X en 8 kHz ≫ 1 kHz, clamp del limitador,
medición LUFS de un tono de referencia y silencio.

## Problemas frecuentes

| Síntoma | Causa | Solución |
|---|---|---|
| `alsa/asoundlib.h not found` | falta libasound2-dev | `sudo apt install libasound2-dev` |
| `X11/Xlib.h not found` | falta x11 dev | `sudo apt install libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev` |
| REAPER no ve el plugin | escaneo/carpeta | Preferencias → Plug-ins → añadir `~/.vst3` y "Rescan" |
| UI gris o vacía | GPU/virtualización | Usa X forwarding o el modo software (por defecto) |
| Linker segfault al compilar (gcc 12) | `-flto` (LTO) inestable en GCC 12 | `cmake -S . -B build -DCINELAB_LTO=OFF` |
| `juce_lv2_helper` crashea (build LV2) | bug del helper con binutils/gcc 12 | El LV2 es opcional (Ardour/Carla). REAPER usa **VST3**; compila `--target CineLab_VST3` |

> Nota: el formato **LV2** se compila como objetivo extra y es *best-effort*:
> algunos toolchains (GCC 12 + binutils 2.40) hacen que el helper de empaquetado
> de JUCE falle. El formato oficial para REAPER es **VST3** (macOS/Linux/Windows)
> y **AU** en macOS.