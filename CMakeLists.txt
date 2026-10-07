cmake_minimum_required(VERSION 3.22)
project(MFRTannhauser VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Sources and string literals are UTF-8 (the UI title has an a-umlaut). The plugin and file
# names stay ASCII: the VST3 wrapper widens the name byte by byte, which garbles UTF-8.
if(MSVC)
    add_compile_options(/utf-8)
endif()

# Offline DSP tests (no JUCE needed)
add_executable(TannTest tools/TannTest.cpp)

option(TANN_BUILD_PLUGIN "Build the plugin" ON)
if(TANN_BUILD_PLUGIN)
    add_subdirectory(JUCE)

    juce_add_plugin(Tannhauser
        COMPANY_NAME "Music For Robots"
        PLUGIN_MANUFACTURER_CODE MfRb
        PLUGIN_CODE Tnhs
        BUNDLE_ID com.musicforrobots.mfrtannhauser
        FORMATS VST3 Standalone
        PRODUCT_NAME "MFR-Tannhauser"
        IS_SYNTH FALSE
        NEEDS_MIDI_INPUT FALSE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        VST3_CATEGORIES Fx Distortion
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(Tannhauser PRIVATE
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)

    target_compile_definitions(Tannhauser PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)

    target_link_libraries(Tannhauser
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)

    # Renders the UI to a PNG with no audio device (for docs and the website).
    juce_add_console_app(TannSnapshot PRODUCT_NAME "TannSnapshot")
    target_sources(TannSnapshot PRIVATE
        tools/Snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_compile_definitions(TannSnapshot PRIVATE
        "JucePlugin_VersionString=\"${PROJECT_VERSION}\""
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(TannSnapshot
        PRIVATE
            juce::juce_audio_utils
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags)
endif()
