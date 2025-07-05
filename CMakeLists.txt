include(${CMAKE_SOURCE_DIR}/cmake/pr_common.cmake)

set(VULKAN_SDK "$ENV{VULKAN_SDK}" CACHE INTERNAL "Path to Vulkan SDK.")

set(PROJ_NAME prosper_vulkan)
pr_add_library(${PROJ_NAME} SHARED)

pr_add_dependency(${PROJ_NAME} prosper TARGET PUBLIC)

pr_add_headers(${PROJ_NAME} "include/")
pr_add_sources(${PROJ_NAME} "src/")

if(UNIX) # Required to create XCB Anvil compatibility
	target_link_libraries(${PROJ_NAME} PRIVATE xcb X11-xcb)
        target_compile_options(${PROJ_NAME} PRIVATE -Wno-comment)
endif()

pr_add_compile_definitions(
	${PROJ_NAME}
		-DSHPROSPER_VULKAN_DLL
)

find_package(vulkan REQUIRED)

# Vulkan
set(SPIRV_SKIP_EXECUTABLES ON CACHE BOOL "" FORCE)
set(SPIRV_SKIP_TESTS ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(SPIRV-Headers_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party_libs/SPIRV-Headers")
add_subdirectory("third_party_libs/SPIRV-Tools")

pr_set_target_folder(spirv-tools-pkg-config modules/rendering/vulkan)
pr_set_target_folder(spirv-tools-shared-pkg-config modules/rendering/vulkan)

pr_add_dependency(${PROJ_NAME} SPIRV TARGET)
pr_add_dependency(${PROJ_NAME} SPIRV-Tools TARGET)
pr_add_dependency(${PROJ_NAME} OSDependent TARGET)
pr_add_dependency(${PROJ_NAME} SPIRV-Tools-opt TARGET)
pr_add_dependency(${PROJ_NAME} OGLCompiler TARGET)
pr_add_dependency(${PROJ_NAME} SPVRemapper TARGET)
pr_add_dependency(${PROJ_NAME} glslang TARGET)

# Anvil
set(ANVIL_LINK_STATICALLY_WITH_VULKAN_LIB ON CACHE BOOL "" FORCE)
set(ANVIL_USE_BUILT_IN_VULKAN_HEADERS OFF CACHE BOOL "" FORCE)
set(ANVIL_USE_BUILT_IN_GLSLANG ON CACHE BOOL "" FORCE)
set(ENABLE_HLSL ON CACHE BOOL "" FORCE)
set(SKIP_GLSLANG_INSTALL ON CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory("third_party_libs/anvil")
set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

pr_add_compile_definitions(Anvil -DANVIL_VULKAN_CPP PUBLIC)
if(UNIX)
	target_compile_options(Anvil PRIVATE -Wno-comment -Wno-unused-variable)
endif()

pr_add_dependency(${PROJ_NAME} Anvil TARGET PUBLIC)

pr_finalize(${PROJ_NAME})
