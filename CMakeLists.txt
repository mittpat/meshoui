cmake_minimum_required(VERSION 3.10.0)
project(meshoui VERSION 0.2.0)

find_package(Vulkan REQUIRED)

add_subdirectory(3rdparty)

set(shaders shaders/dome.glsl shaders/phong.glsl shaders/passthrough.glsl)
set_source_files_properties(${shaders} PROPERTIES HEADER_FILE_ONLY TRUE)
set(resources resources/teapot.dae)
set_source_files_properties(${resources} PROPERTIES HEADER_FILE_ONLY TRUE)
add_library(meshoui
    mo_array.h          mo_material.cpp        mo_mesh_utils.cpp  mo_pipeline_utils.cpp
    mo_buffer.cpp       mo_material.h          mo_mesh_utils.h    mo_pipeline_utils.h
    mo_buffer.h         mo_material_utils.cpp  mo_node.cpp        mo_swapchain.cpp
    mo_device.cpp       mo_material_utils.h    mo_node.h          mo_swapchain.h
    mo_device.h         mo_mesh.cpp            mo_pipeline.cpp
    mo_example_utils.h  mo_mesh.h              mo_pipeline.h
    ${shaders} ${resources})
target_include_directories(meshoui PUBLIC .)
if(NOT MSVC)
    target_link_libraries(meshoui PUBLIC ${Vulkan_LIBRARIES} assimp linalg stb stdc++fs)
else()
    target_link_libraries(meshoui PUBLIC ${Vulkan_LIBRARIES} assimp linalg stb)
endif()

add_custom_command(TARGET meshoui POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy_directory
                       ${CMAKE_CURRENT_SOURCE_DIR}/resources/
                       $<TARGET_FILE_DIR:meshoui>/resources/)

set(glslang_output_dir ${CMAKE_CURRENT_SOURCE_DIR}/cache)
compile_glsl(spirv ${shaders})
add_custom_target(meshoui_spirv DEPENDS ${spirv})
add_custom_command(TARGET meshoui_spirv POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${spirv}
                       ${CMAKE_BINARY_DIR})
add_dependencies(meshoui meshoui_spirv)

add_executable(teapot_readback examples/teapot_readback/main.cpp)
target_link_libraries(teapot_readback PUBLIC meshoui)

add_executable(teapot_viewer examples/teapot_viewer/main.cpp)
target_link_libraries(teapot_viewer PUBLIC meshoui glfw)

add_executable(cube_passthrough examples/cube_passthrough/main.cpp)
target_link_libraries(cube_passthrough PUBLIC meshoui glfw)

add_executable(cube_imgui examples/cube_imgui/main.cpp)
target_link_libraries(cube_imgui PUBLIC meshoui glfw imgui)
