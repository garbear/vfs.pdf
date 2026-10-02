# The page renderer's tests, which need Poppler and libjpeg but not Kodi:
#   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests
cmake_minimum_required(VERSION 3.12...4.0)
project(vfs.pdf.tests CXX)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/../cmake)

find_package(Poppler REQUIRED)
find_package(JPEG REQUIRED)

enable_testing()
add_executable(test_pdfpages TestPdfPages.cpp ../src/PdfPages.cpp)
target_include_directories(test_pdfpages PRIVATE ${POPPLER_INCLUDE_DIRS} ${JPEG_INCLUDE_DIRS})
target_link_libraries(test_pdfpages PRIVATE ${POPPLER_LIBRARIES} ${JPEG_LIBRARIES})
target_compile_features(test_pdfpages PRIVATE cxx_std_17)
add_test(NAME pdf_pages COMMAND test_pdfpages)
