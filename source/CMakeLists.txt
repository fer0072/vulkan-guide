
# Add source to this project's executable.
add_executable (simple-engine
  main.cpp
  vk_engine.cpp
  vk_engine.h

  vk_loader.h
  vk_loader.cpp
  vk_types.h
  vk_initializers.cpp
  vk_initializers.h
  vk_images.h
  vk_images.cpp 
  vk_descriptors.h
  vk_descriptors.cpp
  vk_pipelines.h
  vk_pipelines.cpp
  vk_scene.h
  vk_scene.cpp

  camera.cpp
  camera.h
)


set_property(TARGET simple-engine PROPERTY CXX_STANDARD 20)
set_property(TARGET simple-engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:simple-engine>)

target_compile_definitions(simple-engine PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE)

target_include_directories(simple-engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(simple-engine PUBLIC vkbootstrap imgui fastgltf::fastgltf PUBLIC vma glm Vulkan::Vulkan fmt::fmt stb_image SDL2::SDL2)

target_precompile_headers(simple-engine PUBLIC <optional> <vector> <memory> <string> <vector> <unordered_map> <glm/mat4x4.hpp>  <glm/vec4.hpp> <vulkan/vulkan.h>)

add_custom_command(TARGET simple-engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:simple-engine> $<TARGET_FILE_DIR:simple-engine>
  COMMAND_EXPAND_LISTS
  )