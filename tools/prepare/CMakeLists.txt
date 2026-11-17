add_executable(lumenvox_prepare src/main.cpp)
target_link_libraries(lumenvox_prepare PRIVATE Lumenvox::Core)

# getenv is fine for a CLI that reads two documented variables; the MSVC
# deprecation shim only adds noise here.
if(MSVC)
    target_compile_definitions(lumenvox_prepare PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
