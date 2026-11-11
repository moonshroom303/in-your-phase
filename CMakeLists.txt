cmake_minimum_required(VERSION 3.22)
project(InYourPhase VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Sources contain UTF-8 text (°, ä, ö, ü)
if(MSVC)
    add_compile_options(/utf-8)
endif()

# JUCE: -DJUCE_DIR=/path/to/JUCE, a "JUCE" folder next to this file,
# or automatically downloaded (pinned version) when neither exists.
set(LP_JUCE_VERSION "8.0.15")
set(JUCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/JUCE" CACHE PATH "Path to a JUCE 8 checkout")
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} ${CMAKE_BINARY_DIR}/juce)
else()
    include(FetchContent)
    message(STATUS "Downloading JUCE ${LP_JUCE_VERSION}")
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG ${LP_JUCE_VERSION}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# Audio Unit only on macOS, LV2 on Linux; VST3 and a standalone app everywhere
if(APPLE)
    set(LP_FORMATS AU VST3 Standalone)
elseif(UNIX)
    set(LP_FORMATS VST3 LV2 Standalone)
else()
    set(LP_FORMATS VST3 Standalone)
endif()

# Copying into the system plug-in folders is handy locally, not on CI
option(LP_COPY_PLUGINS "Copy plug-ins to the user plug-in folders after building" ON)

juce_add_plugin(InYourPhase
    PRODUCT_NAME "In Your Phase"
    COMPANY_NAME "moonshroom studio"
    BUNDLE_ID "com.moonshroomstudio.inyourphase"
    VERSION ${PROJECT_VERSION}
    PLUGIN_MANUFACTURER_CODE Mnsh
    PLUGIN_CODE Iyp1
    FORMATS ${LP_FORMATS}
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx Tools
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "In Your Phase braucht in der eigenständigen App Zugriff auf die Audioeingänge."
    COPY_PLUGIN_AFTER_BUILD ${LP_COPY_PLUGINS}
    LV2URI "https://github.com/moonshroom303/in-your-phase"
)

set(LP_SOURCES
    Source/DSP/HilbertConvolver.cpp
    Source/DSP/PhaseRotator.cpp
    Source/Analysis/ScopeAssembler.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/UI/LimeLookAndFeel.cpp
    Source/UI/ChannelPanel.cpp
    Source/UI/AnalysisViews.cpp
    Source/UI/MoonshroomPanel.cpp
)

# Logo für die Moonshroom-Studio-Karte
juce_add_binary_data(InYourPhaseData SOURCES Resources/moonshroom-studio.png)

set(LP_DEFINITIONS
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

set(LP_MODULES
    InYourPhaseData
    juce::juce_audio_utils
    juce::juce_dsp
    juce::juce_recommended_config_flags
)

target_sources(InYourPhase PRIVATE ${LP_SOURCES})
target_include_directories(InYourPhase PRIVATE Source)
target_compile_definitions(InYourPhase PUBLIC ${LP_DEFINITIONS})
target_link_libraries(InYourPhase PRIVATE ${LP_MODULES})

# Tests: DSP, Analyse und Plugin-Zustand, ohne Host
option(LP_BUILD_TESTS "Tests bauen" ON)
if(LP_BUILD_TESTS)
    juce_add_console_app(InYourPhaseTests PRODUCT_NAME "InYourPhaseTests")
    target_sources(InYourPhaseTests PRIVATE ${LP_SOURCES} Tests/InYourPhaseTests.cpp)
    target_include_directories(InYourPhaseTests PRIVATE Source)
    target_compile_definitions(InYourPhaseTests PRIVATE ${LP_DEFINITIONS})
    target_link_libraries(InYourPhaseTests PRIVATE ${LP_MODULES})
endif()
