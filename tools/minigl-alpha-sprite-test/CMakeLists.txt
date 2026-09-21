if(NOT DETHRACE_PLATFORM_AMIGA)
    message(FATAL_ERROR "minigl_alpha_sprite_test is an AmigaOS-only diagnostic")
endif()

if(NOT DETHRACE_AMIGA_SHARED_MINIGL)
    message(FATAL_ERROR "minigl_alpha_sprite_test requires shared minigl.library")
endif()

add_executable(minigl_alpha_sprite_test main.c)

target_compile_definitions(minigl_alpha_sprite_test PRIVATE
    AMIGA
    DETHRACE_PLATFORM_AMIGA
    DETHRACE_AMIGA_SHARED_MINIGL
)

if(DETHRACE_AMIGA_SHARED_MINIGL_DISPATCH)
    target_compile_definitions(minigl_alpha_sprite_test PRIVATE
        DETHRACE_AMIGA_SHARED_MINIGL_DISPATCH)
else()
    target_compile_definitions(minigl_alpha_sprite_test PRIVATE
        MINIGL_REGISTER_LVO)
endif()

target_include_directories(minigl_alpha_sprite_test BEFORE PRIVATE
    "${DETHRACE_AMIGA_SHARED_MINIGL_ROOT}/include"
)

target_link_libraries(minigl_alpha_sprite_test PRIVATE
    "${DETHRACE_AMIGA_SHARED_MINIGL_IMPORT_LIBRARY}"
)

set_target_properties(minigl_alpha_sprite_test PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED TRUE
)
