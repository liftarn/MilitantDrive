cmake_minimum_required(VERSION 3.22)
project(MilitantDrive VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Point JUCE_DIR to your local JUCE clone or use FetchContent:
include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.4
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(MilitantDrive
    COMPANY_NAME "Militant Audio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE MltA
    PLUGIN_CODE Mdrv
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "Militant Drive"
)

target_sources(MilitantDrive
    PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp
)

target_compile_definitions(MilitantDrive
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_EFFECTS=1
)

target_link_libraries(MilitantDrive
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(APPLE AND CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    foreach(_target IN ITEMS MilitantDrive MilitantDrive_VST3 MilitantDrive_AU MilitantDrive_Standalone)
        if(TARGET ${_target})
            target_compile_options(${_target} PRIVATE
                $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-unknown-warning-option>
                $<$<COMPILE_LANGUAGE:CXX,OBJCXX>:-Wno-nontrivial-memcall>
            )
        endif()
    endforeach()
endif()

# Auto-embed Barlow .ttf font files if placed in Fonts/
file(GLOB MILITANT_FONT_FILES "${CMAKE_CURRENT_SOURCE_DIR}/Fonts/*.ttf")
if(MILITANT_FONT_FILES)
    juce_add_binary_data(MilitantDriveFonts SOURCES ${MILITANT_FONT_FILES})
    target_link_libraries(MilitantDrive PRIVATE MilitantDriveFonts)
endif()
