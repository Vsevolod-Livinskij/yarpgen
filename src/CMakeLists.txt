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

include(ExternalProject)
include(FetchContent)

set(LibProtobufMutator_ROOT ${CMAKE_CURRENT_BINARY_DIR}/libprotobuf-mutator)

FetchContent_Declare(LibProtobufMutator
        GIT_REPOSITORY https://github.com/Vsevolod-Livinskij/libprotobuf-mutator
        )

ExternalProject_Add(LibProtobufMutator
        GIT_REPOSITORY https://github.com/Vsevolod-Livinskij/libprotobuf-mutator
        PREFIX ${LibProtobufMutator_ROOT}
        CMAKE_ARGS -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_BUILD_TYPE=Release -DLIB_PROTO_MUTATOR_DOWNLOAD_PROTOBUF=ON -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR> -DPKG_CONFIG_PATH=<INSTALL_DIR>
        )

FetchContent_GetProperties(LibProtobufMutator)
if(NOT LibProtobufMutator_POPULATED)
    FetchContent_Populate(LibProtobufMutator)
    include(${libprotobufmutator_SOURCE_DIR}/cmake/external/protobuf.cmake)
endif()

find_package(LLVM REQUIRED CONFIG)

message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

set(CMAKE_PREFIX_PATH ${PROTOBUF_INSTALL_DIR})
INCLUDE(FindProtobuf)
add_definitions(-DGOOGLE_PROTOBUF_NO_RTTI)
FIND_PACKAGE(Protobuf REQUIRED)

set(Protobuf_LIBRARY ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.a)
set(Protobuf_PROTOC_LIBRARY ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.a)
set(Protobuf_INCLUDE_DIR ${PROTOBUF_INSTALL_DIR}/include/google/protobuf)
set(Protobuf_PROTOC_EXECUTABLE ${PROTOBUF_INSTALL_DIR}/bin/protoc)
set(Protobuf_LIBRARY_DEBUG ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf.a)
set(Protobuf_PROTOC_LIBRARY_DEBUG ${PROTOBUF_INSTALL_DIR}/lib/libprotoc.a)
set(Protobuf_LITE_LIBRARY ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf-lite.a)
set(Protobuf_LITE_LIBRARY_DEBUG ${PROTOBUF_INSTALL_DIR}/lib/libprotobuf-lite.a)


include_directories(${LLVM_INCLUDE_DIRS})
add_definitions(${LLVM_DEFINITIONS})

llvm_map_components_to_libnames(llvm_libs support core irreader)

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
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wall -Wpedantic
        -I${CMAKE_CURRENT_BINARY_DIR}>)
target_link_libraries(yarpgen ${Protobuf_LIBRARIES})


add_executable(yarpgen-fuzz ${LIB_SRCS} ${PROTO_SRCS} ${PROTO_HDRS} fuzz-target.cpp)
add_dependencies(yarpgen-fuzz LibProtobufMutator)
target_compile_features(yarpgen-fuzz PRIVATE cxx_std_14)
target_compile_definitions(yarpgen-fuzz PRIVATE BUILD_VERSION="${GIT_HASH}" BUILD_DATE="${BUILD_DATE}")
target_compile_options(yarpgen-fuzz PRIVATE
  #  $<$<CXX_COMPILER_ID:MSVC>:/WX>
  $<$<OR:$<CXX_COMPILER_ID:GNU>,$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_ID:AppleClang>>:-Wall -Wpedantic -fno-rtti -fsanitize=fuzzer -I${CMAKE_CURRENT_BINARY_DIR}>)

ExternalProject_Get_property(LibProtobufMutator INSTALL_DIR)
include_directories(${INSTALL_DIR}/include/libprotobuf-mutator/src/libfuzzer
                    ${INSTALL_DIR}/include/libprotobuf-mutator/src
                    ${INSTALL_DIR}/include/libprotobuf-mutator)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=gold")

target_link_libraries(yarpgen-fuzz ${INSTALL_DIR}/lib/libprotobuf-mutator-libfuzzer.a
                                   ${INSTALL_DIR}/lib/libprotobuf-mutator.a
                                   ${Protobuf_LIBRARIES})
target_link_libraries(yarpgen-fuzz ${llvm_libs}
        -fsanitize=fuzzer
        -Wl,--start-group
        -lclangAST
        -lclangASTMatchers
        -lclangAnalysis
        -lclangBasic
        -lclangCodeGen
        -lclangDriver
        -lclangEdit
        -lclangFrontend
        -lclangFrontendTool
        -lclangLex
        -lclangParse
        -lclangSema
        -lclangEdit
        -lclangRewrite
        -lclangStaticAnalyzerFrontend
        -lclangStaticAnalyzerCheckers
        -lclangStaticAnalyzerCore
        -lclangCrossTU
        -lclangIndex
        -lclangSerialization
        -lclangToolingCore
        -lclangTooling
        -lclangFormat
        -lLLVMLTO
        -lLLVMPasses
        -lLLVMObjCARCOpts
        -lLLVMSymbolize
        -lLLVMDebugInfoPDB
        -lLLVMDebugInfoDWARF
        -lLLVMFuzzMutate
        -lLLVMTableGen
        -lLLVMDlltoolDriver
        -lLLVMLineEditor
        -lLLVMOrcJIT
        -lLLVMCoverage
        -lLLVMMIRParser
        -lLLVMObjectYAML
        -lLLVMLibDriver
        -lLLVMOption
        -lLLVMWindowsManifest
        -lLLVMX86Disassembler
        -lLLVMX86AsmParser
        -lLLVMX86CodeGen
        -lLLVMGlobalISel
        -lLLVMSelectionDAG
        -lLLVMAsmPrinter
        -lLLVMX86Desc
        -lLLVMMCDisassembler
        -lLLVMX86Info
        -lLLVMX86AsmPrinter
        -lLLVMX86Utils
        -lLLVMMCJIT
        -lLLVMInterpreter
        -lLLVMExecutionEngine
        -lLLVMRuntimeDyld
        -lLLVMCodeGen
        -lLLVMTarget
        -lLLVMCoroutines
        -lLLVMipo
        -lLLVMInstrumentation
        -lLLVMVectorize
        -lLLVMScalarOpts
        -lLLVMLinker
        -lLLVMIRReader
        -lLLVMAsmParser
        -lLLVMInstCombine
        -lLLVMBitWriter
        -lLLVMAggressiveInstCombine
        -lLLVMTransformUtils
        -lLLVMAnalysis
        -lLLVMProfileData
        -lLLVMObject
        -lLLVMMCParser
        -lLLVMMC
        -lLLVMDebugInfoCodeView
        -lLLVMDebugInfoMSF
        -lLLVMBitReader
        -lLLVMCore
        -lLLVMBinaryFormat
        -lLLVMSupport
        -lLLVMDemangle
        #-lasan
        -Wl,--end-group
        )
