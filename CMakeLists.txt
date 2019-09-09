cmake_minimum_required(VERSION 3.10.0)
project(meshoui VERSION 0.2.0)

find_package(Vulkan REQUIRED)

add_subdirectory(3rdparty)

set(shaders dome.glsl phong.glsl)
set_source_files_properties(${shaders} PROPERTIES HEADER_FILE_ONLY TRUE)
add_executable(meshouiview
    main.cpp
    mo_buffer.cpp  mo_material.cpp        mo_mesh.cpp        mo_pipeline.cpp
    mo_buffer.h    mo_material.h          mo_mesh.h          mo_pipeline.h
    mo_device.cpp  mo_material_utils.cpp  mo_mesh_utils.cpp  mo_swapchain.cpp
    mo_device.h    mo_material_utils.h    mo_mesh_utils.h    mo_swapchain.h
    ${shaders})

if(NOT MSVC)
    target_link_libraries(meshouiview ${Vulkan_LIBRARIES} assimp glfw linalg stdc++fs)
else()
    target_link_libraries(meshouiview ${Vulkan_LIBRARIES} assimp glfw linalg)
endif()

add_custom_command(TARGET meshouiview POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${CMAKE_CURRENT_SOURCE_DIR}/teapot.dae
                       $<TARGET_FILE_DIR:meshouiview>/)

set(glslang_output_dir ${CMAKE_CURRENT_SOURCE_DIR}/cache)
compile_glsl(spirv ${shaders})
add_custom_target(meshouiview_spirv DEPENDS ${spirv})
add_custom_command(TARGET meshouiview_spirv POST_BUILD
                   COMMAND ${CMAKE_COMMAND} -E copy
                       ${spirv}
                       ${CMAKE_BINARY_DIR})
add_dependencies(meshouiview meshouiview_spirv)
