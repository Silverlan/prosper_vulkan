cmake_minimum_required(VERSION 3.16)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

#include(${CMAKE_CURRENT_LIST_DIR}/cotire.cmake)

set(INCLUDE_DIRS)
function(add_include_dir IDENTIFIER)
        set(PRETTYNAME ${IDENTIFIER})
        set(ARGV ${ARGN})
        list(LENGTH ARGV ARGC)
        if(${ARGC} GREATER 0)
                list(GET ARGV 0 PRETTYNAME)
        endif()
        string(TOUPPER ${IDENTIFIER} UIDENTIFIER)

        set(${DEFAULT_DIR} "")
        set(DEPENDENCY_${UIDENTIFIER}_INCLUDE ${DEFAULT_DIR} CACHE PATH "Path to ${PRETTYNAME} include directory.")
        set(INCLUDE_DIRS ${INCLUDE_DIRS} DEPENDENCY_${UIDENTIFIER}_INCLUDE PARENT_SCOPE)
endfunction(add_include_dir)

set(LIBRARIES)
function(add_external_library IDENTIFIER)
        set(PRETTYNAME ${IDENTIFIER})
        set(ARGV ${ARGN})
        list(LENGTH ARGV ARGC)
        if(${ARGC} GREATER 0)
                list(GET ARGV 0 PRETTYNAME)
        endif()
        string(TOUPPER ${IDENTIFIER} UIDENTIFIER)

        set(DEPENDENCY_${UIDENTIFIER}_LIBRARY "" CACHE FILEPATH "Path to ${PRETTYNAME} library.")
        set(LIBRARIES ${LIBRARIES} DEPENDENCY_${UIDENTIFIER}_LIBRARY PARENT_SCOPE)
endfunction(add_external_library)

function(link_external_library IDENTIFIER)
        set(PRETTYNAME ${IDENTIFIER})
        set(ARGV ${ARGN})
        list(LENGTH ARGV ARGC)
        if(${ARGC} GREATER 0)
                list(GET ARGV 0 PRETTYNAME)
        endif()
        string(TOUPPER ${IDENTIFIER} UIDENTIFIER)

        set(${DEFAULT_DIR} "")
        set(DEPENDENCY_${UIDENTIFIER}_INCLUDE ${DEFAULT_DIR} CACHE PATH "Path to ${PRETTYNAME} include directory.")
        set(INCLUDE_DIRS ${INCLUDE_DIRS} DEPENDENCY_${UIDENTIFIER}_INCLUDE PARENT_SCOPE)

        set(DEPENDENCY_${UIDENTIFIER}_LIBRARY "" CACHE FILEPATH "Path to ${PRETTYNAME} library.")
        set(LIBRARIES ${LIBRARIES} DEPENDENCY_${UIDENTIFIER}_LIBRARY PARENT_SCOPE)
endfunction(link_external_library)

set(DEPENDENCY_GLSLANG_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/anvil/deps/glslang CACHE STRING "" FORCE)
#set(VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE INTERNAL "Path to Vulkan SDK.")

if(WIN32)
	set(DEPENDENCY_VULKAN_INCLUDE "${VULKAN_SDK}/Include" CACHE PATH "Path to include directory." FORCE)
else()
	if("${VULKAN_SDK}" STREQUAL "")
	    find_package(Vulkan REQUIRED)
	    set(DEPENDENCY_VULKAN_LIBRARY "${Vulkan_LIBRARIES}" CACHE PATH "Path to vulkan library." FORCE)
	    set(DEPENDENCY_VULKAN_INCLUDE_PATH "${Vulkan_INCLUDE_DIR}" CACHE PATH "Path to include directory.")
	else()
	    set(DEPENDENCY_VULKAN_LIBRARY "${VULKAN_SDK}/lib/libvulkan.so" CACHE PATH "Path to vulkan library." FORCE)
	    set(DEPENDENCY_VULKAN_INCLUDE_PATH "${VULKAN_SDK}/include" CACHE PATH "Path to include directory." FORCE)
	endif()
endif()

set(DEPENDENCY_ANVIL_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/anvil/include CACHE PATH "Path to include directory.")
set(DEPENDENCY_ANVIL_BUILD_INCLUDE ${CMAKE_CURRENT_BINARY_DIR}/third_party_libs/anvil/include CACHE PATH "Path to include directory.")
set(DEPENDENCY_ANVIL_DEPS_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/anvil/deps CACHE PATH "Path to include directory.")

# Vulkan
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL ON FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL ".." FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL OFF FORCE)

