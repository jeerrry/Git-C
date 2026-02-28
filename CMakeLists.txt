cmake_minimum_required(VERSION 3.13)

project(codecrafters-git)

file(GLOB_RECURSE SOURCE_FILES src/*.c src/*.h)

set(CMAKE_C_STANDARD 23) # Enable the C23 standard

add_executable(git ${SOURCE_FILES})

if(WIN32)
    # Configure zlib paths for Windows
    set(ZLIB_ROOT "C:/msys64/ucrt64")
    set(ZLIB_INCLUDE_DIR "${ZLIB_ROOT}/include")
    set(ZLIB_LIBRARY "${ZLIB_ROOT}/lib/libz.a")

    include_directories(${ZLIB_INCLUDE_DIR})
    target_link_libraries(git ${ZLIB_LIBRARY})

    # Add OpenSSL paths
    set(OPENSSL_ROOT "C:/msys64/mingw64") # Adjust to your OpenSSL installation path
    set(OPENSSL_LIBRARY_CRYPTO "${OPENSSL_ROOT}/lib/libcrypto.a")
    set(OPENSSL_LIBRARY_SSL "${OPENSSL_ROOT}/lib/libssl.a")
    include_directories("${OPENSSL_ROOT}/include")
    link_directories("${OPENSSL_ROOT}/lib")

    # Link OpenSSL libraries and additional system libraries
    target_link_libraries(git
            ${OPENSSL_LIBRARY_SSL}
            ${OPENSSL_LIBRARY_CRYPTO}
            ws2_32
            crypt32
    )
else()
    # Use find_package for other platforms
    find_package(ZLIB REQUIRED)
    target_link_libraries(git ZLIB::ZLIB)

    find_package(OpenSSL REQUIRED)
    target_link_libraries(git OpenSSL::Crypto) # Link OpenSSL on non-Windows systems
endif()