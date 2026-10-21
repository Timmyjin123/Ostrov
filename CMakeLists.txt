cmake_minimum_required(VERSION 3.22)
project(Ostrov VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW ON)
FetchContent_MakeAvailable(JUCE)

if(APPLE)
    set(OSTROV_FORMATS VST3 AU Standalone)
else()
    set(OSTROV_FORMATS VST3)
endif()

juce_add_plugin(Ostrov
    COMPANY_NAME "Timmyjin"
    BUNDLE_ID "com.timmyjin.ostrov"
    PLUGIN_MANUFACTURER_CODE Tmjn
    PLUGIN_CODE Ostr
    FORMATS ${OSTROV_FORMATS}
    PRODUCT_NAME "Ostrov"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

target_sources(Ostrov PRIVATE
    src/PluginProcessor.cpp
    src/PluginEditor.cpp)

target_include_directories(Ostrov PRIVATE src)

target_compile_definitions(Ostrov PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(NOT APPLE)
    target_compile_definitions(Ostrov PUBLIC JUCE_ALSA=0 JUCE_JACK=0)
endif()

target_link_libraries(Ostrov
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Offline demo renderer (pure C++, no JUCE)
add_executable(ostrov_render tools/render_demo.cpp)
