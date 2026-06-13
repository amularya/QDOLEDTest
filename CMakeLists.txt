cmake_minimum_required(VERSION 3.20)
project(QDOLEDTextTuner VERSION 1.1.0 LANGUAGES CXX)

if(NOT WIN32)
  message(STATUS "QDOLED Text Tuner is a Windows application; source validation is still available via CTest.")
endif()

if(WIN32)
  enable_language(RC)
  add_executable(QDOLEDTextTuner WIN32
    src/main.cpp
    src/QDOLEDTextTuner.rc
    src/QDOLEDTextTuner.manifest
  )
  target_compile_features(QDOLEDTextTuner PRIVATE cxx_std_20)
  target_compile_definitions(QDOLEDTextTuner PRIVATE
    UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX
    WINVER=0x0A00 _WIN32_WINNT=0x0A00
  )
  set_source_files_properties(src/QDOLEDTextTuner.manifest PROPERTIES HEADER_FILE_ONLY TRUE)
  target_link_libraries(QDOLEDTextTuner PRIVATE comctl32 comdlg32 dwmapi ole32 shell32 uuid)
  if(MSVC)
    target_compile_options(QDOLEDTextTuner PRIVATE /W4 /permissive- /utf-8)
  else()
    target_compile_options(QDOLEDTextTuner PRIVATE -Wall -Wextra -Wpedantic)
  endif()
  install(TARGETS QDOLEDTextTuner RUNTIME DESTINATION .)
endif()

enable_testing()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(NAME source_contract
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/source_contract.py")
endif()
