###############################################################################
# The MIT Licence                                                             #
#                                                                             #
# Copyright (c) 2021 Airbus Operations S.A.S                                  #
#                                                                             #
# Permission is hereby granted, free of charge, to any person obtaining a     #
# copy of this software and associated documentation files (the "Software"),  #
# to deal in the Software without restriction, including without limitation   #
# the rights to use, copy, modify, merge, publish, distribute, sublicense,    #
# and/or sell copies of the Software, and to permit persons to whom the       #
# Software is furnished to do so, subject to the following conditions:        #
#                                                                             #
# The above copyright notice and this permission notice shall be included     #
# in all copies or substantial portions of the Software.                      #
#                                                                             #
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  #
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    #
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE #
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      #
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     #
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         #
# DEALINGS IN THE SOFTWARE.                                                   #
###############################################################################
# Special object target that will also be used by unitary tests
add_library(ed247_objects OBJECT
    ed247_logs.cpp
    ed247_time.cpp
    ed247_conversion.cpp
    ed247_xml_parser.cpp
    ed247_xml.cpp
    ed247_cominterface.cpp
    ed247_sample.cpp
    ed247_signal.cpp
    ed247_stream_assistant.cpp
    ed247_stream.cpp
    ed247_frame_header.cpp
    ed247_channel.cpp
    ed247_context.cpp
    ed247.cpp
)

target_link_libraries(ed247_objects
  PUBLIC
     EXPAT::EXPAT
     $<$<PLATFORM_ID:Windows>:wsock32>
     $<$<PLATFORM_ID:Windows>:ws2_32>
     $<$<PLATFORM_ID:QNX>:socket>
     $<$<PLATFORM_ID:QNX>:regex>
     $<$<BOOL:${SimulinkLogger_FOUND}>:SimulinkLogger>
)

target_include_directories(ed247_objects
  PUBLIC
     $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
     $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
     $<INSTALL_INTERFACE:include>
)

target_compile_definitions(ed247_objects
    PRIVATE
        "_PRODUCT_VERSION=${VERSION}"
        "_PRODUCT_NAME=${CMAKE_PROJECT_NAME}"
        $<$<PLATFORM_ID:Windows>:LIBED247_EXPORTS>
)

# ED247 dynamic library definition
add_library(ed247 SHARED)
add_library(Ed247::ed247 ALIAS ed247)
target_link_libraries(ed247 PUBLIC ed247_objects)
set_target_properties(ed247 PROPERTIES PUBLIC_HEADER ed247.h)

# ED247 static library definition
add_library(ed247_static STATIC)
add_library(Ed247::static ALIAS ed247_static)
set_target_properties(ed247_static PROPERTIES OUTPUT_NAME ed247)
target_link_libraries(ed247_static PUBLIC ed247_objects)
set_target_properties(ed247_static PROPERTIES PUBLIC_HEADER ed247.h)


# Note: the XSD schemas are not embedded in the library anymore.
# They used to be compiled into the library (ed247_xsd.h) to validate the ECIC files with
# libXML2. As expat does not support the XSD validation, the schemas are only installed
# as a reference for the users of the library.

# Install required stuff
set(INSTALL_LIBDIR lib)
if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  set(INSTALL_LIBDIR bin)
endif()

install(TARGETS ed247 ed247_static
                                     DESTINATION ${PLATFORM_INSTALL_SUBDIR}${INSTALL_LIBDIR}
  PUBLIC_HEADER                      DESTINATION ${PLATFORM_INSTALL_SUBDIR}include
)
install(FILES ./xsd/ED247A_ECIC.xsd  DESTINATION doc/xsd)
install(FILES ./xsd/ED247A_ECD.xsd   DESTINATION doc/xsd)
