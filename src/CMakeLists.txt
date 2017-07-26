# This is the root ITK CMakeLists file.
cmake_minimum_required(VERSION 2.8.9)
if(COMMAND CMAKE_POLICY)
  cmake_policy(SET CMP0003 NEW)
endif()


# This project is designed to be built outside the Insight source tree.
project(DTIatlas)

# Find ITK.
# this needs to be set: export ITK DIR=/home/hbartsch/src/ITK-build/
find_package(ITK REQUIRED)
include(${ITK_USE_FILE})

add_executable(motion_detection 
    motion_detection.cxx
    utils/ReadDICOMDirectory.h
    utils/ReadDICOMDirectory.cxx
)

target_link_libraries(motion_detection ${ITK_LIBRARIES})
