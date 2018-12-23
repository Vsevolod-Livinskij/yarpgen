###############################################################################
#
# Copyright (c) 2018, Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
###############################################################################


set(LIB_SRCS type.cpp variable.cpp expr.cpp stmt.cpp gen_policy.cpp sym_table.cpp program.cpp options.cpp cxx_proto.pb.cc)

set(SRCS ${LIB_SRCS} main.cpp self-test.cpp)

find_package(LLVM REQUIRED CONFIG)

message(STATUS “Found LLVM ${LLVM_PACKAGE_VERSION}“)
message(STATUS “Using LLVMConfig.cmake in: ${LLVM_DIR}“)

include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

INCLUDE(FindProtobuf)
FIND_PACKAGE(Protobuf REQUIRED)
INCLUDE_DIRECTORIES(${PROTOBUF_INCLUDE_DIR})
PROTOBUF_GENERATE_CPP(PROTO_SRC PROTO_HEADER cxx_proto.proto)

file(GLOB PROTOBUF_FILELIST *.proto)
foreach(proto_file ${PROTOBUF_FILELIST} )
    get_filename_component(proto_name ${proto_file} NAME_WE)
    get_filename_component(proto_path . ABSOLUTE)
    set_source_files_properties("${proto_path}/${proto_name}.pb.cc"
            "${proto_path}/${proto_name}.pb.h"
            PROPERTIES GENERATED TRUE)
endforeach()

add_executable(yarpgen ${SRCS} ${PROTO_SRCS} ${PROTO_HDRS})

message(STATUS "DIR ${CMAKE_CURRENT_BINARY_DIR}")

target_compile_features(yarpgen PRIVATE cxx_std_14)
target_compile_definitions(yarpgen PRIVATE BUILD_VERSION="${GIT_HASH}" BUILD_DATE="${BUILD_DATE}")
target_compile_options(yarpgen PRIVATE
  #  $<$<CXX_COMPILER_ID:MSVC>:/WX>
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wall -Wpedantic -Werror
        -I${CMAKE_CURRENT_BINARY_DIR}>)
target_link_libraries(yarpgen ${Protobuf_LIBRARIES})

add_executable(yarpgen-fuzz ${LIB_SRCS} ${PROTO_SRCS} ${PROTO_HDRS} fuzz-target.cpp)
target_compile_features(yarpgen-fuzz PRIVATE cxx_std_14)
target_compile_definitions(yarpgen-fuzz PRIVATE BUILD_VERSION="${GIT_HASH}" BUILD_DATE="${BUILD_DATE}")
target_compile_options(yarpgen-fuzz PRIVATE
  #  $<$<CXX_COMPILER_ID:MSVC>:/WX>
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wall -Wpedantic -Werror -fsanitize=fuzzer -I${CMAKE_CURRENT_BINARY_DIR}  -I/home/vlivinsk/workspace/libprotobuf-mutator/src/libfuzzer/  -I/home/vlivinsk/workspace/libprotobuf-mutator/  -I/home/vlivinsk/workspace/libprotobuf-mutator/src>)
target_link_libraries(yarpgen-fuzz ${llvm_libs} ${Protobuf_LIBRARIES}
                      ~/workspace/libprotobuf-mutator/build/src/libprotobuf-mutator.a
                      ~/workspace/libprotobuf-mutator/build/src/libfuzzer/libprotobuf-mutator-libfuzzer.a)