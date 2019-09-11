cmake_minimum_required(VERSION 3.10.0)
project(meshoui VERSION 0.2.0)

find_package(Vulkan REQUIRED)

add_subdirectory(3rdparty)

set(shaders shaders/dome.glsl shaders/phong.glsl)
set_source_files_properties(${shaders} PROPERTIES HEADER_FILE_ONLY TRUE)
set(resources resources/teapot.dae)
set_source_files_properties(${resources} PROPERTIES HEADER_FILE_ONLY TRUE)
add_library(meshoui
    mo_buffer.cpp  mo_material.cpp        mo_mesh.cpp        mo_pipeline.cpp
    mo_buffer.h    mo_material.h          mo_mesh.h          mo_pipeline.h
    mo_device.cpp  mo_material_utils.cpp  mo_mesh_utils.cpp  mo_swapchain.cpp
    mo_device.h    mo_material_utils.h    mo_mesh_utils.h    mo_swapchain.h
    ${shaders} ${resources})
target_include_directories(meshoui PUBLIC .)
if(NOT MSVC)
    target_link_libraries(meshoui PUBLIC ${Vulkan_LIBRARIES} assimp linalg stdc++fs)
else()
    target_link_libraries(meshoui PUBLIC ${Vulkan_LIBRARIES} assimp linalg)
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
target_link_libraries(teapot_readback PUBLIC meshoui stb)

add_executable(teapot_viewer examples/teapot_viewer/main.cpp)
target_link_libraries(teapot_viewer PUBLIC meshoui glfw stb)

