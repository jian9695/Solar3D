set(PROJECT_NAME Solar3D)

################################################################################
# Source groups
################################################################################
set(no_group_source_files
    "../bin/tests/boston.earth"
    "../bin/tests/boston_projected.earth"
    "Main.cpp"
)
source_group("" FILES ${no_group_source_files})

set(Examples
    "../bin/tests/solar3d_global.earth"
    "../bin/tests/solar3d_projected.earth"
)
source_group("Examples" FILES ${Examples})

set(SolarEngine
    "../SolarEngine/Cubemap.cpp"
    "../SolarEngine/Cubemap.h"
    "../SolarEngine/CustomControls.cpp"
    "../SolarEngine/CustomControls.h"
    "../SolarEngine/GrassSolar.cpp"
    "../SolarEngine/GrassSolar.h"
    "../SolarEngine/ModelLoader.cpp"
    "../SolarEngine/ModelLoader.h"
    "../SolarEngine/PointsRenderer.cpp"
    "../SolarEngine/PointsRenderer.h"
    "../SolarEngine/RenderSurface.cpp"
    "../SolarEngine/RenderSurface.h"
    "../SolarEngine/SolarInteractiveHandler.cpp"
    "../SolarEngine/SolarInteractiveHandler.h"
    "../SolarEngine/TypeDef.cpp"
    "../SolarEngine/TypeDef.h"
    "../SolarEngine/Utils.cpp"
    "../SolarEngine/Utils.h"
)
source_group("SolarEngine" FILES ${SolarEngine})

set(ALL_FILES
    ${no_group_source_files}
    ${Examples}
    ${SolarEngine}
)

################################################################################
# Target
################################################################################
add_executable(${PROJECT_NAME} ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
set(ROOT_NAMESPACE Registration)

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_RELEASE  "${CMAKE_SOURCE_DIR}//bin"
    OUTPUT_DIRECTORY_RELEASED "${CMAKE_SOURCE_DIR}//bin"
)
set_target_properties(${PROJECT_NAME} PROPERTIES
    INTERPROCEDURAL_OPTIMIZATION          "TRUE"
)
################################################################################
# Include directories
################################################################################
target_include_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../include;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../include/QtCore;"
    "${CMAKE_CURRENT_SOURCE_DIR}/../SolarEngine"
)

################################################################################
# Compile definitions
################################################################################
target_compile_definitions(${PROJECT_NAME} PRIVATE
    "NDEBUG;"
    "_CONSOLE;"
    "UNICODE;"
    "_UNICODE"
)

################################################################################
# Compile and link options
################################################################################
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:
            /O2
        >
        $<$<CONFIG:ReleaseD>:
            /Od
        >
        /Oi;
        /Gy;
        /W3;
        ${DEFAULT_CXX_DEBUG_INFORMATION_FORMAT};
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:ReleaseD>:
            /DEBUG
        >
        /SUBSYSTEM:CONSOLE;
        /OPT:REF;
        /OPT:ICF;
        /INCREMENTAL:NO
    )
endif()

################################################################################
# Dependencies
################################################################################
set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "osgEarth;"
    "osgEarthFeatures;"
    "osgEarthUtil;"
    "osgEarthSymbology;"
    "osgEarthAnnotation;"
    "osg;"
    "osgDB;"
    "osgUtil;"
    "osgViewer;"
    "OpenThreads;"
    "osgSim;"
    "osgTerrain;"
    "osgFX;"
    "osgShadow;"
    "osgManipulator;"
    "osgText;"
    "osgGA;"
    "Qt5Core"
)
target_link_libraries(${PROJECT_NAME} PRIVATE "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

target_link_directories(${PROJECT_NAME} PRIVATE
    "${CMAKE_SOURCE_DIR}//lib"
)

