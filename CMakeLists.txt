cmake_minimum_required(VERSION 3.20)

project(ITKLibrary LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/ITKLibrary/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/ITKLibrary/*.h"
    "${PROJECT_SOURCE_DIR}/ITKLibrary/*.hpp"
)

# DLL
add_library(${PROJECT_NAME} SHARED
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        ${PROJECT_SOURCE_DIR}/src
        D:/LIBS/install/DCMTK/bin_1/include
        D:/LIBS/install/ITK-master/bin_1/include/ITK-5.4
)

set_target_properties(${PROJECT_NAME}
    PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
)

# ============================================================
# Пути поиска .lib файлов
# ============================================================

set(DCMTK_LIB_DIR "D:/LIBS/install/DCMTK/bin_1/lib")
set(ITK_LIB_DIR "D:/LIBS/install/ITK-master/bin_1/lib")
set(ZLIB_LIB_DIR "D:/LIBS/install/zlib-1.3.1/lib")

# ============================================================
# Линковка библиотек
# ============================================================

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    ${DCMTK_LIB_DIR}/dcmdata.lib
    ${DCMTK_LIB_DIR}/oflog.lib
    ${DCMTK_LIB_DIR}/ofstd.lib

    ${ITK_LIB_DIR}/ITKCommon-5.4.lib
    ${ITK_LIB_DIR}/ITKIOGDCM-5.4.lib
    ${ITK_LIB_DIR}/ITKIOImageBase-5.4.lib
    ${ITK_LIB_DIR}/itksys-5.4.lib
    ${ITK_LIB_DIR}/itkv3p_netlib-5.4.lib
    ${ITK_LIB_DIR}/itkvnl_algo-5.4.lib
    ${ITK_LIB_DIR}/itkvnl-5.4.lib
    ${ITK_LIB_DIR}/ITKIODCMTK-5.4.lib

    ${ZLIB_LIB_DIR}/zlib.lib

    ws2_32
    netapi32
    iphlpapi
)