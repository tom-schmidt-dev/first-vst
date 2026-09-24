cmake_minimum_required(VERSION 3.22)
project(DDSP_TIMBRE_PLUGIN VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# RPATH definieren, damit dynamische Bibliotheken relativ zur Binärdatei gesucht werden
set(CMAKE_BUILD_RPATH_USE_ORIGIN TRUE)
set(CMAKE_BUILD_RPATH "$ORIGIN")

include(FetchContent)
FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        8.0.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(DDSP_Timbre
    COMPANY_NAME "AudioAI"
    PLUGIN_MANUFACTURER_CODE AuAi
    PLUGIN_CODE Ddsp
    FORMATS VST3 Standalone
    PRODUCT_NAME "DDSP Timbre Transfer"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
)

target_sources(DDSP_Timbre PRIVATE
    source/PluginProcessor.cpp
    source/PluginProcessor.h
    source/PluginEditor.cpp
    source/PluginEditor.h
    source/InferenceWorker.h
)

# ONNX Runtime als explizites imported Target definieren
add_library(onnxruntime SHARED IMPORTED)
set_target_properties(onnxruntime PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime/lib/libonnxruntime.so"
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime/include"
)

# Header und Präprozessor-Flags mit PUBLIC an Standalone und VST3 vererben
target_include_directories(DDSP_Timbre PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../dsp/include
)

target_compile_definitions(DDSP_Timbre PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_CURL=0
)

target_link_libraries(DDSP_Timbre PUBLIC
    onnxruntime
    juce::juce_audio_utils
    juce::juce_audio_processors
)

juce_generate_juce_header(DDSP_Timbre)

# Modell und Bibliothek nach dem Build in das Verzeichnis der Binärdateien kopieren
add_custom_command(TARGET DDSP_Timbre_Standalone POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/ddsp_decoder.onnx
        $<TARGET_FILE_DIR:DDSP_Timbre_Standalone>/ddsp_decoder.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime/lib/libonnxruntime.so
        $<TARGET_FILE_DIR:DDSP_Timbre_Standalone>/libonnxruntime.so
)

add_custom_command(TARGET DDSP_Timbre_VST3 POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/ddsp_decoder.onnx
        $<TARGET_FILE_DIR:DDSP_Timbre_VST3>/ddsp_decoder.onnx
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_CURRENT_SOURCE_DIR}/libs/onnxruntime/lib/libonnxruntime.so
        $<TARGET_FILE_DIR:DDSP_Timbre_VST3>/libonnxruntime.so
)
