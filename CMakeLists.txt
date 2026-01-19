cmake_minimum_required(VERSION 3.20)

project(MEngine LANGUAGES CXX)

# Options
option(MYENGINE_BUILD_SHADERS "Compile HLSL shaders to .cso during build (requires dxc.exe)" ON)
option(MYENGINE_USE_WINPIX "Link WinPixEventRuntime if the NuGet package exists" ON)

# Output layout to match the existing VS project: bin/<platform>/<config>/
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set(_mengine_platform "${CMAKE_VS_PLATFORM_NAME}")
elseif(CMAKE_GENERATOR_PLATFORM)
  set(_mengine_platform "${CMAKE_GENERATOR_PLATFORM}")
else()
  set(_mengine_platform "${CMAKE_SYSTEM_PROCESSOR}")
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin/${_mengine_platform}")
foreach(_cfg Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${_cfg}" _cfg_u)
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfg_u} "${CMAKE_SOURCE_DIR}/bin/${_mengine_platform}/${_cfg}")
endforeach()

# Source files (mirrors src/MYEngine.vcxproj)
set(MENGINE_SOURCES
  ${CMAKE_SOURCE_DIR}/Common/MathHelper.cpp
  ${CMAKE_SOURCE_DIR}/src/D3D12QueueManager.cpp
  ${CMAKE_SOURCE_DIR}/src/DescriptorHeapManagement.cpp
  ${CMAKE_SOURCE_DIR}/src/Win32Application.cpp
  ${CMAKE_SOURCE_DIR}/src/D3D12DynamicIndexing.cpp
  ${CMAKE_SOURCE_DIR}/src/DXSample.cpp
  ${CMAKE_SOURCE_DIR}/src/FrameResource.cpp
  ${CMAKE_SOURCE_DIR}/src/Main.cpp
  ${CMAKE_SOURCE_DIR}/src/SimpleCamera.cpp
  ${CMAKE_SOURCE_DIR}/src/stdafx.cpp
)

set(MENGINE_HEADERS
  ${CMAKE_SOURCE_DIR}/Common/Direct3DUtils.h
  ${CMAKE_SOURCE_DIR}/Common/MathHelper.h
  ${CMAKE_SOURCE_DIR}/RHI/DX12RHI/DX12RHIResource.h
  ${CMAKE_SOURCE_DIR}/RHI/RHIResource.h
  ${CMAKE_SOURCE_DIR}/src/Assert.h
  ${CMAKE_SOURCE_DIR}/src/D3D12QueueManger.h
  ${CMAKE_SOURCE_DIR}/src/DescriptorHeapManagement.h
  ${CMAKE_SOURCE_DIR}/src/Win32Application.h
  ${CMAKE_SOURCE_DIR}/src/D3D12DynamicIndexing.h
  ${CMAKE_SOURCE_DIR}/src/d3dx12.h
  ${CMAKE_SOURCE_DIR}/src/DXSample.h
  ${CMAKE_SOURCE_DIR}/src/DXSampleHelper.h
  ${CMAKE_SOURCE_DIR}/src/FrameResource.h
  ${CMAKE_SOURCE_DIR}/src/occcity.h
  ${CMAKE_SOURCE_DIR}/src/SimpleCamera.h
  ${CMAKE_SOURCE_DIR}/src/StepTimer.h
  ${CMAKE_SOURCE_DIR}/src/stdafx.h
)

set(MENGINE_SHADERS
  ${CMAKE_SOURCE_DIR}/src/shader_mesh_dynamic_indexing_pixel.hlsl
  ${CMAKE_SOURCE_DIR}/src/shader_mesh_simple_vert.hlsl
)

# Keep shader sources visible in IDE, but do NOT let Visual Studio try to compile them via fxc.
# (We compile them ourselves with dxc in a POST_BUILD step below.)
set_source_files_properties(${MENGINE_SHADERS} PROPERTIES
  HEADER_FILE_ONLY ON
  VS_TOOL_OVERRIDE "None"
)
set_source_files_properties(${CMAKE_SOURCE_DIR}/src/occcity.bin PROPERTIES HEADER_FILE_ONLY ON)

# WIN32 => build as Windows subsystem app (entry point is WinMain)
add_executable(MEngine WIN32
  ${MENGINE_SOURCES}
  ${MENGINE_HEADERS}
  ${MENGINE_SHADERS}
  ${CMAKE_SOURCE_DIR}/src/occcity.bin
)

set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MEngine)

target_compile_features(MEngine PRIVATE cxx_std_17)

target_include_directories(MEngine PRIVATE
  ${CMAKE_SOURCE_DIR}/Common
  ${CMAKE_SOURCE_DIR}/RHI
  ${CMAKE_SOURCE_DIR}/RHI/DX12RHI
)

