# Install script for directory: /media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "RELWITHDEBINFO")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "1")
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set default install directory permissions.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "/usr/bin/objdump")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/pkgconfig" TYPE FILE FILES "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/libhs.pc")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/hs" TYPE FILE FILES
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/src/hs.h"
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/src/hs_common.h"
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/src/hs_compile.h"
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/src/hs_runtime.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs_runtime.so.5.4.2"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs_runtime.so.5"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/lib/libhs_runtime.so.5.4.2"
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/lib/libhs_runtime.so.5"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs_runtime.so.5.4.2"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs_runtime.so.5"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/lib/libhs_runtime.so")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs.so.5.4.2"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs.so.5"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      file(RPATH_CHECK
           FILE "${file}"
           RPATH "")
    endif()
  endforeach()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/lib/libhs.so.5.4.2"
    "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/lib/libhs.so.5"
    )
  foreach(file
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs.so.5.4.2"
      "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/libhs.so.5"
      )
    if(EXISTS "${file}" AND
       NOT IS_SYMLINK "${file}")
      if(CMAKE_INSTALL_DO_STRIP)
        execute_process(COMMAND "/usr/bin/strip" "${file}")
      endif()
    endif()
  endforeach()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE SHARED_LIBRARY FILES "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/lib/libhs.so")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  # Include the install script for each subdirectory.
  include("/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/util/cmake_install.cmake")
  include("/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/doc/dev-reference/cmake_install.cmake")
  include("/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/unit/cmake_install.cmake")
  include("/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/tools/cmake_install.cmake")
  include("/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/examples/cmake_install.cmake")

endif()

if(CMAKE_INSTALL_COMPONENT)
  set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
file(WRITE "/media/ducanh/Acer/Users/ADMIN/Desktop/coding/hpc-spi-classifier/third_party/hyperscan-5.4.2/build/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
