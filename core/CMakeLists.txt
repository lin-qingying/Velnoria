add_library(lumenvox_core STATIC
    src/byte_reader.cpp
    src/chunk_pos.cpp
    src/chunk_snapshot.cpp
    src/error.cpp
    src/frame.cpp
    src/handshake.cpp
    src/md5.cpp
    src/modified_utf8.cpp
    src/nbt_io.cpp
    src/nbt_tag.cpp
    src/packet_table.cpp
    src/protocol_types.cpp
    src/utf8.cpp
    src/uuid.cpp
    src/varint.cpp)

target_include_directories(lumenvox_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lumenvox_core PUBLIC Lumenvox::Warnings)

# Generated manifests (packet tables, registries) are produced by the prepare
# tool from the user's own installation and land outside the source tree. The
# path is injected so no versioned data is ever committed.
set(LUMENVOX_DATA_DIR "${CMAKE_SOURCE_DIR}/var/prepared" CACHE PATH
    "Directory holding locally generated protocol and registry manifests")
target_compile_definitions(lumenvox_core PUBLIC LUMENVOX_DATA_DIR="${LUMENVOX_DATA_DIR}")

add_library(Lumenvox::Core ALIAS lumenvox_core)