if("${VULKAN_SDK}" STREQUAL "")

    #don't even bother trying to find tools

    if("${DEPENDENCY_SPIRV_TOOLS_DIR}" STREQUAL "")
        message(ERROR "DEPENDENCY_SPIRV_TOOLS_DIR is not set! pr_prosper_vulkan will NOT be built!")
        return()
    else()

        add_subdirectory("${DEPENDENCY_SPIRV_TOOLS_DIR}" third_party_libs/vulkan/spirv-tools)

    endif()
else()

    set(DEPENDENCY_SPIRV_TOOLS_DIR "${VULKAN_SDK}/spirv-tools" CACHE PATH "Path to spirv-tools repo")
    add_subdirectory(${DEPENDENCY_SPIRV_TOOLS_DIR} third_party_libs/vulkan/spirv-tools)
endif()

set(VULKAN_INCLUDE_DIR ${DEPENDENCY_VULKAN_INCLUDE} CACHE STRING "" FORCE)
set(VULKAN_LIBRARY ${DEPENDENCY_VULKAN_LIBRARY} CACHE STRING "" FORCE)

##### CONFIGURATION #####

set(PROJ_NAME prosper_vulkan)

project(${PROJ_NAME} CXX)

set(CMAKE_CXX_STANDARD 20)
set(ENABLE_STATIC_LIBRARY_FLAG 0)
set(PRECOMPILED_HEADER "stdafx_prosper_vulkan")

# PROSPER_VULKAN_ENABLE_LUNAR_GLASS
link_external_library(prosper)
link_external_library(sharedutils)
link_external_library(mathutil)
link_external_library(vfilesystem)
link_external_library(vulkan)
link_external_library(anvil)
link_external_library(util_image)
link_external_library(glfw)
link_external_library(iglfw)
link_external_library(glslang)
# link_external_library(lunarglass)
# link_external_library(util_lunarglass)

add_include_dir(glm)
add_include_dir(gli)
add_include_dir(anvil_build)
add_include_dir(anvil_deps)
# add_include_dir(lunarglass_core)

add_external_library(SPIRV)

add_external_library(GLSLang_Machine_Independent)
add_external_library(GLSLang_OS_Dependent)

add_external_library(GLSLang_Generic_Code_Gen)
add_external_library(GLSLang_OGL_Compiler)
add_external_library(SPIRV_Tools_Opt)
add_external_library(SPV_Remapper)
add_external_library(SPIRV_Tools)
# add_external_library(HLSL)

set(DEFINITIONS
        SHPROSPER_VULKAN_DLL
        GLM_FORCE_DEPTH_ZERO_TO_ONE
        ANVIL_VULKAN_CPP
        UIMG_ENABLE_NVTT
)

set(DEPENDENCY_GLSLANG_INCLUDE ${CMAKE_CURRENT_LIST_DIR}/third_party_libs/anvil/deps/glslang CACHE STRING "" FORCE)
search_lib_recursive(DEPENDENCY_GLSLANG_LIBRARY third_party_libs/anvil/deps/glslang/glslang glslang STATIC)

set(VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE INTERNAL "Path to Vulkan SDK.")

if(WIN32)
        set(DEPENDENCY_VULKAN_INCLUDE "${VULKAN_SDK}/Include" CACHE PATH "Path to include directory." FORCE)
	search_lib_recursive(DEPENDENCY_SPIRV_TOOLS_LIBRARY third_party_libs/vulkan/spirv-tools/source/${BINARY_PRAGMA_DIR} SPIRV-Tools)
	search_lib_recursive(DEPENDENCY_SPIRV_TOOLS_OPT_LIBRARY third_party_libs/vulkan/spirv-tools/source/opt/${BINARY_PRAGMA_DIR} SPIRV-Tools-opt)
	search_lib_recursive(DEPENDENCY_GLSLANG_LIBRARY third_party_libs/anvil/deps/glslang/glslang/${BINARY_PRAGMA_DIR} glslang)
	search_lib_recursive(DEPENDENCY_GLSLANG_OGL_COMPILER_LIBRARY third_party_libs/anvil/deps/glslang/OGLCompilersDLL/${BINARY_PRAGMA_DIR} OGLCompiler)
	search_lib_recursive(DEPENDENCY_GLSLANG_MACHINE_INDEPENDENT_LIBRARY third_party_libs/anvil/deps/glslang/glslang/${BINARY_PRAGMA_DIR} MachineIndependent)
	search_lib_recursive(DEPENDENCY_GLSLANG_GENERIC_CODE_GEN_LIBRARY third_party_libs/anvil/deps/glslang/glslang/${BINARY_PRAGMA_DIR} GenericCodeGen)
	search_lib_recursive(DEPENDENCY_SPIRV_LIBRARY third_party_libs/anvil/deps/glslang/SPIRV/${BINARY_PRAGMA_DIR} SPIRV)
	search_lib_recursive(DEPENDENCY_GLSLANG_OS_DEPENDENT_LIBRARY third_party_libs/anvil/deps/glslang/glslang/OSDependent/Windows/${BINARY_PRAGMA_DIR} OSDependent)
	search_lib_recursive(DEPENDENCY_SPV_REMAPPER_LIBRARY third_party_libs/anvil/deps/glslang/SPIRV/${BINARY_PRAGMA_DIR} SPVRemapper)
	# search_lib_recursive(DEPENDENCY_HLSL_LIBRARY "${VULKAN_SDK}/glslang/${BINARY_PRAGMA_DIR}/hlsl/${BINARY_PRAGMA_DIR}/HLSL")

	search_lib_recursive_full(DEPENDENCY_VULKAN_LIBRARY "${VULKAN_SDK}/Lib/vulkan-1")
