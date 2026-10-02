cmake_minimum_required(VERSION 3.12...4.0)
project(vfs.pdf)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

find_package(Kodi REQUIRED)
find_package(Poppler REQUIRED)
find_package(JPEG REQUIRED)

include_directories(${KODI_INCLUDE_DIR}/..
                    ${POPPLER_INCLUDE_DIRS}
                    ${JPEG_INCLUDE_DIRS})

set(PDF_SOURCES src/PdfFile.cpp
                src/PdfPages.cpp)
set(PDF_HEADERS src/PdfPages.h)

set(DEPLIBS ${POPPLER_LIBRARIES} ${JPEG_LIBRARIES})

build_addon(vfs.pdf PDF DEPLIBS)

include(CPack)
