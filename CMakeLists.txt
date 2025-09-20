cmake_minimum_required(VERSION 3.20)

project(ImGenie)

# Option to control installation (disabled by default if used as subproject)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(IM_GENIE_INSTALL_DEFAULT ON)
else()
    set(IM_GENIE_INSTALL_DEFAULT OFF)
endif()

option(IM_GENIE_INSTALL "Install ImGenie library and headers" ${IM_GENIE_INSTALL_DEFAULT})

find_package(imgui QUIET) # fails quietly if not found (imgui is the target name for vcpkg and nixpkgs).

add_library(ImGenie
    ImGenie.cpp
    ImGenie.h
)

if(imgui_FOUND)
    target_link_libraries(ImGenie PUBLIC imgui::imgui)
    message(STATUS "ImGenie: Found imgui package, linking automatically")
else()
    message(STATUS "ImGenie: imgui package not found - ensure ImGui is available in your project")
endif()

# Use generator expressions for proper include directory handling
target_include_directories(ImGenie
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<INSTALL_INTERFACE:include>
)

if(UNIX)
    target_compile_options(ImGenie PUBLIC -Wno-unknown-pragmas)
endif()

# Installation configuration
if(IM_GENIE_INSTALL)
    include(GNUInstallDirs)

    # Install the library
    install(TARGETS ImGenie
       EXPORT ImGenieTargets
       ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
       LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
       RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Install headers
    install(FILES 
       ImGenie.h 
       DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install export targets
    install(EXPORT ImGenieTargets
       FILE ImGenieConfig.cmake
       NAMESPACE ImGenie::
       DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/ImGenie
    )
endif()