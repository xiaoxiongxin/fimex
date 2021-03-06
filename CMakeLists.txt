# Fimex, CMakeLists.txt
#
# Copyright (C) 2018-2019 met.no
#
# Contact information:
# Norwegian Meteorological Institute
# Box 43 Blindern
# 0313 OSLO
# NORWAY
# email: diana@met.no
#
# Project Info:  https://wiki.met.no/fimex/start
#
# This library is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as published by
# the Free Software Foundation; either version 2.1 of the License, or
# (at your option) any later version.
#
# This library is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
# or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public
# License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
# USA.

CMAKE_MINIMUM_REQUIRED(VERSION 2.8.12)
PROJECT(fimex)

SET(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
INCLUDE(FimexUtils)

FIMEX_CMAKE_SETUP()
FIMEX_VERSION_DEFINES(FIMEX "include/fimex/CDMconstants.h")

FIMEX_FIND_PACKAGE("libxml2" "libxml-2.0>=2.6.0" "xml2" "libxml/xmlversion.h")
LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE ${libxml2_PC})

FIMEX_FIND_PACKAGE("udunits2" "udunits>=2" "udunits2" "udunits2.h")
LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE ${udunits2_PC})

FIMEX_FIND_PACKAGE("proj" "proj" "proj" "proj_api.h")
LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE ${proj_PC})

OPTION(BUILD_SHARED_LIBS "Select ON (default) for shared lib, OFF for static lib, BOTH for both" ON)

OPTION(ENABLE_PYTHON "Use python" ON)

OPTION(USE_BOOST_UNIT_TEST "Use boost unit_test_framework" OFF)
IF (USE_BOOST_UNIT_TEST)
  FIND_PACKAGE(Boost REQUIRED COMPONENTS unit_test_framework)
  IF ((Boost_UNIT_TEST_FRAMEWORK_FOUND) AND (NOT (Boost_VERSION LESS 103400)))
    SET (HAVE_BOOST_UNIT_TEST_FRAMEWORK TRUE)
  ENDIF ()
ENDIF ()

OPTION(ENABLE_MPI "Use MPI" OFF)
IF(ENABLE_MPI)
  FIND_PACKAGE(MPI)
ENDIF()

OPTION(ENABLE_FIMEX_OMP "Use OpenMP" OFF)
IF(ENABLE_FIMEX_OMP)
  FIND_PACKAGE(OpenMP)
  IF(OPENMP_FOUND)
    SET(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
  ENDIF()
ENDIF()

OPTION(ENABLE_LOG4CPP "Use Log4Cpp" OFF)
IF(ENABLE_LOG4CPP)
  LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE log4cpp)
ENDIF()

OPTION(ENABLE_NETCDF "Use NetCDF" ON)
IF(ENABLE_NETCDF)
  FIMEX_FIND_PACKAGE("netcdf" "netcdf" "netcdf" "netcdf.h")
  LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE ${netcdf_PC})
ENDIF()

OPTION(ENABLE_FELT "Use Felt library" ON)

OPTION(ENABLE_ECCODES "Use ecCodes library" OFF)
OPTION(ENABLE_GRIBAPI "Use GribAPI library" OFF)

IF((ENABLE_GRIBAPI) AND (ENABLE_ECCODES))
  MESSAGE(FATAL_ERROR "Cannot use both ecCodes libraries")
ELSEIF(ENABLE_ECCODES)
  FIND_PACKAGE(eccodes REQUIRED)
  SET(eccodes_LIB "eccodes")
  MESSAGE(STATUS "Detected ecCodes version ${eccodes_VERSION}")
  # cmake variable "eccodes_FEATURES" was called "ECCODES_FEATURES" before 2.13.0
  IF((NOT eccodes_FEATURES) AND (ECCODES_FEATURES))
    SET(eccodes_FEATURES "${ECCODES_FEATURES}")
  ENDIF()
  # MESSAGE(STATUS "Detected ecCodes features '${eccodes_FEATURES}'")
  IF("${eccodes_FEATURES}" MATCHES "ECCODES_OMP_THREADS")
    MESSAGE(STATUS "Detected ecCodes OpenMP threading support")
    SET(HAVE_GRIB_API_THREADSAFE 1)
  ENDIF()
