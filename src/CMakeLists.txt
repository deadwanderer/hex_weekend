add_definitions(-D${sokol_backend})

fips_begin_app(hex_weekend windowed)
    fips_files(main.c)
    sokol_shader(shaders.glsl ${slang})
    #fips_deps(sokol HandmadeMath cdbgui)
fips_end_app()