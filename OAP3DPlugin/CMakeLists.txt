set(PROJECT_NAME Pluginsoap3d)

################################################################################
# Source groups
################################################################################
set(Header_Files
    "../SolarEngine/ModelLoader.h"
)
source_group("Header Files" FILES ${Header_Files})

set(Source_Files
    "../SolarEngine/ModelLoader.cpp"
    "ReaderWriterOAP3D.cpp"
)
source_group("Source Files" FILES ${Source_Files})

set(ALL_FILES
    ${Header_Files}
    ${Source_Files}
)

################################################################################
# Target
################################################################################
add_library(${PROJECT_NAME} SHARED ${ALL_FILES})

use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "${DEFAULT_CXX_PROPS}")
################################################################################
# Includes for CMake from *.props
################################################################################
use_props(${PROJECT_NAME} "${CMAKE_CONFIGURATION_TYPES}" "do_not_import_user.cmake")

set_target_properties(${PROJECT_NAME} PROPERTIES
    VS_GLOBAL_KEYWORD "Win32Proj"
)
################################################################################
# Target name
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    TARGET_NAME_RELEASE  "osgdb_oap3d"
    TARGET_NAME_RELEASED "osgdb_oap3d"
)
################################################################################
# Output directory
################################################################################
set_target_properties(${PROJECT_NAME} PROPERTIES
    OUTPUT_DIRECTORY_RELEASE  "${CMAKE_SOURCE_DIR}//bin/osgPlugins-3.6.4/"
    OUTPUT_DIRECTORY_RELEASED "${CMAKE_SOURCE_DIR}//bin/osgPlugins-3.6.4/"
)
################################################################################
# MSVC runtime library
################################################################################
get_property(MSVC_RUNTIME_LIBRARY_DEFAULT TARGET ${PROJECT_NAME} PROPERTY MSVC_RUNTIME_LIBRARY)
string(CONCAT "MSVC_RUNTIME_LIBRARY_STR"
    MultiThreadedDLL
)
set_target_properties(${PROJECT_NAME} PROPERTIES MSVC_RUNTIME_LIBRARY ${MSVC_RUNTIME_LIBRARY_STR})

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
    "WIN32;"
    "_WINDOWS;"
    "NDEBUG;"
    "_WIN32_WINNT=0x0A00;"
    "_SCL_SECURE_NO_WARNINGS;"
    "_CRT_SECURE_NO_DEPRECATE;"
    "LIB3DS_STATIC;"
    "CMAKE_INTDIR=\"Release\";"
    "osgdb_3ds_EXPORTS;"
    "_MBCS"
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
        /Zi;
        /TP;
        /GR;
        /wd4589;
        /wd4706;
        /wd4127;
        /wd4100;
        /wd4456;
        /utf-8;
        ${DEFAULT_CXX_EXCEPTION_HANDLING};
        /Y-
    )
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:ReleaseD>:
            /DEBUG
        >
        /MANIFEST;
        /SUBSYSTEM:CONSOLE;
        /INCREMENTAL:NO;
        /machine:x64
    )
endif()

################################################################################
# Dependencies
################################################################################
set(ADDITIONAL_LIBRARY_DEPENDENCIES
    "osgDB;"
    "osgUtil;"
    "opengl32;"
    "osg;"
    "OpenThreads;"
    "zlib;"
    "Qt5Core;"
    "kernel32;"
    "user32;"
    "gdi32;"
    "winspool;"
    "shell32;"
    "ole32;"
    "oleaut32;"
    "uuid;"
    "comdlg32;"
    "advapi32"
)
target_link_libraries(${PROJECT_NAME} PUBLIC "${ADDITIONAL_LIBRARY_DEPENDENCIES}")

target_link_directories(${PROJECT_NAME} PUBLIC
    "${CMAKE_SOURCE_DIR}//lib"
)

