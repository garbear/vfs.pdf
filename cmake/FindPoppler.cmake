# FindPoppler
# -----------
# Finds Poppler's C++ frontend.
#
#   POPPLER_FOUND, POPPLER_INCLUDE_DIRS, POPPLER_LIBRARIES

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_POPPLER QUIET poppler-cpp)
endif()

find_path(POPPLER_INCLUDE_DIR NAMES poppler-document.h
                              PATH_SUFFIXES poppler/cpp
                              HINTS ${PC_POPPLER_INCLUDEDIR} ${PC_POPPLER_INCLUDE_DIRS})
find_library(POPPLER_CPP_LIBRARY NAMES poppler-cpp
                                 HINTS ${PC_POPPLER_LIBDIR} ${PC_POPPLER_LIBRARY_DIRS})
find_library(POPPLER_CORE_LIBRARY NAMES poppler
                                  HINTS ${PC_POPPLER_LIBDIR} ${PC_POPPLER_LIBRARY_DIRS})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Poppler
                                  REQUIRED_VARS POPPLER_CPP_LIBRARY POPPLER_CORE_LIBRARY
                                                POPPLER_INCLUDE_DIR
                                  VERSION_VAR PC_POPPLER_VERSION)

if(POPPLER_FOUND)
  set(POPPLER_INCLUDE_DIRS ${POPPLER_INCLUDE_DIR})
  # A static Poppler needs the libraries it was built against linked in too.
  if(APPLE AND POPPLER_CPP_LIBRARY MATCHES "\\.a$")
    find_package(Freetype REQUIRED)
    find_package(ZLIB REQUIRED)
    find_package(OpenJPEG REQUIRED)
    find_package(Iconv REQUIRED)

    set(POPPLER_LIBRARIES
        ${POPPLER_CPP_LIBRARY}
        ${POPPLER_CORE_LIBRARY}
        Freetype::Freetype
        ZLIB::ZLIB
        ${OPENJPEG_LIBRARIES}
        Iconv::Iconv)
  elseif(POPPLER_CPP_LIBRARY MATCHES "\\.a$" AND PC_POPPLER_STATIC_LDFLAGS)
    set(POPPLER_LIBRARIES ${PC_POPPLER_STATIC_LDFLAGS})
  else()
    set(POPPLER_LIBRARIES ${POPPLER_CPP_LIBRARY} ${POPPLER_CORE_LIBRARY})
  endif()
endif()

mark_as_advanced(POPPLER_INCLUDE_DIR POPPLER_CPP_LIBRARY POPPLER_CORE_LIBRARY)
