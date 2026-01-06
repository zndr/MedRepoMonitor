cmake_minimum_required(VERSION 3.20)
project(MedicalReportMonitor LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DUNICODE -D_UNICODE)
add_definitions(-D_SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING)

set(SOURCES
    src/main.cpp
    src/Config.cpp
    src/FileWatcher.cpp
    src/PdfExtractor.cpp
    src/TextParser.cpp
    src/ReportProfile.cpp
    src/ClipboardHelper.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_link_libraries(${PROJECT_NAME} PRIVATE user32 shell32)

set_target_properties(${PROJECT_NAME} PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)