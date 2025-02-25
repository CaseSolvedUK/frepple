
set(CMAKE_VERBOSE_MAKEFILE ON)
cmake_minimum_required (VERSION 3.0)

project(frepple VERSION 6.21.0)

set(PROJECT_EDITION "Community Edition")
set(PROJECT_BRANCH "community")

if (POLICY CMP0111)
  cmake_policy(SET CMP0111 OLD)
endif()

if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  # Detect the linux distribution
  if (EXISTS "/etc/issue")
    file(READ "/etc/issue" LINUX_ISSUE)
    # Ubuntu 18
    if (LINUX_ISSUE MATCHES "Ubuntu 18")
      set(CPACK_GENERATOR "DEB")
      set(DISTRO "ubuntu18")
    # Ubuntu 19
    elseif (LINUX_ISSUE MATCHES "Ubuntu 19")
      set(CPACK_GENERATOR "DEB")
      set(DISTRO "ubuntu19")
    # Ubuntu 20
    elseif (LINUX_ISSUE MATCHES "Ubuntu 20")
      set(CPACK_GENERATOR "DEB")
      set(DISTRO "ubuntu20")
    # Debian case
    elseif (LINUX_ISSUE MATCHES "Debian GNU/Linux 10")
      set(CPACK_GENERATOR "DEB")
      set(DISTRO "debian10")
    elseif (EXISTS "/etc/redhat-release")
      file(READ "/etc/redhat-release" REDHAT_RELEASE)
      if (REDHAT_RELEASE MATCHES "Red Hat Enterprise Linux release 8")
        set(CPACK_GENERATOR "RPM")
        set(DISTRO "rhel8")
      elseif (REDHAT_RELEASE MATCHES "Rocky Linux release 8")
        set(CPACK_GENERATOR "RPM")
        set(DISTRO "rockylinux8")
      endif()
    elseif (EXISTS "/etc/os-release")
      file(READ "/etc/os-release" OS_RELEASE)
      if (OS_RELEASE MATCHES "openSUSE Leap 15")
        set(CPACK_GENERATOR "RPM")
        set(DISTRO "opensuse15")
      endif()
    endif()
  endif()
  if (NOT DISTRO)
    message(WARNING "FrePPLe has not been tested on this linux distribution.")
  endif()
endif()

# Check third party libraries
if (WIN32)
  # Some hard-coded paths for third party libraries on Windows.
  set(XERCESC_FOLDER "/develop/xerces-c-3.2.3")
  if(EXISTS "${XERCESC_FOLDER}")
    include_directories("${XERCESC_FOLDER}/include")
    list(APPEND CMAKE_REQUIRED_INCLUDES "${XERCESC_FOLDER}/include")
    add_library(xerces-c STATIC IMPORTED)
    set_target_properties(xerces-c PROPERTIES
       IMPORTED_LOCATION_DEBUG "${XERCESC_FOLDER}/lib/xerces-c_3D.lib"
       IMPORTED_LOCATION_RELEASE "${XERCESC_FOLDER}/lib/xerces-c_3.lib"
       )
    endif()
  # PostgreSQL ZIP-installer from https://www.enterprisedb.com/download-postgresql-binaries
  # Used to embed PostgreSQL in frepple's windows installer
  set(POSTGRES_FOLDER "c:/develop/pgsql")
else()
  find_library(xerces-c NAMES xerces-c)
  if (NOT xerces-c)
    message(FATAL_ERROR "xerces-c library not found")
  endif()
endif()

if (UNIX)
  set(CMAKE_INSTALL_PREFIX "/")
  include(GNUInstallDirs)
else()
  set(CMAKE_INSTALL_LIBDIR "bin")
  set(CMAKE_INSTALL_SYSCONFDIR "custom")
  set(CMAKE_INSTALL_DATADIR "custom")
  set(CMAKE_INSTALL_BINDIR "custom")
  set(CMAKE_INSTALL_INCLUDEDIR "include")
