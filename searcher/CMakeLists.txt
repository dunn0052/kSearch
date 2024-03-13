cmake_minimum_required(VERSION 3.16)

project(kSearch)

set(SRC
    src/main.cpp
    src/kSearch.cpp
)

add_executable(${PROJECT_NAME}  ${SRC} )

target_include_directories(${PROJECT_NAME} PRIVATE
  ${CMAKE_SOURCE_DIR}
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)
target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)

target_compile_definitions(${PROJECT_NAME} PRIVATE
  __LOG_ENABLE
  #__LOG_SHOW_LINE
  __LOG_SHOW_COLOR
)

install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)