
# Add source to this project's executable.
add_executable (simple-engine
  main.cpp
  vk_engine.cpp
  vk_engine.h

  vk_loader.h
  vk_loader.cpp
)


set_property(TARGET simple-engine PROPERTY CXX_STANDARD 20)
set_property(TARGET simple-engine PROPERTY VS_DEBUGGER_WORKING_DIRECTORY $<TARGET_FILE_DIR:simple-engine>)

target_include_directories(simple-engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(simple-engine vkguide_shared vkbootstrap imgui fastgltf::fastgltf)

add_custom_command(TARGET simple-engine POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_RUNTIME_DLLS:simple-engine> $<TARGET_FILE_DIR:simple-engine>
  COMMAND_EXPAND_LISTS
  )