endif()
find_package(PythonInterp 3.6 REQUIRED)
find_package(PythonLibs 3.6 REQUIRED)
execute_process(
  COMMAND "${PYTHON_EXECUTABLE}" -c "import django"
  OUTPUT_QUIET
  ERROR_QUIET
  RESULT_VARIABLE NO_DJANGO
  )
if (NO_DJANGO)
  message(FATAL_ERROR "Django isn't installed.")
endif()

find_program(npm NAMES npm)
find_program(grunt NAMES grunt)

# C++ compiler flags and features
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if (WIN32)
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT /EHsc /MP")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_DEBUG /MTd /EHsc /MP")
  add_definitions("-D_CRT_SECURE_NO_DEPRECATE")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=date-time")
  set(CMAKE_CXX_FLAGS_PROFILING "${CMAKE_CXX_FLAGS_DEBUG} -DNDEBUG")
  set(CMAKE_C_FLAGS_PROFILING "${CMAKE_C_FLAGS_DEBUG} -DNDEBUG")
  set(CMAKE_EXE_LINKER_FLAGS_PROFILING "")
  set(CMAKE_SHARED_LINKER_FLAGS_PROFILING "")
endif()
include(CheckIncludeFile)
include(CheckIncludeFileCXX)
include(CheckFunctionExists)
check_include_file(sys/prctl.h HAVE_SYS_PRCTL_H)
check_include_file(errno.h HAVE_ERRNO_H)
check_include_file(unistd.h HAVE_UNISTD_H)
check_include_file(crypt.h HAVE_CRYPT_H)
check_function_exists(localtime_r HAVE_LOCALTIME_R)
check_function_exists(dup2 HAVE_DUP2)
check_function_exists(memset HAVE_MEMSET)
check_function_exists(strncasecmp HAVE_STRNCASECMP)
check_function_exists(strnicmp HAVE_STRNICMP)
check_function_exists(strptime HAVE_STRPTIME)
check_function_exists(prctl HAVE_PRCTL)

# Verify third party header files
check_include_file_cxx("xercesc/util/XercesDefs.hpp" HAVE_XERCESC)
if (NOT ${HAVE_XERCESC})
  message(FATAL_ERROR "xerces-c include headers not found")
endif()

# C++ header file locations
include_directories(
  "${CMAKE_SOURCE_DIR}/include"
  "${CMAKE_BINARY_DIR}/include"
  "${PYTHON_INCLUDE_DIRS}"
  )
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/include/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
  )

# File installation
install(
  FILES frepplectl.py
  DESTINATION "${CMAKE_INSTALL_BINDIR}"
  RENAME frepplectl
  PERMISSIONS OWNER_EXECUTE OWNER_READ GROUP_EXECUTE GROUP_READ
  )
install(
  FILES djangosettings.py
  DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/frepple"
  )
install(
  FILES requirements.txt
  DESTINATION "${CMAKE_INSTALL_DATADIR}/frepple"
  )

# Packaging of the django app: Python module + static files
# Note that we use \$ to delay the evaluation till install time
configure_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/setup.py.in"
  "${CMAKE_CURRENT_BINARY_DIR}/setup.py"
  )
execute_process (
  COMMAND ${PYTHON_EXECUTABLE} -c "import sys; print(sys.prefix)"
  OUTPUT_VARIABLE PYTHON_PREFIX
  ERROR_QUIET
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
if ("${CPACK_GENERATOR}" STREQUAL "DEB")
  # Debian installs in dist-packages. Nobody knows why :-)
  install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/setup.py install --prefix=\$ENV{DESTDIR}${PYTHON_PREFIX}  --quiet --install-layout=deb)")
else()
  # Standard install goes into site-packages
  install(CODE "execute_process(COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_BINARY_DIR}/setup.py install --prefix=\$ENV{DESTDIR}${PYTHON_PREFIX} --quiet)")
endif()
install(CODE "execute_process(COMMAND ${CMAKE_COMMAND} -E env FREPPLE_STATIC=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_DATADIR}/frepple/static ${PYTHON_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/frepplectl.py collectstatic --noinput --clear --ignore '*.less' --verbosity=0)")

