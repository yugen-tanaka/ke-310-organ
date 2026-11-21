cmake_minimum_required(VERSION 3.22)

project(ke-310-organ VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(ke310_dsp INTERFACE)
target_include_directories(ke310_dsp INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
)
target_compile_features(ke310_dsp INTERFACE cxx_std_20)

function(ke310_apply_warnings target_name)
    if(MSVC)
        target_compile_options(${target_name} PRIVATE /W4)
    else()
        target_compile_options(${target_name} PRIVATE -Wall -Wextra -Wpedantic -Wshadow)
    endif()
endfunction()

option(KE310_BUILD_TESTS "Build Catch2 DSP/Plugin unit tests and parity CLI" ON)
option(KE310_BUILD_PLUGIN "Build JUCE 8 VST3/AU/Standalone plugin and plugin tests" ON)

include(FetchContent)

if(KE310_BUILD_PLUGIN)
    FetchContent_Declare(
        JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(JUCE)

    juce_add_plugin(KE310Organ
        COMPANY_NAME "Yuguen Organ"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE
        BUNDLE_ID "com.yuguenorgan.ke310"
        PLUGIN_MANUFACTURER_CODE YgOr
        PLUGIN_CODE K310
        FORMATS VST3 AU Standalone
        PRODUCT_NAME "KE-310 Organ"
        ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_1024.png"
        ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_256.png"
    )

    target_sources(KE310Organ PRIVATE
        src/plugin/PluginProcessor.cpp
        src/plugin/PluginEditor.cpp
    )

    target_compile_features(KE310Organ PUBLIC cxx_std_20)

    target_compile_definitions(KE310Organ PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
    )

    target_link_libraries(KE310Organ PRIVATE
        ke310_dsp
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
    )

    # Suppress third-party JUCE 8.0.6 internal Apple SDK / bundled HarfBuzz warnings on modern Xcode
    if(APPLE)
        target_compile_options(KE310Organ PUBLIC
            -Wno-unknown-warning-option
            -Wno-nontrivial-memcall
            -Wno-deprecated-declarations
        )
    endif()

    # Apply strict warnings only to our own plugin source files, not JUCE wrapper code
    if(MSVC)
        set_source_files_properties(src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp
            PROPERTIES COMPILE_OPTIONS /W4)
    else()
        set_source_files_properties(src/plugin/PluginProcessor.cpp src/plugin/PluginEditor.cpp
            PROPERTIES COMPILE_OPTIONS "-Wall;-Wextra;-Wpedantic;-Wshadow")
    endif()
endif()

if(KE310_BUILD_TESTS)
    include(CTest)

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.7.1
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
    include(Catch)

    add_executable(ke310_dsp_tests tests/test_ke310_dsp.cpp)
    target_link_libraries(ke310_dsp_tests PRIVATE ke310_dsp Catch2::Catch2WithMain)
    ke310_apply_warnings(ke310_dsp_tests)
    catch_discover_tests(ke310_dsp_tests)

    add_executable(ke310_render_cli tests/ke310_render_cli.cpp)
    target_link_libraries(ke310_render_cli PRIVATE ke310_dsp)
    ke310_apply_warnings(ke310_render_cli)

    if(KE310_BUILD_PLUGIN)
        add_executable(ke310_plugin_tests tests/test_ke310_plugin.cpp)
        target_link_libraries(ke310_plugin_tests PRIVATE
            KE310Organ
            ke310_dsp
            juce::juce_audio_processors
            juce::juce_gui_basics
            Catch2::Catch2WithMain
        )
        ke310_apply_warnings(ke310_plugin_tests)
        catch_discover_tests(ke310_plugin_tests)
    endif()
endif()
