add_executable(SandboxApp
    src/main.cpp
)

target_link_libraries(SandboxApp PRIVATE TucanoCore)

# Copy shaders to output directory
add_custom_command(TARGET SandboxApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_CURRENT_SOURCE_DIR}/assets
    $<TARGET_FILE_DIR:SandboxApp>/assets
)
