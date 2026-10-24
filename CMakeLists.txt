cmake_minimum_required(VERSION 3.22)

project(ShapeCrank VERSION 0.2.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(SHAPECRANK_FETCH_JUCE "Download JUCE with CMake FetchContent" OFF)
option(SHAPECRANK_BUILD_CHECKS "Build audio regression checks and editor snapshot tool" OFF)
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
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)

juce_generate_juce_header(ShapeCrankPlugin)

if (APPLE)
    # Relinking a local bundle can leave a stale resource signature. Sign after
    # JUCE's resources/manifest steps so every preview bundle can launch/load.
    foreach(pluginTarget ShapeCrankPlugin_Standalone ShapeCrankPlugin_VST3 ShapeCrankPlugin_AU)
        add_custom_command(TARGET ${pluginTarget} POST_BUILD
            COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:${pluginTarget}>"
            VERBATIM
        )
    endforeach()
endif()

if (SHAPECRANK_BUILD_CHECKS)
    enable_testing()
    add_executable(ShapeCrankChecks Tests/PluginChecks.cpp)
    target_include_directories(ShapeCrankChecks PRIVATE
        Source/Plugin
        "${CMAKE_CURRENT_BINARY_DIR}/ShapeCrankPlugin_artefacts/JuceLibraryCode"
        "${JUCE_MODULES_DIR}"
    )
    target_link_libraries(ShapeCrankChecks PRIVATE ShapeCrankPlugin)
    add_test(NAME ShapeCrankAudioChecks COMMAND ShapeCrankChecks)
endif()
