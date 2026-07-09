cmake_minimum_required(VERSION 3.22)

project(ShapeCrank VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(SHAPECRANK_FETCH_JUCE "Download JUCE with CMake FetchContent" OFF)
set(JUCE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../DoomEngine/VoidLoomJUCE/JUCE" CACHE PATH "Path to a local JUCE checkout.")
set(JUCE_GIT_TAG "8.0.12" CACHE STRING "JUCE tag or commit used when SHAPECRANK_FETCH_JUCE is enabled.")

if (JUCE_PATH AND EXISTS "${JUCE_PATH}/CMakeLists.txt")
    add_subdirectory("${JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
elseif (EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
elseif (SHAPECRANK_FETCH_JUCE)
    include(FetchContent)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG "${JUCE_GIT_TAG}"
    )
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR "JUCE not found. Configure with -DJUCE_PATH=/path/to/JUCE or set SHAPECRANK_FETCH_JUCE=ON after checking licensing.")
endif()

juce_add_plugin(ShapeCrankPlugin
    COMPANY_NAME "DevMuse"
    BUNDLE_ID "com.devmuse.shapecrank"
    PLUGIN_MANUFACTURER_CODE DvMs
    PLUGIN_CODE ShCk
    FORMATS Standalone VST3 AU
    PRODUCT_NAME "Shape Crank"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Shape Crank uses audio input so the standalone app can process instruments and microphones."
    COPY_PLUGIN_AFTER_BUILD FALSE
)

target_sources(ShapeCrankPlugin PRIVATE
    Source/Plugin/ShapeCrankProcessor.cpp
    Source/Plugin/ShapeCrankEditor.cpp
)

juce_add_binary_data(ShapeCrankAssets SOURCES
    Assets/footswitch.png
    Assets/knob.png
    Assets/led_off.png
    Assets/led_on.png
    Assets/pedal_body_clean.png
    Assets/texture_overlay_clean.png
)

target_include_directories(ShapeCrankPlugin PRIVATE
    Source
    Source/DSP
    Source/Plugin
)

target_compile_definitions(ShapeCrankPlugin PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(ShapeCrankPlugin PRIVATE
    ShapeCrankAssets
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(ShapeCrankPlugin)
