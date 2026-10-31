cmake_minimum_required(VERSION 3.22)

project(InstantVirtualInstrument VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(IVI_BUILD_TESTS "Build the core engine test suite" ON)
set(IVI_JUCE_PATH "" CACHE PATH "Optional path to a local JUCE checkout (otherwise JUCE is downloaded)")

# ---------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------
if(IVI_JUCE_PATH)
    add_subdirectory("${IVI_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------
# Core engine sources (shared by the plugin and the tests)
# ---------------------------------------------------------------------------
set(IVI_CORE_SOURCES
    Source/Core/PitchTracker.cpp
    Source/Core/SpectralAnalyzer.cpp
    Source/Core/Instrument.cpp
    Source/Core/Articulations.cpp
    Source/Core/ResynthVoice.cpp
    Source/Core/SynthEngine.cpp
    Source/Core/MultisampleExporter.cpp
    Source/Core/DemoSource.cpp)

set(IVI_COMMON_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_DISPLAY_SPLASH_SCREEN=0)

if(MSVC)
    set(IVI_WARNINGS /W3 /utf-8)
else()
    set(IVI_WARNINGS -Wall -Wextra -Wno-unused-parameter)
endif()

# ---------------------------------------------------------------------------
# Plugin
# ---------------------------------------------------------------------------
set(IVI_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND IVI_FORMATS AU)
endif()

juce_add_plugin(InstantVirtualInstrument
    COMPANY_NAME                "Instant Virtual Instrument"
    PRODUCT_NAME                "Instant Virtual Instrument"
    BUNDLE_ID                   "com.instantvirtualinstrument.ivi"
    PLUGIN_MANUFACTURER_CODE    Ivin
    PLUGIN_CODE                 Ivi1
    FORMATS                     ${IVI_FORMATS}
    IS_SYNTH                    TRUE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Instrument Sampler Synth
    AU_MAIN_TYPE                kAudioUnitType_MusicDevice)

target_sources(InstantVirtualInstrument PRIVATE
    ${IVI_CORE_SOURCES}
    Source/Plugin/PluginProcessor.cpp
    Source/Plugin/PluginEditor.cpp
    Source/Plugin/Parameters.cpp
    Source/Plugin/Views.cpp
    Source/Plugin/Theme.cpp)

target_include_directories(InstantVirtualInstrument PRIVATE Source)
target_compile_options(InstantVirtualInstrument PRIVATE ${IVI_WARNINGS})

target_compile_definitions(InstantVirtualInstrument PUBLIC
    ${IVI_COMMON_DEFINITIONS}
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(InstantVirtualInstrument
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

# ---------------------------------------------------------------------------
# Tests (headless: analysis, resynthesis, layers, round robins, articulations)
# ---------------------------------------------------------------------------
if(IVI_BUILD_TESTS)
    enable_testing()
    juce_add_console_app(IVICoreTests PRODUCT_NAME "IVICoreTests")
    target_sources(IVICoreTests PRIVATE ${IVI_CORE_SOURCES} Tests/CoreTests.cpp)
    target_include_directories(IVICoreTests PRIVATE Source)
    target_compile_options(IVICoreTests PRIVATE ${IVI_WARNINGS})
    target_compile_definitions(IVICoreTests PRIVATE ${IVI_COMMON_DEFINITIONS})
    target_link_libraries(IVICoreTests
        PRIVATE
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags)
    add_test(NAME IVICoreTests COMMAND IVICoreTests)

    # Smoke test of the real processor/editor (needs a display on Linux, e.g. xvfb-run)
    add_executable(IVIPluginTests Tests/PluginTests.cpp)
    target_include_directories(IVIPluginTests PRIVATE Source
        $<TARGET_PROPERTY:InstantVirtualInstrument,INCLUDE_DIRECTORIES>)
    target_compile_definitions(IVIPluginTests PRIVATE
        $<TARGET_PROPERTY:InstantVirtualInstrument,COMPILE_DEFINITIONS>)
    target_link_libraries(IVIPluginTests PRIVATE InstantVirtualInstrument)
endif()
