add_executable(velnoria_tests
    unit/test_chunk.cpp
    unit/test_frame.cpp
    unit/test_harness.cpp
    unit/test_md5_uuid.cpp
    unit/test_modified_utf8.cpp
    unit/test_nbt.cpp
    unit/test_packet_table.cpp
    unit/test_protocol_types.cpp
    unit/test_varint.cpp)

target_link_libraries(velnoria_tests PRIVATE Velnoria::Core)
target_include_directories(velnoria_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/unit)

add_test(NAME unit COMMAND velnoria_tests)

# Differential tests need locally generated fixtures and are therefore not part
# of the default test run. See docs/design/vertical-slice-01.md section 9.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/differential/CMakeLists.txt")
    add_subdirectory(differential)
endif()
