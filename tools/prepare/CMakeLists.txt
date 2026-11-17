add_executable(velnoria_prepare src/main.cpp)
target_link_libraries(velnoria_prepare PRIVATE Velnoria::Core)

# getenv is fine for a CLI that reads two documented variables; the MSVC
# deprecation shim only adds noise here.
if(MSVC)
    target_compile_definitions(velnoria_prepare PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
