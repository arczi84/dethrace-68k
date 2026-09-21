if(NOT DETHRACE_PLATFORM_AMIGA)
    message(FATAL_ERROR "brender_perspective_test is an AmigaOS-only diagnostic")
endif()

if(NOT DETHRACE_AMIGA_SHARED_MINIGL)
    message(FATAL_ERROR "brender_perspective_test requires the shared minigl.library ABI")
endif()

add_executable(brender_perspective_test main.c)
add_executable(brdepth main.c)

# This build directory is dedicated to the standalone diagnostic.  Enable
# switchable texture-coordinate paths in the linked 3dfx shim without changing
# the default path used by normal Dethrace builds.
target_compile_definitions(3dfx_amiga PRIVATE
    FXA_PERSPECTIVE_DIAGNOSTIC=1)

target_compile_definitions(brender_perspective_test PRIVATE
    AMIGA
    DETHRACE_PLATFORM_AMIGA
    DETHRACE_AMIGA_SHARED_MINIGL
)

target_compile_definitions(brdepth PRIVATE
    AMIGA
    DETHRACE_PLATFORM_AMIGA
    DETHRACE_AMIGA_SHARED_MINIGL
    BRENDER_DEPTH_STRESS=1
)

if(DETHRACE_AMIGA_SHARED_MINIGL_DISPATCH)
    target_compile_definitions(brender_perspective_test PRIVATE
        DETHRACE_AMIGA_SHARED_MINIGL_DISPATCH)
    target_compile_definitions(brdepth PRIVATE
        DETHRACE_AMIGA_SHARED_MINIGL_DISPATCH)
else()
    target_compile_definitions(brender_perspective_test PRIVATE
        MINIGL_REGISTER_LVO)
    target_compile_definitions(brdepth PRIVATE
        MINIGL_REGISTER_LVO)
endif()

target_include_directories(brender_perspective_test BEFORE PRIVATE
    "${DETHRACE_AMIGA_SHARED_MINIGL_ROOT}/include"
)
target_include_directories(brdepth BEFORE PRIVATE
    "${DETHRACE_AMIGA_SHARED_MINIGL_ROOT}/include"
)

target_link_libraries(brender_perspective_test PRIVATE
    BRender::Full
    BRender::DDI
    "${DETHRACE_AMIGA_SHARED_MINIGL_IMPORT_LIBRARY}"
)
target_link_libraries(brdepth PRIVATE
    BRender::Full
    BRender::DDI
    "${DETHRACE_AMIGA_SHARED_MINIGL_IMPORT_LIBRARY}"
)

set_target_properties(brender_perspective_test PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED TRUE
)
set_target_properties(brdepth PROPERTIES
    C_STANDARD 11
    C_STANDARD_REQUIRED TRUE
)
