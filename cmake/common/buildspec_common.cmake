# Common build dependencies module

include_guard(GLOBAL)

# _check_deps_version: Checks for obs-deps VERSION file in prefix paths
function(_check_deps_version version)
  set(found FALSE)

  foreach(path IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${path}/share/obs-deps/VERSION")
      if(dependency STREQUAL qt6 AND NOT EXISTS "${path}/lib/cmake/Qt6/Qt6Config.cmake")
        set(found FALSE)
        continue()
      endif()

      file(READ "${path}/share/obs-deps/VERSION" _check_version)
      string(REPLACE "\n" "" _check_version "${_check_version}")
      string(REPLACE "-" "." _check_version "${_check_version}")
      string(REPLACE "-" "." version "${version}")

      if(_check_version VERSION_EQUAL version)
        set(found TRUE)
        break()
      elseif(_check_version VERSION_LESS version)
        message(
          AUTHOR_WARNING
          "Older ${label} version detected in ${path}: \n"
          "Found ${_check_version}, require ${version}"
        )
        list(REMOVE_ITEM CMAKE_PREFIX_PATH "${path}")
        list(APPEND CMAKE_PREFIX_PATH "${path}")
        set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH})
        continue()
      else()
        message(
          AUTHOR_WARNING
          "Newer ${label} version detected in ${path}: \n"
          "Found ${_check_version}, require ${version}"
        )
        set(found TRUE)
        break()
      endif()
    endif()
  endforeach()

  return(PROPAGATE found CMAKE_PREFIX_PATH)
endfunction()