# Generic packaging
set(CPACK_PACKAGE_VENDOR "frePPLe")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "FrePPLe is an open source production planning and scheduling application.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_FILE_NAME "${DISTRO}-${PROJECT_NAME}-${CPACK_PACKAGE_VERSION}")
set(CPACK_SOURCE_GENERATOR "TGZ")
set(CPACK_SOURCE_IGNORE_FILES
  "__pycache__"
  "^${PROJECT_SOURCE_DIR}/\\\\.git"
  "^${PROJECT_SOURCE_DIR}/\\\\.vs"
  "^${PROJECT_SOURCE_DIR}/\\\\.settings"
  "^${PROJECT_SOURCE_DIR}/logs/"
  "^${PROJECT_SOURCE_DIR}/build.*"
  "^${PROJECT_SOURCE_DIR}/static$"
  "^${PROJECT_SOURCE_DIR}/localsettings.*py$"
  "^${PROJECT_SOURCE_DIR}/node_modules"
  "^${PROJECT_SOURCE_DIR}/bin/.*\\\\.exe$"
  "^${PROJECT_SOURCE_DIR}/bin/frepple$"
  "^${PROJECT_SOURCE_DIR}/bin/frepple.dll$"
  "^${PROJECT_SOURCE_DIR}/bin/libfrepple.*"
  "^${PROJECT_SOURCE_DIR}/bin/.*\\\\.pdb$"
  )

# Add folders
add_subdirectory(bin)
add_subdirectory(src)
add_subdirectory(doc)
add_subdirectory(contrib/docker)
add_subdirectory(contrib/linux)

# Test suite
enable_testing()
add_test(NAME engine
  COMMAND ${PYTHON} ${CMAKE_CURRENT_SOURCE_DIR}/test/runtest.py ${TESTARGS}
  )

# Debugging targets
find_program(valgrind NAMES valgrind)
if (valgrind)
  add_custom_target(callgrind ${CMAKE_COMMAND} -E
    env
      FREPPLE_HOME=${CMAKE_SOURCE_DIR}/bin
      PYTHONPATH=${CMAKE_SOURCE_DIR}
      DJANGO_SETTINGS_MODULE=freppledb.settings
      FREPPLE_LOGFILE=frepple.log
      LD_BIND_NOW=1
      fcst=1
      invplan=1
      supply=1
      nowebservice=1
      constraint=13
      plantype=1
    valgrind --tool=callgrind
       ${CMAKE_SOURCE_DIR}/bin/frepple
       ${CMAKE_SOURCE_DIR}/freppledb/common/commands.py
    )
endif()
find_program(gdb NAMES gdb)
if (gdb)
  add_custom_target(gdb ${CMAKE_COMMAND} -E
    env
      FREPPLE_HOME=${CMAKE_SOURCE_DIR}/bin
      PYTHONPATH=${CMAKE_SOURCE_DIR}
      DJANGO_SETTINGS_MODULE=freppledb.settings
      FREPPLE_LOGFILE=frepple.log
      supply=1
      constraint=13
      plantype=1
    gdb -ex=r --args ${CMAKE_SOURCE_DIR}/bin/frepple ${CMAKE_SOURCE_DIR}/freppledb/common/commands.py
    )
endif()

# Translations process:
# 1) Run "cmake --build . --target translations_extract" to get all translatable strings from the source code.
# 2) Translate all strings local/<LANGUAGE>/<LANGUAGE>.po file. The real work!
# 3) Run "make  --build . --target compile-translations" to merge the translations into the right places.
if (CMAKE_SYSTEM_NAME MATCHES "Linux")
  add_custom_target(translations_extract
    COMMAND ${CMAKE_SOURCE_DIR}/translations.sh extract
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
  add_custom_target(translations_compile
    COMMAND ${CMAKE_SOURCE_DIR}/translations.sh compile
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    )
endif()

