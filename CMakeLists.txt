cmake_minimum_required(VERSION 3.13.4)

project(opt-chance 
     VERSION 1.0
     LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)

set(OPT_CHANCE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OPT_CHANCE_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR})

set(EXTERN_LLVM_SRC_DIR ON CACHE STRING "")

if (NOT EXISTS "${OPT_CHANCE_SOURCE_DIR}/llvm-project/llvm")
     if (NOT EXISTS "${EXTERN_LLVM_SRC_DIR}/llvm")
          message(FATAL_ERROR "The llvm submodule was not downloaded! Please use --recurse-submodules when cloning the project or use EXTERN_LLVM_SRC_DIR cmake variable.")
     endif()
endif()

if (EXISTS ${EXTERN_LLVM_SRC_DIR})
     include_directories(${EXTERN_LLVM_SRC_DIR}/llvm)
     add_subdirectory(${EXTERN_LLVM_SRC_DIR}/llvm llvm_build)
     message("Using ${EXTERN_LLVM_SRC_DIR}/llvm as LLVM")
else()
     add_subdirectory(llvm-project/llvm)
endif()

if (NOT LLVM_DIR)
    message(FATAL_ERROR "LLVM_DIR must be specified in order to find LLVMConfig")
endif()

find_package(LLVM REQUIRED CONFIG)

include_directories(${LLVM_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/llvm-project/llvm/include)
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})
add_definitions(${LLVM_DEFINITIONS_LIST})

set(LLVM_LINK_COMPONENTS
  DebugInfoDWARF
  AllTargetsDescs
  AllTargetsInfos
  MC
  Object
  Support
  TargetParser
  )

add_compile_options(-fno-rtti)

add_llvm_tool(opt-chance
  opt-chance.cpp
  )
