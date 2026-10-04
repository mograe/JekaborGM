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

set(JEKABORGM_FLUIDSYNTH_ROOT "" CACHE PATH "FluidSynth installation prefix")
if(WIN32 AND NOT JEKABORGM_FLUIDSYNTH_ROOT
   AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.toolchain/fluidsynth-win")
    set(JEKABORGM_FLUIDSYNTH_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.toolchain/fluidsynth-win")
endif()
if(JEKABORGM_FLUIDSYNTH_ROOT)
    list(PREPEND CMAKE_PREFIX_PATH "${JEKABORGM_FLUIDSYNTH_ROOT}")
endif()

find_package(FluidSynth CONFIG QUIET)
if(TARGET FluidSynth::libfluidsynth)
    set(JEKABORGM_FLUIDSYNTH_TARGET FluidSynth::libfluidsynth)
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(FLUIDSYNTH REQUIRED IMPORTED_TARGET fluidsynth)
    set(JEKABORGM_FLUIDSYNTH_TARGET PkgConfig::FLUIDSYNTH)
endif()

juce_add_plugin(JekaborGM
    COMPANY_NAME Jekabor
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    PLUGIN_MANUFACTURER_CODE Jekb
    PLUGIN_CODE Jkgm
    FORMATS VST3
    VST3_AUTO_MANIFEST FALSE
    PRODUCT_NAME "JekaborGM"
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(JekaborGM
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginProcessor.h
        Source/PluginEditor.cpp
        Source/PluginEditor.h)

target_compile_definitions(JekaborGM
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(JekaborGM
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        ${JEKABORGM_FLUIDSYNTH_TARGET}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Package the Windows runtime dependencies beside the VST3 module before
# generating its manifest.
if(WIN32)
    if(MSVC AND TARGET FluidSynth::libfluidsynth)
        # Hosts do not necessarily search the VST3 directory for imported DLLs.
        # Load FluidSynth explicitly from the module directory before its first use.
        target_compile_definitions(JekaborGM PRIVATE
            JEKABORGM_DELAY_LOAD_FLUIDSYNTH=1
            JEKABORGM_FLUIDSYNTH_DLL_NAME="$<TARGET_FILE_NAME:FluidSynth::libfluidsynth>")
        target_link_options(JekaborGM_VST3 PRIVATE
            "/DELAYLOAD:$<TARGET_FILE_NAME:FluidSynth::libfluidsynth>")
        target_link_libraries(JekaborGM_VST3 PRIVATE delayimp)
    endif()

    set(JEKABORGM_FLUIDSYNTH_RUNTIME_DIR "${JEKABORGM_FLUIDSYNTH_ROOT}/bin"
        CACHE PATH "Directory containing FluidSynth and its runtime DLLs")
    file(GLOB JEKABORGM_RUNTIME_DLLS CONFIGURE_DEPENDS
        "${JEKABORGM_FLUIDSYNTH_RUNTIME_DIR}/*.dll")
    if(NOT JEKABORGM_RUNTIME_DLLS)
        message(FATAL_ERROR "Set JEKABORGM_FLUIDSYNTH_RUNTIME_DIR to the FluidSynth DLL directory")
    endif()
    add_custom_command(TARGET JekaborGM_VST3 POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${JEKABORGM_RUNTIME_DLLS}
            "$<TARGET_FILE_DIR:JekaborGM_VST3>"
        VERBATIM)
endif()

juce_enable_vst3_manifest_step(JekaborGM)

option(JEKABORGM_BUILD_TESTS "Build processor regression checks and UI snapshots" OFF)
if(JEKABORGM_BUILD_TESTS)
    juce_add_console_app(JekaborGMVerification PRODUCT_NAME "JekaborGM Verification")
    target_sources(JekaborGMVerification PRIVATE Tests/Verification.cpp)
    target_include_directories(JekaborGMVerification PRIVATE Source)
    target_compile_definitions(JekaborGMVerification PRIVATE
        JucePlugin_Name="JekaborGM" JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(JekaborGMVerification PRIVATE JekaborGM
        juce::juce_audio_utils juce::juce_dsp ${JEKABORGM_FLUIDSYNTH_TARGET}
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    if(WIN32 AND MSVC AND TARGET FluidSynth::libfluidsynth)
        target_link_libraries(JekaborGMVerification PRIVATE delayimp)
        target_link_options(JekaborGMVerification PRIVATE
            "/DELAYLOAD:$<TARGET_FILE_NAME:FluidSynth::libfluidsynth>")
        add_custom_command(TARGET JekaborGMVerification POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different ${JEKABORGM_RUNTIME_DLLS}
                "$<TARGET_FILE_DIR:JekaborGMVerification>" VERBATIM)
    endif()
    enable_testing()
    add_test(NAME JekaborGMVerification COMMAND JekaborGMVerification
        "${CMAKE_CURRENT_BINARY_DIR}/verification")
endif()
