cmake_minimum_required(VERSION 3.22)
project(PapaFuzz VERSION 0.7.1)

if(APPLE)
  set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "" FORCE)
endif()

include(FetchContent)
FetchContent_Declare(
  juce
  GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
  GIT_TAG 7.0.12
)
FetchContent_MakeAvailable(juce)

juce_add_plugin(PapaFuzz
    COMPANY_NAME "EgoAgnosia"
    COMPANY_WEBSITE "https://example.com"
    PLUGIN_MANUFACTURER_CODE EgoA
    PLUGIN_CODE PFuz
    FORMATS AU VST3
    PRODUCT_NAME "Papa Fuzz"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
)

target_sources(PapaFuzz PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.h
)

target_compile_features(PapaFuzz PRIVATE cxx_std_20)

target_link_libraries(PapaFuzz PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
)

target_compile_definitions(PapaFuzz PRIVATE
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_INCLUDE_VST2_SDK=0
)

juce_add_binary_data(PapaFuzzData SOURCES
    Source/Resources/gui-bg.png
)
target_link_libraries(PapaFuzz PRIVATE PapaFuzzData)

if (CMAKE_BUILD_TYPE MATCHES "Release")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT lto_supported OUTPUT lto_error)
    if(lto_supported)
        set_property(TARGET PapaFuzz PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)
    endif()
endif()