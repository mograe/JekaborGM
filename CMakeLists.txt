cmake_minimum_required(VERSION 3.22)

project(JekaborGM VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE)

FetchContent_MakeAvailable(JUCE)

find_package(PkgConfig REQUIRED)
pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)

juce_add_plugin(OurGM
    COMPANY_NAME Jekabor
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Jekb
    PLUGIN_CODE Ougm
    FORMATS VST3
    PRODUCT_NAME "OurGM"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(OurGM
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

target_compile_definitions(OurGM
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(OurGM
    PRIVATE
        juce::juce_audio_utils
        PkgConfig::FLUIDSYNTH
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
