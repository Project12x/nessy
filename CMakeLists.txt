cmake_minimum_required(VERSION 3.24)
project(Nessy VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CPM for dependency management
include(cmake/CPM.cmake)

# JUCE
CPMAddPackage(
    NAME JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    OPTIONS
        "JUCE_BUILD_EXAMPLES OFF"
        "JUCE_BUILD_EXTRAS OFF"
)

# ghostmoon-oss (sibling repo) — the distributable, GPL-compatible slice of
# Ghostmoon (MIT first-party + LGPL-2.1 ReverbSC). Nessy is GPL-3.0 (NSFPlay) and
# must NOT link the proprietary ghostmoon. Folder is still named ghostmoongpl.
set(GHOSTMOON_OSS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../ghostmoongpl"
    CACHE PATH "Path to the ghostmoon-oss repository")
add_subdirectory(${GHOSTMOON_OSS_DIR} ghostmoon_oss_build)

# Nessy VST Plugin
juce_add_plugin(Nessy
    COMPANY_NAME "AntigravityLabs"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Agra
    PLUGIN_CODE Nesy
    FORMATS VST3 Standalone
    PRODUCT_NAME "Nessy"
    COPY_PLUGIN_AFTER_BUILD TRUE
)

# Embedded fonts as BinaryData
juce_add_binary_data(NessyFonts
    SOURCES
        src/resources/fonts/Inter-Regular.ttf
        src/resources/fonts/Inter-Bold.ttf
        src/resources/fonts/PressStart2P-Regular.ttf
        src/resources/images/background.png
)

# Source files
target_sources(Nessy
    PRIVATE
        src/PluginProcessor.cpp
        src/PluginEditor.cpp
        
        # NessyAPU wrapper
        src/apu/NessyAPU.cpp
        src/apu/VoiceAllocator.cpp
        src/apu/MacroEngine.cpp
        src/apu/Arpeggiator.cpp
        
        # Blip_Buffer (LGPL - bandlimited synthesis)
        src/apu/blip_buffer/Blip_Buffer.cpp
        
        # NSFPlay cores (GPL)
        src/apu/nsfplay/xgm/devices/Sound/nes_apu.cpp
        src/apu/nsfplay/xgm/devices/Sound/nes_dmc.cpp
        src/apu/nsfplay/xgm/devices/Sound/nes_vrc6.cpp
)

# Include directories
target_include_directories(Nessy
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apu
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apu/blip_buffer
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apu/nsfplay
        ${CMAKE_CURRENT_SOURCE_DIR}/src/apu/utils
)

# JUCE modules
target_link_libraries(Nessy
    PRIVATE
        NessyFonts
        ghostmoon_oss::dsp
        ghostmoon_oss::core
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_core
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Plugin definitions
target_compile_definitions(Nessy
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

option(NESSY_BUILD_TESTS "Build the Nessy test suite" ON)
if(NESSY_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