# _setup_obs_studio: Create obs-studio build project, then build libobs and obs-frontend-api
function(_setup_obs_studio)
  set(_obs_use_ninja FALSE)
  if(NOT libobs_DIR)
    set(_is_fresh --fresh)
  endif()

  if(OS_WINDOWS)
    set(_cmake_generator "${CMAKE_GENERATOR}")
    set(_cmake_arch "-A ${arch},version=${CMAKE_VS_WINDOWS_TARGET_PLATFORM_VERSION}")
    set(_cmake_extra "-DCMAKE_SYSTEM_VERSION=${CMAKE_SYSTEM_VERSION} -DCMAKE_ENABLE_SCRIPTING=OFF")
  elseif(OS_MACOS)
    # OBS's macOS CMake requires Xcode generator; use Xcode so the dependency build succeeds.
    set(_cmake_generator "Xcode")
    set(_cmake_arch "-DCMAKE_OSX_ARCHITECTURES:STRING=arm64;x86_64")
    set(_cmake_extra "-DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(CMAKE_C_COMPILER)
      list(APPEND _cmake_extra "-DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}")
    endif()
    if(CMAKE_CXX_COMPILER)
      list(APPEND _cmake_extra "-DCMAKE_CXX_COMPILER=${CMAKE_CXX_COMPILER}")
    endif()
    set(_obs_use_ninja FALSE)
  endif()

  if(_obs_use_ninja)
    # Use Command Line Tools only so Xcode.app (and its broken IDESimulatorFoundation) is never loaded.
    set(_clt_dir "/Library/Developer/CommandLineTools")
    if(NOT EXISTS "${_clt_dir}/usr/bin/clang")
      message(
        FATAL_ERROR
        "Command Line Tools required for OBS build (Xcode generator disabled). "
        "Install with: xcode-select --install"
      )
    endif()
    set(ENV{DEVELOPER_DIR} "${_clt_dir}")
    # Ninja is single-config: one configure/build/install per configuration.
    set(_obs_configs Debug Release)
    foreach(_cfg IN LISTS _obs_configs)
      set(_bdir "build_${arch}_${_cfg}")
      message(STATUS "Configure ${label} (${arch} - ${_cfg})")
      execute_process(
        COMMAND
          "${CMAKE_COMMAND}" -S "${dependencies_dir}/${_obs_destination}" -B
          "${dependencies_dir}/${_obs_destination}/${_bdir}" -G Ninja "${_cmake_arch}"
          -DCMAKE_BUILD_TYPE=${_cfg}
          -DCMAKE_OSX_DEPLOYMENT_TARGET=${CMAKE_OSX_DEPLOYMENT_TARGET}
          -DOBS_CMAKE_VERSION:STRING=3.0.0 -DENABLE_PLUGINS:BOOL=OFF -DENABLE_FRONTEND:BOOL=OFF
          -DOBS_VERSION_OVERRIDE:STRING=${_obs_version} "-DCMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}" ${_is_fresh}
          ${_cmake_extra}
        RESULT_VARIABLE _process_result
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
      )
      message(STATUS "Configure ${label} (${arch} - ${_cfg}) - done")

      message(STATUS "Build ${label} (${_cfg} - ${arch})")
      execute_process(
        COMMAND "${CMAKE_COMMAND}" --build ${_bdir} --target obs-frontend-api --parallel
        WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
        RESULT_VARIABLE _process_result
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
      )
      message(STATUS "Build ${label} (${_cfg} - ${arch}) - done")

      message(STATUS "Install ${label} (${_cfg} - ${arch})")
      execute_process(
        COMMAND
          "${CMAKE_COMMAND}" --install ${_bdir} --component Development --prefix "${dependencies_dir}"
        WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
        RESULT_VARIABLE _process_result
        COMMAND_ERROR_IS_FATAL ANY
        OUTPUT_QUIET
      )
      message(STATUS "Install ${label} (${_cfg} - ${arch}) - done")
    endforeach()
    unset(ENV{DEVELOPER_DIR})
  else()
    # So OBS configure uses Xcode (not Command Line Tools), unset DEVELOPER_DIR for subprocess.
    if(DEFINED ENV{DEVELOPER_DIR})
      set(_saved_developer_dir "$ENV{DEVELOPER_DIR}")
      unset(ENV{DEVELOPER_DIR})
    endif()
    message(STATUS "Configure ${label} (${arch})")
    execute_process(
      COMMAND
        "${CMAKE_COMMAND}" -S "${dependencies_dir}/${_obs_destination}" -B
        "${dependencies_dir}/${_obs_destination}/build_${arch}" -G ${_cmake_generator} "${_cmake_arch}"
        -DOBS_CMAKE_VERSION:STRING=3.0.0 -DENABLE_PLUGINS:BOOL=OFF -DENABLE_FRONTEND:BOOL=OFF
        -DOBS_VERSION_OVERRIDE:STRING=${_obs_version} "-DCMAKE_PREFIX_PATH='${CMAKE_PREFIX_PATH}'" ${_is_fresh}
        ${_cmake_extra}
      RESULT_VARIABLE _process_result
      COMMAND_ERROR_IS_FATAL ANY
      OUTPUT_QUIET
    )
    message(STATUS "Configure ${label} (${arch}) - done")

    message(STATUS "Build ${label} (Debug - ${arch})")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Debug --parallel
      WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
      RESULT_VARIABLE _process_result
      COMMAND_ERROR_IS_FATAL ANY
      OUTPUT_QUIET
    )
    message(STATUS "Build ${label} (Debug - ${arch}) - done")

    message(STATUS "Build ${label} (Release - ${arch})")
    execute_process(
      COMMAND "${CMAKE_COMMAND}" --build build_${arch} --target obs-frontend-api --config Release --parallel
      WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
      RESULT_VARIABLE _process_result
      COMMAND_ERROR_IS_FATAL ANY
      OUTPUT_QUIET
    )
    message(STATUS "Build ${label} (Release - ${arch}) - done")

    message(STATUS "Install ${label} (${arch})")
    execute_process(
      COMMAND
        "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Debug --prefix "${dependencies_dir}"
      WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
      RESULT_VARIABLE _process_result
      COMMAND_ERROR_IS_FATAL ANY
      OUTPUT_QUIET
    )
    execute_process(
      COMMAND
        "${CMAKE_COMMAND}" --install build_${arch} --component Development --config Release --prefix "${dependencies_dir}"
      WORKING_DIRECTORY "${dependencies_dir}/${_obs_destination}"
      RESULT_VARIABLE _process_result
      COMMAND_ERROR_IS_FATAL ANY
      OUTPUT_QUIET
    )
    message(STATUS "Install ${label} (${arch}) - done")
    if(DEFINED _saved_developer_dir)
      set(ENV{DEVELOPER_DIR} "${_saved_developer_dir}")
    endif()
  endif()
endfunction()

