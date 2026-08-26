cmake_minimum_required(VERSION 3.22)

project(CineLab VERSION 0.1.0 LANGUAGES C CXX)

# ---------------------------------------------------------------------------
#  CineLab — Suite de audio para cine y video (postproducción)
#  Multiplataforma: macOS / Linux / Windows · REAPER (VST3, AU)
# ---------------------------------------------------------------------------

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# JUCE se incluye como subdirectorio (checkout local en ./juce)
# En macOS/Linux/Windows con CMake >= 3.22 + compilador C++17.
add_subdirectory(juce)

# MSVC: interpreta las fuentes como UTF-8 (los literales llevan — y ·)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# --- Plugin ----------------------------------------------------------------

set(CINELAB_FORMATS VST3 LV2 Standalone)
if(APPLE)
    list(APPEND CINELAB_FORMATS AU AUv3)
endif()

# Carpeta de instalación VST3 (por defecto dentro del build; cámbiala con
# -DCINELAB_VST3_INSTALL_DIR=~/.vst3 para instalación automática en REAPER).
set(CINELAB_VST3_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/install/VST3"
    CACHE PATH "Carpeta donde se copia el VST3 tras compilar")

juce_add_plugin(CineLab
    VENDOR_NAME "PeDROLBY"
    PLUGIN_MANUFACTURER_CODE "PDLB"
    PLUGIN_CODE "Pedr"
    VERSION "0.2.0"
    DESCRIPTION "PeDROLBY Surround - suite de audio para cine y video: EQ de sala (curva X ISO 2969), normalizacion LUFS por estandar de entrega, escenas, simulador de sala (pre-escucha), A/B y masterizacion multicanal (5.1/7.1)."
    PRODUCT_NAME "PeDROLBY Surround"
    COMPANY_NAME "PeDROLBY"
    COMPANY_WEBSITE "https://pedrolby.local"
    COMPANY_EMAIL "dev@pedrolby.local"
    BUNDLE_ID "lab.pedrolby.surround"
    FORMATS ${CINELAB_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_COPY_DIR "${CINELAB_VST3_INSTALL_DIR}"
    LV2_COPY_DIR "${CMAKE_CURRENT_BINARY_DIR}/install/LV2"
    VST3_CATEGORIES "Fx|EQ"
    LV2URI "urn:pedrolby:surround"
    LV2_PLUGIN_CLASS "EQPlugin"
    LV2_PLUGIN_CLASS_EXTENSIONS "ControlInput")
if(APPLE)
    set_target_properties(CineLab PROPERTIES JUCE_AU_COPY_DIR "$ENV{HOME}/Library/Audio/Plug-Ins/Components")
endif()

# --- Sources ----------------------------------------------------------------

target_sources(CineLab
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
        Source/ui/DirectorView.cpp
        Source/ui/ProView.cpp)

target_include_directories(CineLab PRIVATE Source)

# --- Modules / flags --------------------------------------------------------

target_compile_definitions(CineLab
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(CineLab
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# LTO (-flto): activo por defecto (Release). GCC 12 a veces produce un
# segfault del enlazador al enlazar el formato LV2; desactívelo con
# -DCINELAB_LTO=OFF si falla tu compilador.
option(CINELAB_LTO "Usar optimización de enlazado (LTO)" ON)
if(CINELAB_LTO)
    target_link_libraries(CineLab PUBLIC juce::juce_recommended_lto_flags)
endif()

# ---------------------------------------------------------------------------
#  Tests DSP (consola, sin DAW): cmake -DCINELAB_BUILD_TESTS=ON
# ---------------------------------------------------------------------------
option(CINELAB_BUILD_TESTS "Compilar el test de consola del DSP" OFF)

if(CINELAB_BUILD_TESTS)
    add_executable(CineLabDSPTest tools/dsptest.cpp)
    target_include_directories(CineLabDSPTest PRIVATE Source)
    target_compile_definitions(CineLabDSPTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(CineLabDSPTest
        PRIVATE
            juce::juce_audio_basics
            juce::juce_dsp
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)

    add_executable(CineLabLayoutTest tools/layouttest.cpp)
    target_include_directories(CineLabLayoutTest PRIVATE Source)
    target_compile_definitions(CineLabLayoutTest PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0)
    target_link_libraries(CineLabLayoutTest
        PRIVATE
            CineLab
            juce::juce_audio_utils
            juce::juce_gui_basics
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags)
endif()