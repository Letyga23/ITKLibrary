cmake_minimum_required(VERSION 3.20)

project(ITKLibrary LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.cpp"
)

file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/src/*.h"
    "${PROJECT_SOURCE_DIR}/src/*.hpp"
)

add_library(${PROJECT_NAME} STATIC
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    ${PROJECT_SOURCE_DIR}/src
)

if(WIN32)

    target_include_directories(${PROJECT_NAME} PRIVATE
        D:/LIBS/install/DCMTK/bin_1/include
        D:/LIBS/install/ITK-master/bin_1/include/ITK-5.4
    )

    set(DCMTK_LIB_DIR "D:/LIBS/install/DCMTK/bin_1/lib")
    set(ITK_LIB_DIR "D:/LIBS/install/ITK-master/bin_1/lib")
    set(ZLIB_LIB_DIR "D:/LIBS/install/zlib-1.3.1/lib")

    target_link_libraries(${PROJECT_NAME} PRIVATE
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

elseif(UNIX)

    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_C_VISIBILITY_PRESET hidden)

    set(UE_ROOT "/home/letyga/UnrealEngine-4.27")
    set(UE_LIBCXX_INCLUDE "${UE_ROOT}/Engine/Source/ThirdParty/Linux/LibCxx/include/c++/v1")
    set(UE_LIBCXX_LIB "${UE_ROOT}/Engine/Source/ThirdParty/Linux/LibCxx/lib/Linux/x86_64-unknown-linux-gnu")

    set(BASE_LIBS_DIR "/home/letyga/LIBS/install")
    set(ITK_DIR "${BASE_LIBS_DIR}/itk-static/lib/cmake/ITK-5.4")
    set(DCMTK_DIR "${BASE_LIBS_DIR}/dcmtk-static/lib/cmake/dcmtk")
    set(OPENSSL_ROOT_DIR "${BASE_LIBS_DIR}/openssl")
    set(ZLIB_ROOT "${BASE_LIBS_DIR}/zlib")

    target_compile_options(${PROJECT_NAME} PRIVATE
        -stdlib=libc++
        -isystem "${UE_LIBCXX_INCLUDE}"
        -fPIC
    )

    target_link_options(${PROJECT_NAME} PRIVATE
        -stdlib=libc++
        -L"${UE_LIBCXX_LIB}"
        -Wl,-Bsymbolic
        -Wl,-Bsymbolic-functions
    )

    find_package(OpenSSL REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(DCMTK REQUIRED CONFIG)
    find_package(ITK REQUIRED CONFIG)

    target_link_libraries(${PROJECT_NAME} PRIVATE
        DCMTK::DCMTK
        ${ITK_LIBRARIES}
        OpenSSL::SSL
        OpenSSL::Crypto
        ZLIB::ZLIB
        c++
        c++abi
        pthread
        dl
        m
    )

    set_target_properties(${PROJECT_NAME} PROPERTIES
        BUILD_RPATH "$ORIGIN/ITKLibrary_Run"
        INSTALL_RPATH "$ORIGIN/ITKLibrary_Run"
    )

endif()

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output"
)