ELSEIF(ENABLE_GRIBAPI)
  FIMEX_FIND_PACKAGE("grib_api" "" "grib_api" "grib_api.h")
  OPTION(ENABLE_GRIBAPIPTHREAD "Use GribAPI library with pthread support" ON)
  IF(ENABLE_GRIBAPIPTHREAD)
    SET(HAVE_GRIB_API_THREADSAFE 1)
  ENDIF()
ENDIF()

OPTION(ENABLE_METGM "Use metgm" OFF)
IF(ENABLE_METGM)
  LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE metgm)
ENDIF()

OPTION(ENABLE_PRORADXML "Use proradxml" OFF)
IF(ENABLE_PRORADXML)
  LIST(APPEND FIMEX_PC_REQUIRES_PRIVATE libproradxmlrw)
ENDIF()

OPTION(ENABLE_FORTRAN "Use fortran" OFF)
IF(ENABLE_FORTRAN)
  ENABLE_LANGUAGE(Fortran)
ENDIF()

OPTION(ENABLE_FIMEX_VERSIONNUMBERED "Use version numbers in filenames" ON)

PKG_CHECK_MODULES(PC REQUIRED ${FIMEX_PC_REQUIRES};${FIMEX_PC_REQUIRES_PRIVATE})

IF(ENABLE_NETCDF)
  CHECK_NETCDF_HAS_HDF5(HAVE_NETCDF_HDF5_LIB)
ENDIF()

