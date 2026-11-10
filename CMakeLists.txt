cmake_minimum_required(VERSION 3.22)
project(VoVolt VERSION 0.1.0 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(juce GIT_REPOSITORY https://github.com/juce-framework/JUCE.git GIT_TAG 8.0.8 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(juce)

juce_add_plugin(VoVolt
    COMPANY_NAME "VoVolt Audio"
    BUNDLE_ID "audio.vovolt.plugin"
    PLUGIN_MANUFACTURER_CODE VvAu
    PLUGIN_CODE Vv01
    FORMATS VST3 Standalone
    PRODUCT_NAME "VoVolt"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

# Generate the legacy umbrella header required by PluginProcessor.h and PluginEditor.h.
juce_generate_juce_header(VoVolt)

target_sources(VoVolt PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/VoltCore.h
)

target_compile_definitions(VoVolt PRIVATE
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(VoVolt PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_audio_processors
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
)

option(VOVOLT_BUILD_TESTS "Build standalone DSP checks" ON)
if(VOVOLT_BUILD_TESTS)
    enable_testing()
    add_executable(VoVoltDSPTests Tests/VoltCoreTests.cpp)
    target_include_directories(VoVoltDSPTests PRIVATE Source)
    target_compile_features(VoVoltDSPTests PRIVATE cxx_std_20)
    add_test(NAME VoVoltDSPTests COMMAND VoVoltDSPTests)
endif()