else()

    if(NOT "${VULKAN_SDK}" STREQUAL "")
        set(DEPENDENCY_VULKAN_INCLUDE "${VULKAN_SDK}/include" CACHE PATH "Path to include directory." FORCE) #linux or macs are case-sensitive.
    else()
        set(DEPENDENCY_VULKAN_INCLUDE "/usr/include" CACHE PATH "Path to include directory." FORCE)
    endif()
        #we do not know where the libraries live. They may be in /usr/lib, or /usr/lib/{gcc-arch-id},or /usr/local/lib (Or God knows where really) or at VULKAN_SDK. Hint the last one to the library searcher.
        #set(ENV{PKG_CONFIG_PATH} "${VULKAN_SDK}/lib/pkgconfig")
        #set(TMP_VAR "")
        #find_library(TMP_VAR NAMES vulkan HINTS "${VULKAN_SDK}/lib" NO_CACHE)
        #set(DEPENDENCY_VULKAN_LIBRARY "${TMP_VAR}" CACHE PATH "" FORCE)
        #unset(TMP_VAR)
	# search_lib_recursive_full(DEPENDENCY_GLSLANG_LIBRARY )
	# search_lib_recursive_full(DEPENDENCY_GLSLANG_OGL_COMPILER_LIBRARY )
	# search_lib_recursive_full(DEPENDENCY_GLSLANG_OS_DEPENDENT_LIBRARY )
	# search_lib_recursive_full(DEPENDENCY_HLSL_LIBRARY )




        search_lib_recursive(DEPENDENCY_SPIRV_TOOLS_LIBRARY third_party_libs/vulkan/spirv-tools/source/${BINARY_PRAGMA_DIR} SPIRV-Tools STATIC)
        search_lib_recursive(DEPENDENCY_SPIRV_TOOLS_OPT_LIBRARY third_party_libs/vulkan/spirv-tools/source/opt/${BINARY_PRAGMA_DIR} SPIRV-Tools-opt STATIC)
        search_lib_recursive(DEPENDENCY_GLSLANG_LIBRARY third_party_libs/anvil/deps/glslang/glslang/${BINARY_PRAGMA_DIR} glslang STATIC)
        search_lib_recursive(DEPENDENCY_GLSLANG_OGL_COMPILER_LIBRARY third_party_libs/anvil/deps/glslang/OGLCompilersDLL/${BINARY_PRAGMA_DIR} OGLCompiler STATIC)
        search_lib_recursive(DEPENDENCY_GLSLANG_MACHINE_INDEPENDENT_LIBRARY third_party_libs/anvil/deps/glslang/glslang/${BINARY_PRAGMA_DIR} MachineIndependent STATIC)
        search_lib_recursive(DEPENDENCY_GLSLANG_GENERIC_CODE_GEN_LIBRARY third_party_libs/anvil/deps/glslang/glslang/${BINARY_PRAGMA_DIR} GenericCodeGen STATIC)
        search_lib_recursive(DEPENDENCY_SPIRV_LIBRARY third_party_libs/anvil/deps/glslang/SPIRV/${BINARY_PRAGMA_DIR} SPIRV STATIC)
        search_lib_recursive(DEPENDENCY_GLSLANG_OS_DEPENDENT_LIBRARY third_party_libs/anvil/deps/glslang/glslang/OSDependent/Unix/${BINARY_PRAGMA_DIR} OSDependent STATIC)
        search_lib_recursive(DEPENDENCY_SPV_REMAPPER_LIBRARY third_party_libs/anvil/deps/glslang/SPIRV/${BINARY_PRAGMA_DIR} SPVRemapper STATIC)
