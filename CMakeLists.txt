cmake_minimum_required(VERSION 2.8)

# Set the plugin name to build
project(dnp3)

# Supported options:
# -DFOGLAMP_INCLUDE
# -DFOGLAMP_LIB
# -DFOGLAMP_SRC
# -DFOGLAMP_INSTALL
#
# If no -D options are given and FOGLAMP_ROOT environment variable is set
# then FogLAMP libraries and header files are pulled from FOGLAMP_ROOT path.

set(CMAKE_CXX_FLAGS "-std=c++11 -O3")

# Generation version header file
set_source_files_properties(version.h PROPERTIES GENERATED TRUE)
add_custom_command(
  OUTPUT version.h
  DEPENDS ${CMAKE_SOURCE_DIR}/VERSION
  COMMAND ${CMAKE_SOURCE_DIR}/mkversion ${CMAKE_SOURCE_DIR}
  COMMENT "Generating version header"
  VERBATIM
)
include_directories(${CMAKE_BINARY_DIR})

# Set plugin type (south, north, filter)
set(PLUGIN_TYPE "south")

# Add here all needed FogLAMP libraries as list
set(NEEDED_FOGLAMP_LIBS common-lib)

# Find source files
file(GLOB SOURCES *.cpp)

# Find FogLAMP includes and libs, by including FindFogLAMP.cmak file
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_CURRENT_SOURCE_DIR})
find_package(FogLAMP)
# If errors: make clean and remove Makefile
if (NOT FOGLAMP_FOUND)
	if (EXISTS "${CMAKE_BINARY_DIR}/Makefile")
		execute_process(COMMAND make clean WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
		file(REMOVE "${CMAKE_BINARY_DIR}/Makefile")
	endif()
	# Stop the build process
	message(FATAL_ERROR "FogLAMP plugin '${PROJECT_NAME}' build error.")
endif()
# On success, FOGLAMP_INCLUDE_DIRS and FOGLAMP_LIB_DIRS variables are set 

# Find the opendnp3 files
if (NOT "$ENV{OPENDNP3_LIB_DIR}" STREQUAL "")
	set(OPENDNP3_LIB_DIR $ENV{OPENDNP3_LIB_DIR})
else()
	set(OPENDNP3_LIB_DIR "$ENV{HOME}/opendnp3")
endif()

# Add ./include
include_directories(include)
# Add FogLAMP include dir(s)
include_directories(${FOGLAMP_INCLUDE_DIRS})

# Add other include paths
# We assume the 'freeopcua' header files are available here: 
if (NOT EXISTS "${OPENDNP3_LIB_DIR}/cpp/libs/include")
	message(FATAL_ERROR "OPENDNP3_LIB_DIR ${OPENDNP3_LIB_DIR}/cpp/libs/include \n"
		"does not appear to be pointing at a valid OPENDNP3_LIB_DIR source tree")
	return()
endif()
include_directories(${OPENDNP3_LIB_DIR}/cpp/libs/include)

# Add FogLAMP lib path
link_directories(${FOGLAMP_LIB_DIRS})

# Create shared library
add_library(${PROJECT_NAME} SHARED ${SOURCES} version.h)

# Add FogLAMP library names
target_link_libraries(${PROJECT_NAME} ${NEEDED_FOGLAMP_LIBS})

# Add the static libopendnp3
find_library(LIBOPENDP3 libopendnp3.a "${OPENDNP3_LIB_DIR}/build")
if (NOT LIBOPENDP3)
	message(FATAL_ERROR "The opendnp3 library libopendnp3 was not found in ${OPENDNP3_LIB_DIR}/build\n"
			"Please build opendnp3 and set the environment variable OPENDNP3_LIB_DIR to root of opendnp3 source tree")
	return()
endif()
# Add libopendnp3 and other static libraries
target_link_libraries(${PROJECT_NAME} -L${OPENDNP3_LIB_DIR}/build -lasiodnp3 -lopendnp3 -lasiopal -lopenpal)

# Add additional libraries
target_link_libraries(${PROJECT_NAME} -lpthread -ldl)

# Set the build version 
set_target_properties(${PROJECT_NAME} PROPERTIES SOVERSION 1)

set(FOGLAMP_INSTALL "" CACHE INTERNAL "")
# Install library
if (FOGLAMP_INSTALL)
	message(STATUS "Installing ${PROJECT_NAME} in ${FOGLAMP_INSTALL}/plugins/${PLUGIN_TYPE}/${PROJECT_NAME}")
	install(TARGETS ${PROJECT_NAME} DESTINATION ${FOGLAMP_INSTALL}/plugins/${PLUGIN_TYPE}/${PROJECT_NAME})
endif()
