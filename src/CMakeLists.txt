add_executable(
    Stickit WIN32
    main.cpp

    resource.h
    resource.rc
)
target_compile_definitions(Stickit PRIVATE UNICODE)
if (MSVC)
    target_compile_options(Stickit PRIVATE /await)
endif ()
target_link_libraries(Stickit PRIVATE d2d1.lib)
set_target_properties(Stickit PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