endif()

search_lib_recursive(DEPENDENCY_ANVIL_LIBRARY third_party_libs/anvil/${BINARY_PRAGMA_DIR} Anvil STATIC)

##### CONFIGURATION #####

set(LIB_TYPE SHARED)
if(ENABLE_STATIC_LIBRARY_FLAG)
        option(CONFIG_STATIC_LIBRARY "Build as static library?" OFF)
endif()
if(DEFINED ${CONFIG_STATIC_LIBRARY} AND ${CONFIG_STATIC_LIBRARY})
        set(LIB_TYPE STATIC)
endif()

foreach(def IN LISTS DEFINITIONS)
        add_definitions(-D${def})
endforeach(def)

function(def_vs_filters FILE_LIST)
        foreach(source IN LISTS FILE_LIST)
            get_filename_component(source_path "${source}" PATH)
            string(REPLACE "${CMAKE_CURRENT_LIST_DIR}" "" source_path_relative "${source_path}")
            string(REPLACE "/" "\\" source_path_msvc "${source_path_relative}")
            source_group("${source_path_msvc}" FILES "${source}")
        endforeach()
endfunction(def_vs_filters)

file(GLOB_RECURSE SRC_FILES
    "${CMAKE_CURRENT_LIST_DIR}/src/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.hpp"
    "${CMAKE_CURRENT_LIST_DIR}/src/*.cpp"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.h"
    "${CMAKE_CURRENT_LIST_DIR}/include/*.hpp"
)
add_library(${PROJ_NAME} ${LIB_TYPE} ${SRC_FILES})
if(WIN32)
        target_compile_options(${PROJ_NAME} PRIVATE /wd4251)
        target_compile_options(${PROJ_NAME} PRIVATE /wd4996)
else()
        target_compile_options(${PROJ_NAME} PRIVATE -Wno-comment)
endif()
def_vs_filters("${SRC_FILES}")

foreach(LIB IN LISTS LIBRARIES)
        target_link_libraries(${PROJ_NAME} ${${LIB}})
endforeach(LIB)

if(UNIX) # Required to create XCB Anvil compatibility
        target_link_libraries(${PROJ_NAME} xcb X11-xcb)
endif()

target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/include)
target_include_directories(${PROJ_NAME} PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

foreach(INCLUDE_PATH IN LISTS INCLUDE_DIRS)
        target_include_directories(${PROJ_NAME} PRIVATE ${${INCLUDE_PATH}})
endforeach(INCLUDE_PATH)

set(TARGET_PROPERTIES LINKER_LANGUAGE CXX)

#if(DEFINED PRECOMPILED_HEADER)
#	set_target_properties(${PROJ_NAME} PROPERTIES COTIRE_CXX_PREFIX_HEADER_INIT "src/${PRECOMPILED_HEADER}.h")
#endif()
#cotire(${PROJ_NAME})
target_precompile_headers(${PROJ_NAME} PRIVATE "src/${PRECOMPILED_HEADER}.h")
set_target_properties(${PROJ_NAME} PROPERTIES ${TARGET_PROPERTIES})

set(ANVIL_LINK_STATICALLY_WITH_VULKAN_LIB ON CACHE BOOL ON FORCE)
set(ANVIL_USE_BUILT_IN_VULKAN_HEADERS OFF CACHE BOOL OFF FORCE)
set(ANVIL_USE_BUILT_IN_GLSLANG ON CACHE BOOL ON FORCE)
set(ENABLE_HLSL ON CACHE BOOL ".." FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL ON FORCE)
set(VULKAN_LIBRARY ${DEPENDENCY_VULKAN_LIBRARY} CACHE PATH "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL OFF FORCE)
add_subdirectory("third_party_libs/anvil")
set(BUILD_SHARED_LIBS ON CACHE BOOL ON FORCE)

if(UNIX)
        target_compile_options(Anvil PRIVATE -Wno-comment -Wno-unused-variable)
endif()

# add_subdirectory(external_libs/util_lunarglass)
add_dependencies(${PROJ_NAME} prosper iclient Anvil SPIRV SPIRV-Tools OSDependent OGLCompiler SPIRV-Tools-opt SPVRemapper glslang)
# util_lunarglass