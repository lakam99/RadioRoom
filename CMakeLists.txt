cmake_minimum_required(VERSION 3.22)
project(RadioRoom VERSION 0.2.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/JUCE)
juce_add_binary_data(RadioAssets SOURCES
 resources/generated/ivory.png resources/generated/graphite.png resources/generated/walnut.png
 resources/generated/knob.png resources/generated/meter.png resources/generated/switches.png
 resources/generated/buttons.png resources/generated/lamps.png resources/generated/vfd.png
 resources/generated/transport.png resources/generated/app-icon.png)
juce_add_plugin(RadioRoom COMPANY_NAME "Lakam Audio" BUNDLE_ID "audio.lakam.radioroom"
 PLUGIN_MANUFACTURER_CODE Lkam PLUGIN_CODE Rdrm FORMATS VST3 PRODUCT_NAME "RadioRoom"
 IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE COPY_PLUGIN_AFTER_BUILD FALSE
 VST3_CATEGORIES Fx Reverb Distortion Dynamics NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE
 ICON_BIG resources/generated/app-icon.png ICON_SMALL resources/generated/app-icon.png)
target_sources(RadioRoom PRIVATE Source/Engine.cpp Source/VinylTexture.cpp Source/Processor.cpp Source/Editor.cpp
 Source/RasterAssets.cpp Source/VfdDisplay.cpp)
target_include_directories(RadioRoom PUBLIC Source "${CMAKE_CURRENT_SOURCE_DIR}/third_party/JUCE/modules")
target_compile_definitions(RadioRoom PUBLIC JUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 JUCE_WEB_BROWSER=0
 RADIOROOM_VERSION="${PROJECT_VERSION}" JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0 JUCE_REPORT_APP_USAGE=0
 JUCE_PLUGINHOST_VST3=1 JUCE_MODAL_LOOPS_PERMITTED=1)
target_link_libraries(RadioRoom PRIVATE RadioAssets juce::juce_audio_utils juce::juce_dsp
 PUBLIC juce::juce_recommended_config_flags)
juce_add_gui_app(RadioRoomStandalone COMPANY_NAME "Lakam Audio" PRODUCT_NAME "RadioRoom"
 NEEDS_WEB_BROWSER FALSE NEEDS_CURL FALSE
 ICON_BIG resources/generated/app-icon.png ICON_SMALL resources/generated/app-icon.png)
target_sources(RadioRoomStandalone PRIVATE Source/Standalone.cpp Source/Tests.cpp Source/Updater.cpp)
target_link_libraries(RadioRoomStandalone PRIVATE RadioRoom juce::juce_recommended_config_flags)
add_custom_command(TARGET RadioRoomStandalone POST_BUILD COMMAND ${CMAKE_COMMAND} -E copy_if_different
 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/RadioRoom-Update.ps1"
 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Release-Common.ps1"
 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Install-RadioRoom.ps1"
 "${CMAKE_CURRENT_SOURCE_DIR}/scripts/Uninstall-RadioRoom.ps1"
 "$<TARGET_FILE_DIR:RadioRoomStandalone>")
if(WIN32)
 target_link_libraries(RadioRoomStandalone PRIVATE dwmapi)
endif()
