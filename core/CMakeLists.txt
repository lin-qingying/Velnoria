add_library(velnoria_core STATIC
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

target_include_directories(velnoria_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(velnoria_core PUBLIC Velnoria::Warnings)

# Generated manifests (packet tables, registries) are produced by the prepare
# tool from the user's own installation and land outside the source tree. The
# path is injected so no versioned data is ever committed.
set(VELNORIA_DATA_DIR "${CMAKE_SOURCE_DIR}/var/prepared" CACHE PATH
    "Directory holding locally generated protocol and registry manifests")
target_compile_definitions(velnoria_core PUBLIC VELNORIA_DATA_DIR="${VELNORIA_DATA_DIR}")

add_library(Velnoria::Core ALIAS velnoria_core)
