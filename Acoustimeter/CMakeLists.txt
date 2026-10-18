cmake_minimum_required(VERSION 3.16)
project(Acoustimeter VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOUIC_SEARCH_PATHS ${CMAKE_CURRENT_SOURCE_DIR}/src)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

qt_standard_project_setup()

set(SOURCES
    src/main.cpp
    src/LoginWindow.cpp
    src/MainWindow.cpp
    src/ConfigWindow.cpp
    src/HydroWidget.cpp
    src/AmplifierWidget.cpp
    src/ToolsWidget.cpp
    src/GraphWidget.cpp
    src/StyleManager.cpp
    src/HardwareManager.cpp
    src/MliGenerator.cpp
    src/MliOutput.cpp
)

set(HEADERS
    include/LoginWindow.h
    include/MainWindow.h
    include/ConfigWindow.h
    include/HydroWidget.h
    include/AmplifierWidget.h
    include/ToolsWidget.h
    include/GraphWidget.h
    include/StyleManager.h
    include/HardwareManager.h
    include/MliGenerator.h
    include/MliOutput.h
    include/HydroModels.h
    include/DataTypes.h
)

set(UI_FILES
    src/LoginWindow.ui
    src/MainWindow.ui
)

qt_add_executable(Acoustimeter ${SOURCES} ${HEADERS} ${UI_FILES})
target_include_directories(Acoustimeter PRIVATE include)
target_link_libraries(Acoustimeter PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# ---------------------------------------------------------------- NI-DAQmx
# Production : -DACOUSTIMETER_WITH_NI=ON
option(ACOUSTIMETER_WITH_NI "Compiler avec NI-DAQmx (acces materiel reel)" OFF)
set(NI_DAQMX_DIR "C:/Program Files (x86)/National Instruments/Shared/ExternalCompilerSupport/C"
    CACHE PATH "Dossier ExternalCompilerSupport/C de NI-DAQmx (contient include/ et lib64/msvc/)")
if(ACOUSTIMETER_WITH_NI)
    target_compile_definitions(Acoustimeter PRIVATE ACOUSTIMETER_WITH_NI)
    target_include_directories(Acoustimeter PRIVATE "${NI_DAQMX_DIR}/include")
    target_link_directories(Acoustimeter PRIVATE "${NI_DAQMX_DIR}/lib64/msvc")
    target_link_libraries(Acoustimeter PRIVATE NIDAQmx)
endif()

# ---------------------------------------------------------------- Sortie MLI
# NONE       : pas de sortie MLI materielle (la generation MLI fonctionne, l'envoi a la carte est refuse)
# NIDIGITAL  : carte PXIe-6570 (API niDigital)   <-- a changer si la carte est une PXIe-6537
set(ACOUSTIMETER_MLI_BACKEND "NONE" CACHE STRING "NONE | NIDIGITAL")
set_property(CACHE ACOUSTIMETER_MLI_BACKEND PROPERTY STRINGS NONE NIDIGITAL)
set(NI_DIGITAL_INCLUDE_DIR "C:/Program Files (x86)/IVI Foundation/IVI/Include" CACHE PATH "niDigital.h")
set(NI_DIGITAL_LIB_DIR "C:/Program Files (x86)/IVI Foundation/IVI/Lib_x64/msc" CACHE PATH "niDigital.lib")
if(ACOUSTIMETER_MLI_BACKEND STREQUAL "NIDIGITAL")
    target_compile_definitions(Acoustimeter PRIVATE ACOUSTIMETER_MLI_NIDIGITAL)
    target_include_directories(Acoustimeter PRIVATE "${NI_DIGITAL_INCLUDE_DIR}")
    target_link_directories(Acoustimeter PRIVATE "${NI_DIGITAL_LIB_DIR}")
    target_link_libraries(Acoustimeter PRIVATE niDigital)
endif()

set_target_properties(Acoustimeter PROPERTIES WIN32_EXECUTABLE TRUE MACOSX_BUNDLE TRUE)
