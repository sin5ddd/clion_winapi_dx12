cmake_minimum_required(VERSION 3.20)
project(winapi)

if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
	add_compile_options(-D _DEBUG)
endif ()

set(CMAKE_CXX_STANDARD 20)

add_executable(winapi main.cpp)
target_link_libraries(winapi d3d12.lib dxgi.lib dxguid.lib d3dcompiler.lib)