# _check_dependencies: Fetch and extract pre-built OBS build dependencies
function(_check_dependencies)
  file(READ "${CMAKE_CURRENT_SOURCE_DIR}/buildspec.json" buildspec)

  string(JSON dependency_data GET ${buildspec} dependencies)

  foreach(dependency IN LISTS dependencies_list)
    string(JSON data GET ${dependency_data} ${dependency})
    string(JSON version GET ${data} version)
    string(JSON hash GET ${data} hashes ${platform})
    string(JSON url GET ${data} baseUrl)
    string(JSON label GET ${data} label)
    string(JSON revision ERROR_VARIABLE error GET ${data} revision ${platform})

    message(STATUS "Setting up ${label} (${arch})")

    set(file "${${dependency}_filename}")
    set(destination "${${dependency}_destination}")
    string(REPLACE "VERSION" "${version}" file "${file}")
    string(REPLACE "VERSION" "${version}" destination "${destination}")
    string(REPLACE "ARCH" "${arch}" file "${file}")
    string(REPLACE "ARCH" "${arch}" destination "${destination}")
    if(revision)
      string(REPLACE "_REVISION" "_v${revision}" file "${file}")
      string(REPLACE "-REVISION" "-v${revision}" file "${file}")
    else()
      string(REPLACE "_REVISION" "" file "${file}")
      string(REPLACE "-REVISION" "" file "${file}")
    endif()

    if(EXISTS "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256")
      file(
        READ
        "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256"
        OBS_DEPENDENCY_${dependency}_${arch}_HASH
      )
    endif()

    set(skip FALSE)
    if(dependency STREQUAL prebuilt OR dependency STREQUAL qt6)
      if(OBS_DEPENDENCY_${dependency}_${arch}_HASH STREQUAL ${hash})
        _check_deps_version(${version})

        if(found)
          set(skip TRUE)
        endif()
      endif()
    endif()

    if(skip)
      message(STATUS "Setting up ${label} (${arch}) - skipped")
      continue()
    endif()

    if(dependency STREQUAL obs-studio)
      set(url ${url}/${file})
    else()
      set(url ${url}/${version}/${file})
    endif()

    if(NOT EXISTS "${dependencies_dir}/${file}")
      message(STATUS "Downloading ${url}")
      file(DOWNLOAD "${url}" "${dependencies_dir}/${file}" STATUS download_status EXPECTED_HASH SHA256=${hash})

      list(GET download_status 0 error_code)
      list(GET download_status 1 error_message)
      if(error_code GREATER 0)
        message(STATUS "Downloading ${url} - Failure")
        message(FATAL_ERROR "Unable to download ${url}, failed with error: ${error_message}")
        file(REMOVE "${dependencies_dir}/${file}")
      else()
        message(STATUS "Downloading ${url} - done")
      endif()
    endif()

    if(NOT OBS_DEPENDENCY_${dependency}_${arch}_HASH STREQUAL ${hash})
      file(REMOVE_RECURSE "${dependencies_dir}/${destination}")
    endif()

    if(NOT EXISTS "${dependencies_dir}/${destination}")
      file(MAKE_DIRECTORY "${dependencies_dir}/${destination}")
      if(dependency STREQUAL obs-studio)
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${dependencies_dir}")
      else()
        file(ARCHIVE_EXTRACT INPUT "${dependencies_dir}/${file}" DESTINATION "${dependencies_dir}/${destination}")
      endif()
    endif()

    file(WRITE "${dependencies_dir}/.dependency_${dependency}_${arch}.sha256" "${hash}")

    if(dependency STREQUAL prebuilt)
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/${destination}")
    elseif(dependency STREQUAL qt6)
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}/${destination}")
    elseif(dependency STREQUAL obs-studio)
      set(_obs_version ${version})
      set(_obs_destination "${destination}")
      list(APPEND CMAKE_PREFIX_PATH "${dependencies_dir}")
    endif()

    message(STATUS "Setting up ${label} (${arch}) - done")
  endforeach()

  list(REMOVE_DUPLICATES CMAKE_PREFIX_PATH)

  set(CMAKE_PREFIX_PATH ${CMAKE_PREFIX_PATH} CACHE PATH "CMake prefix search path" FORCE)

  _setup_obs_studio()
endfunction()
