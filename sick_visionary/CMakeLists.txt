cmake_minimum_required(VERSION 2.8.3)
project(sick_visionary)

find_package(catkin REQUIRED COMPONENTS
  cv_bridge
  pcl_conversions
  pcl_ros
  image_transport
  roscpp
  roslaunch
)

find_package(Boost REQUIRED COMPONENTS system)
find_package(OpenCV REQUIRED)
find_package(PCL 1.3 REQUIRED)

catkin_package(
  INCLUDE_DIRS include
  CATKIN_DEPENDS pcl_conversions pcl_ros roscpp
)

### COMPILER FLAGS ###
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_FLAGS "-Wall -Wextra")
set(CMAKE_CXX_FLAGS_DEBUG "-g")
set(CMAKE_CXX_FLAGS_RELEASE "-O3")

set(CMAKE_CXX_FLAGS "-std=c++11 ${CMAKE_CXX_FLAGS}")

### BUILD ###
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
  include
  sick_visionary_cpp_shared
)

add_subdirectory(sick_visionary_cpp_shared)

add_executable(sick_visionary_t_node src/visionary_t.cpp)
add_executable(sick_visionary_t_mini_node src/visionary_t_mini.cpp)
add_executable(sick_visionary_t_vga_node src/visionary_t_vga.cpp)
add_executable(sick_visionary_s_node src/visionary_s.cpp)

target_link_libraries(sick_visionary_t_node
  sick_visionary_cpp_shared
  ${catkin_LIBRARIES}
  ${OpenCV_LIBS}
  ${PCL_LIBRARY_DIRS}
)

target_link_libraries(sick_visionary_t_mini_node
  sick_visionary_cpp_shared
  ${catkin_LIBRARIES}
  ${OpenCV_LIBS}
  ${PCL_LIBRARY_DIRS}
)

target_link_libraries(sick_visionary_t_vga_node
  sick_visionary_cpp_shared
  ${catkin_LIBRARIES}
  ${OpenCV_LIBS}
  ${PCL_LIBRARY_DIRS}
)

target_link_libraries(sick_visionary_s_node
  sick_visionary_cpp_shared
  ${catkin_LIBRARIES}
  ${OpenCV_LIBS}
  ${PCL_LIBRARY_DIRS}
)

add_definitions(${PCL_DEFINITIONS})

### TEST ###
if(CATKIN_ENABLE_TESTING)
  roslaunch_add_file_check(launch)
endif()

### INSTALL ###
install(TARGETS sick_visionary_t_node sick_visionary_t_mini_node sick_visionary_t_vga_node sick_visionary_s_node
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)

install(DIRECTORY launch
  DESTINATION ${CATKIN_PACKAGE_SHARE_DESTINATION}
)