# Match the VS project settings
target_compile_definitions(MEngine PRIVATE
  WIN32
  _WINDOWS
  UNICODE
  _UNICODE
  $<$<CONFIG:Debug>:_DEBUG>
  $<$<CONFIG:Release>:NDEBUG>
)

if(MSVC)
  # Keep source encoding consistent (comments/strings)
  target_compile_options(MEngine PRIVATE /utf-8)

  # MSVC runtime to match /MDd (Debug) and /MD (Release)
  set_property(TARGET MEngine PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")

  # Precompiled header (stdafx.h / stdafx.cpp)
  # Use an absolute path so we don't need to add `src/` to include dirs (avoids assert.h vs Assert.h collisions on Windows).
  target_precompile_headers(MEngine PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_SOURCE_DIR}/src/stdafx.h>")
endif()

# DX12 dependencies
target_link_libraries(MEngine PRIVATE d3d12 dxgi dxguid)

# Delay-load d3d12.dll like the original project
if(MSVC)
  target_link_libraries(MEngine PRIVATE delayimp)
  target_link_options(MEngine PRIVATE "/DELAYLOAD:d3d12.dll")
endif()

# WinPixEventRuntime (NuGet package under src/packages)
if(MYENGINE_USE_WINPIX)
  set(_winpix_root "${CMAKE_SOURCE_DIR}/src/packages/WinPixEventRuntime.1.0.161208001")
  if(EXISTS "${_winpix_root}/Include/WinPixEventRuntime" AND EXISTS "${_winpix_root}/bin/WinPixEventRuntime.lib")
    target_include_directories(MEngine PRIVATE "${_winpix_root}/Include/WinPixEventRuntime")
    target_link_directories(MEngine PRIVATE "${_winpix_root}/bin")
    target_link_libraries(MEngine PRIVATE WinPixEventRuntime)

    # Copy DLL if it exists (some packages ship only .lib in source control)
    if(EXISTS "${_winpix_root}/bin/WinPixEventRuntime.dll")
      add_custom_command(TARGET MEngine POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${_winpix_root}/bin/WinPixEventRuntime.dll"
                "$<TARGET_FILE_DIR:MEngine>/WinPixEventRuntime.dll"
        VERBATIM)
    endif()
  else()
    message(STATUS "WinPixEventRuntime package not found (or incomplete). Skip linking WinPixEventRuntime.")
  endif()
endif()

# Make debugging/asset path behavior match the original helper (loads assets from exe directory)
set_target_properties(MEngine PROPERTIES
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:MEngine>"
)

# Copy binary asset required at runtime
add_custom_command(TARGET MEngine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          "${CMAKE_SOURCE_DIR}/src/occcity.bin"
          "$<TARGET_FILE_DIR:MEngine>/occcity.bin"
  VERBATIM)

# Compile shaders to the runtime directory (so GetAssetFullPath(...) can find them)
if(MSVC AND MYENGINE_BUILD_SHADERS)
  find_program(DXC_EXE NAMES dxc.exe)
  if(NOT DXC_EXE)
    message(FATAL_ERROR "MYENGINE_BUILD_SHADERS=ON but dxc.exe was not found. Install Windows 10/11 SDK / DXC and ensure dxc.exe is in PATH, or set MYENGINE_BUILD_SHADERS=OFF.")
  endif()

  add_custom_command(TARGET MEngine POST_BUILD
    COMMAND "${DXC_EXE}" -nologo
            -E VSMain
            -T $<IF:$<CONFIG:Debug>,vs_6_0,vs_5_0>
            -Fo "$<TARGET_FILE_DIR:MEngine>/shader_mesh_simple_vert.cso"
            "${CMAKE_SOURCE_DIR}/src/shader_mesh_simple_vert.hlsl"
    COMMAND "${DXC_EXE}" -nologo
            -E PSMain
            -T $<IF:$<CONFIG:Debug>,ps_6_0,ps_5_1>
            /enable_unbounded_descriptor_tables
            -Fo "$<TARGET_FILE_DIR:MEngine>/shader_mesh_dynamic_indexing_pixel.cso"
            "${CMAKE_SOURCE_DIR}/src/shader_mesh_dynamic_indexing_pixel.hlsl"
    VERBATIM)
endif()

# Nice source grouping in Visual Studio
source_group(TREE "${CMAKE_SOURCE_DIR}" FILES
  ${MENGINE_SOURCES}
  ${MENGINE_HEADERS}
  ${MENGINE_SHADERS}
  ${CMAKE_SOURCE_DIR}/src/occcity.bin
)
