# Assimp, built from the source tree in ThirdParty/assimp (no prebuilt binaries),
# so its code can be stepped into from the MEngine debugger.
# Built as a static library: nothing to deploy, and its symbols end up in MEngine.pdb.

set(MENGINE_ASSIMP_ROOT "${CMAKE_SOURCE_DIR}/ThirdParty/assimp")

# Only the formats the engine actually loads are compiled; add entries here (e.g. OBJ;GLTF) when needed.
set(MENGINE_ASSIMP_IMPORTERS "FBX" CACHE STRING "Assimp importers to build from source (;-separated, e.g. FBX;OBJ;GLTF)")

if(NOT EXISTS "${MENGINE_ASSIMP_ROOT}/CMakeLists.txt")
  message(FATAL_ERROR
    "Assimp source not found under ThirdParty/assimp (expected ThirdParty/assimp/CMakeLists.txt). "
    "Put the Assimp source tree there, or configure with -DMYENGINE_ENABLE_FBX=OFF.")
endif()

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ZLIB ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_VIEW OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_SAMPLES OFF CACHE BOOL "" FORCE)
set(ASSIMP_INSTALL OFF CACHE BOOL "" FORCE)
set(ASSIMP_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
set(ASSIMP_IGNORE_GIT_HASH ON CACHE BOOL "" FORCE)
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)

set(ASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT OFF CACHE BOOL "" FORCE)
foreach(_importer IN LISTS MENGINE_ASSIMP_IMPORTERS)
  set(ASSIMP_BUILD_${_importer}_IMPORTER ON CACHE BOOL "" FORCE)
endforeach()

# EXCLUDE_FROM_ALL: only the targets MEngine links against get built.
add_subdirectory("${MENGINE_ASSIMP_ROOT}" "${CMAKE_BINARY_DIR}/ThirdParty/assimp" EXCLUDE_FROM_ALL)

# Group every Assimp target (assimp, zlib, ...) under a "ThirdParty" folder in Visual Studio.
function(mengine_set_folder_recursive Dir Folder)
  get_property(_targets DIRECTORY "${Dir}" PROPERTY BUILDSYSTEM_TARGETS)
  foreach(_target IN LISTS _targets)
    set_property(TARGET ${_target} PROPERTY FOLDER "${Folder}")
  endforeach()

  get_property(_subdirs DIRECTORY "${Dir}" PROPERTY SUBDIRECTORIES)
  foreach(_subdir IN LISTS _subdirs)
    mengine_set_folder_recursive("${_subdir}" "${Folder}")
  endforeach()
endfunction()

set_property(GLOBAL PROPERTY USE_FOLDERS ON)
mengine_set_folder_recursive("${MENGINE_ASSIMP_ROOT}" "ThirdParty/assimp")

target_link_libraries(MEngine PRIVATE assimp)
target_compile_definitions(MEngine PRIVATE MYENGINE_WITH_ASSIMP=1)