# github/HowardHinnant/date
IF (NOT date_INC_DIR)
  SET (DATE_3RDPARTY "third_party/date")
  IF ((NOT date_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${DATE_3RDPARTY}/CMakeLists.txt"))
    SET (date_INC_DIR "${CMAKE_SOURCE_DIR}/${DATE_3RDPARTY}/include")
  ELSE ()
    FIND_PACKAGE(date REQUIRED)
    SET (date_LIB date_interface) # just a header file, therefore '..._interface'
  ENDIF ()
ELSEIF (NOT (EXISTS "${date_INC_DIR}/date/date.h"))
  MESSAGE (SEND_ERROR "Missing '${date_INC_DIR}/date/date.h'")
ELSE()
  MESSAGE (STATUS "Using '${date_INC_DIR}/date/date.h'")
ENDIF ()

SET (MI_PROGRAMOPTIONS_3RDPARTY "third_party/mi-programoptions")
IF ((NOT mi-programoptions_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${MI_PROGRAMOPTIONS_3RDPARTY}/CMakeLists.txt"))
  ADD_SUBDIRECTORY(${MI_PROGRAMOPTIONS_3RDPARTY})
ELSE ()
  FIND_PACKAGE(mi-programoptions REQUIRED)
ENDIF ()

IF(ENABLE_FIMEX_VERSIONNUMBERED)
  SET(MINUS_FIMEX_VERSION "-${FIMEX_VERSION}")
  SET(pc_fimex_includedir "/fimex${MINUS_FIMEX_VERSION}")
  CONFIGURE_FILE("fimex-noversion.pc.in" "fimex.pc" @ONLY)
  INSTALL(FILES "${CMAKE_BINARY_DIR}/fimex.pc"    DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")
  SET(lib_soversion 0)
  SET(lib_version "${lib_soversion}.${FIMEX_VERSION_PATCH}.${FIMEX_VERSION_STATUS_DEC}")
ELSE()
  SET(MINUS_FIMEX_VERSION "")
  SET(pc_fimex_includedir "")
  SET(lib_soversion 0)
  SET(lib_version "${lib_soversion}.0.0")
ENDIF()
SET(lib_name "fimex${MINUS_FIMEX_VERSION}")
SET(FIMEX_INSTALL_INCLUDEDIR "${CMAKE_INSTALL_INCLUDEDIR}${pc_fimex_includedir}")


SET(FIMEX_PC "fimex${MINUS_FIMEX_VERSION}.pc")
FIMEX_GEN_PKGCONFIG(fimex.pc.in "${FIMEX_PC}"
  "${FIMEX_PC_REQUIRES}"
  "${grib_api_LIB};${libxml2_LIB};${netcdf_LIB};${proj_LIB};${udunits2_LIB};${eccodes_LIB}"
  "${CMAKE_INSTALL_LIBDIR}"
  "${FIMEX_INSTALL_INCLUDEDIR};${udunits2_INC_DIR};${proj_INC_DIR};${grib_api_INC_DIR}"
)
INSTALL(FILES "${CMAKE_BINARY_DIR}/${FIMEX_PC}" DESTINATION "${CMAKE_INSTALL_LIBDIR}/pkgconfig")

ENABLE_TESTING()

ADD_SUBDIRECTORY(include)
ADD_SUBDIRECTORY(src)
IF(ENABLE_FORTRAN)
  ADD_SUBDIRECTORY(modules/F90)
ENDIF()

IF(ENABLE_PYTHON)
  SET (PYBIND11_3RDPARTY "third_party/pybind11")
  IF ((NOT pybind11_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${PYBIND11_3RDPARTY}/CMakeLists.txt"))
    MESSAGE(STATUS "Including pybind11 project from ${PYBIND11_3RDPARTY}")
    ADD_SUBDIRECTORY(${PYBIND11_3RDPARTY})
  ELSE ()
    FIND_PACKAGE(pybind11 REQUIRED)
  ENDIF ()

  ADD_SUBDIRECTORY(modules/python)
ENDIF()

IF (NOT USE_BOOST_UNIT_TEST)
  SET (MI_CPPTEST_3RDPARTY "third_party/mi-cpptest")
  IF ((NOT mi-cpptest_DIR) AND (EXISTS "${CMAKE_SOURCE_DIR}/${MI_CPPTEST_3RDPARTY}/CMakeLists.txt"))
    ADD_SUBDIRECTORY(${MI_CPPTEST_3RDPARTY})
  ELSE ()
    FIND_PACKAGE(mi-cpptest REQUIRED)
  ENDIF ()
ENDIF ()

ADD_SUBDIRECTORY(test)
ADD_SUBDIRECTORY(share/etc)

SET(EXPORTNAMES "fimex")
IF(ENABLE_FELT)
  LIST(APPEND EXPORTNAMES "felt")
ENDIF()
IF(ENABLE_FORTRAN)
  LIST(APPEND EXPORTNAMES "fimexf")
ENDIF()

SET(FIMEX_VERSION_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fimex${MINUS_FIMEX_VERSION}")
IF(ENABLE_FIMEX_VERSIONNUMBERED)
  SET(FIMEX_NOVERSION_CMAKE_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/fimex")
ENDIF()

FOREACH(EXPORTNAME ${EXPORTNAMES})
  INSTALL (
    EXPORT ${EXPORTNAME}
    DESTINATION "${FIMEX_VERSION_CMAKE_DIR}"
    FILE "${EXPORTNAME}${MINUS_FIMEX_VERSION}-targets.cmake"
  )

  WRITE_BASIC_PACKAGE_VERSION_FILE(
    "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}${MINUS_FIMEX_VERSION}-config-version.cmake"
    VERSION
    "${FIMEX_VERSION_MAJOR}.${FIMEX_VERSION_MINOR}.${FIMEX_VERSION_PATCH}"
    COMPATIBILITY
    SameMajorVersion
  )

  CONFIGURE_FILE("export-config.cmake.in" "${EXPORTNAME}${MINUS_FIMEX_VERSION}-config.cmake" @ONLY)

  INSTALL(
    FILES
      "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}${MINUS_FIMEX_VERSION}-config.cmake"
      "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}${MINUS_FIMEX_VERSION}-config-version.cmake"
    DESTINATION "${FIMEX_VERSION_CMAKE_DIR}"
  )

  IF(ENABLE_FIMEX_VERSIONNUMBERED)
    CONFIGURE_FILE("export-noversion-config.cmake.in" "${EXPORTNAME}-config.cmake" @ONLY)
    CONFIGURE_FILE("export-noversion-config-version.cmake.in" "${EXPORTNAME}-config-version.cmake" @ONLY)
    INSTALL(
      FILES
        "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}-config.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/${EXPORTNAME}-config-version.cmake"
      DESTINATION "${FIMEX_NOVERSION_CMAKE_DIR}"
    )
  ENDIF()
ENDFOREACH()

FIND_PACKAGE(Doxygen)
IF(DOXYGEN_FOUND)
  CONFIGURE_FILE("Doxyfile.in" "Doxyfile" @ONLY)
  ADD_CUSTOM_TARGET(doc
    ${DOXYGEN_EXECUTABLE} "${CMAKE_BINARY_DIR}/Doxyfile"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Generating documentation with Doxygen" VERBATIM
    )
ENDIF()
