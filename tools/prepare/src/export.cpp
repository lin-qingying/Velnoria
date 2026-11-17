#include "export.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <optional>
#include <sstream>
#include <vector>

#include "velnoria/core/json.hpp"
#include "velnoria/core/sha1.hpp"

namespace fs = std::filesystem;

namespace velnoria::prepare {
namespace {

constexpr std::size_t kHashChunk = 1u << 20;

struct PacketRow {
    std::string state;
    std::string bound;
    std::string name;
    std::int32_t id = 0;
};

struct StateRow {
    std::uint32_t id = 0;
    std::string block;
    std::string properties;
};

std::optional<std::string> read_text_file(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    return buffer.str();
}

std::optional<Digest20> hash_file(const fs::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    Sha1 hasher;
    std::vector<std::uint8_t> chunk(kHashChunk);
    while (stream) {
        stream.read(reinterpret_cast<char*>(chunk.data()), static_cast<std::streamsize>(chunk.size()));
        const auto read = static_cast<std::size_t>(stream.gcount());
        if (read == 0) {
            break;
        }
        hasher.update(std::span<const std::uint8_t>(chunk.data(), read));
    }
    return hasher.final();
}

std::string to_hex(const Digest20& digest) {
    static constexpr char kDigits[] = "0123456789abcdef";
    std::string out;
    out.reserve(40);
    for (auto byte : digest) {
        out.push_back(kDigits[byte >> 4]);
        out.push_back(kDigits[byte & 0x0Fu]);
    }
    return out;
}

bool expect_hash(const fs::path& path, const std::string& expected, const char* label) {
    auto digest = hash_file(path);
    if (!digest.has_value()) {
        std::fprintf(stderr, "  [MISS] %s: %s\n", label, path.string().c_str());
        return false;
    }
    const std::string actual = to_hex(*digest);
    if (actual != expected) {
        std::fprintf(stderr, "  [BAD ] %s sha1\n         expected %s\n         actual   %s\n", label,
                     expected.c_str(), actual.c_str());
        return false;
    }
    std::printf("  [ ok ] %s sha1 %s\n", label, actual.c_str());
    return true;
}

int state_rank(const std::string& state) {
    if (state == "handshake") {
        return 0;
    }
    if (state == "status") {
        return 1;
    }
    if (state == "login") {
        return 2;
    }
    if (state == "configuration") {
        return 3;
    }
    if (state == "play") {
        return 4;
    }
    return 5;
}

bool collect_packets(const json::Value& root, std::vector<PacketRow>& rows) {
    for (const auto& [state, bounds] : root.members()) {
        if (!bounds.is_object()) {
            return false;
        }
        for (const auto& [bound, packets] : bounds.members()) {
            if (!packets.is_object()) {
                return false;
            }
            for (const auto& [name, body] : packets.members()) {
                const json::Value* id = body.find("protocol_id");
                if (id == nullptr || !id->is_number()) {
                    return false;
                }
                rows.push_back(PacketRow{state, bound, name, static_cast<std::int32_t>(id->as_int64())});
            }
        }
    }
    return true;
}

bool collect_block_states(const json::Value& root, std::vector<std::optional<StateRow>>& slots) {
    std::uint32_t max_id = 0;
    std::vector<StateRow> pending;

    for (const auto& [block_name, block] : root.members()) {
        const json::Value* states = block.find("states");
        if (states == nullptr || !states->is_array()) {
            return false;
        }
        for (const auto& state : states->items()) {
            const json::Value* id = state.find("id");
            if (id == nullptr || !id->is_number()) {
                return false;
            }
            StateRow row;
            row.id = static_cast<std::uint32_t>(id->as_int64());
            row.block = block_name;

            if (const json::Value* properties = state.find("properties");
                properties != nullptr && properties->is_object()) {
                bool first = true;
                for (const auto& [key, value] : properties->members()) {
                    if (!first) {
                        row.properties.push_back(',');
                    }
                    first = false;
                    row.properties.append(key);
                    row.properties.push_back('=');
                    row.properties.append(value.is_string() ? value.as_string() : std::string());
                }
            }

            max_id = std::max(max_id, row.id);
            pending.push_back(std::move(row));
        }
    }

    slots.assign(static_cast<std::size_t>(max_id) + 1u, std::nullopt);
    for (auto& row : pending) {
        const auto index = static_cast<std::size_t>(row.id);
        if (slots[index].has_value()) {
            std::fprintf(stderr, "duplicate block state id %u\n", row.id);
            return false;
        }
        slots[index] = std::move(row);
    }

    for (std::size_t i = 0; i < slots.size(); ++i) {
        if (!slots[i].has_value()) {
            std::fprintf(stderr, "block state id %zu is missing: the table has a hole\n", i);
            return false;
        }
    }
    return true;
}

bool write_file(const fs::path& path, const std::string& content) {
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(content.data(), static_cast<std::streamsize>(content.size()));
    return stream.good();
}

int run_generator(const ExportOptions& options) {
    std::error_code ec;
    fs::create_directories(options.work_dir, ec);
    if (ec) {
        std::fprintf(stderr, "cannot create work dir %s\n", options.work_dir.string().c_str());
        return 5;
    }

    const fs::path previous = fs::current_path();
    fs::current_path(options.work_dir, ec);
    if (ec) {
        std::fprintf(stderr, "cannot enter work dir\n");
        return 5;
    }

    std::string inner = "\"" + options.java_command + "\"";
    inner += " -DbundlerMainClass=net.minecraft.data.Main -jar \"";
    inner += options.server_jar.string();
    inner += "\" --reports";

    // cmd.exe strips the outer pair of quotes when the line starts with one, so
    // the whole command is wrapped: without this a quoted java path breaks.
    const std::string command = "\"" + inner + "\"";

    std::printf("running data generator: %s\n", inner.c_str());
    const int result = std::system(command.c_str());
    fs::current_path(previous, ec);

    if (result != 0) {
        std::fprintf(stderr, "data generator exited with %d\n", result);
        return 5;
    }
    return 0;
}

}  // namespace

int run_export(const ExportOptions& incoming) {
    // Resolve every path before the generator changes the working directory,
    // otherwise a relative server jar path silently resolves against the
    // generator's scratch directory.
    ExportOptions options = incoming;
    options.install_root = fs::absolute(incoming.install_root);
    options.server_jar = fs::absolute(incoming.server_jar);
    options.work_dir = fs::absolute(incoming.work_dir);
    options.out_dir = fs::absolute(incoming.out_dir);

    const fs::path version_dir = options.install_root / "versions" / options.version_id;
    const fs::path version_json = version_dir / (options.version_id + ".json");
    const fs::path client_jar = version_dir / (options.version_id + ".jar");

    std::printf("export %s\n\n", options.version_id.c_str());

    auto manifest_text = read_text_file(version_json);
    if (!manifest_text.has_value()) {
        std::fprintf(stderr, "missing version json: %s\n", version_json.string().c_str());
        return 3;
    }
    auto manifest_json = json::parse(*manifest_text);
    if (!manifest_json.ok()) {
        std::fprintf(stderr, "cannot parse version json: %s\n",
                     manifest_json.error().detail.c_str());
        return 4;
    }

    const json::Value* downloads = manifest_json.value().find("downloads");
    const json::Value* client = downloads != nullptr ? downloads->find("client") : nullptr;
    const json::Value* server = downloads != nullptr ? downloads->find("server") : nullptr;
    if (client == nullptr || server == nullptr) {
        std::fprintf(stderr, "version json lacks download entries\n");
        return 4;
    }
    const json::Value* client_sha = client->find("sha1");
    const json::Value* server_sha = server->find("sha1");
    if (client_sha == nullptr || server_sha == nullptr) {
        std::fprintf(stderr, "download entries lack sha1\n");
        return 4;
    }

    if (!fs::exists(client_jar)) {
        std::fprintf(stderr, "missing client jar: %s\n", client_jar.string().c_str());
        return 3;
    }
    if (!fs::exists(options.server_jar)) {
        std::fprintf(stderr, "missing server jar: %s\n", options.server_jar.string().c_str());
        return 3;
    }

    const bool client_ok = expect_hash(client_jar, client_sha->as_string(), "client jar");
    const bool server_ok = expect_hash(options.server_jar, server_sha->as_string(), "server jar");
    if (!client_ok || !server_ok) {
        std::fprintf(stderr, "input rejected: hash mismatch, nothing written\n");
        return 4;
    }

    const int generator_status = run_generator(options);
    if (generator_status != 0) {
        return generator_status;
    }

    const fs::path reports = options.work_dir / "generated" / "reports";
    auto packets_text = read_text_file(reports / "packets.json");
    auto blocks_text = read_text_file(reports / "blocks.json");
    if (!packets_text.has_value() || !blocks_text.has_value()) {
        std::fprintf(stderr, "generator output missing under %s\n", reports.string().c_str());
        return 5;
    }

    auto packets_json = json::parse(*packets_text);
    if (!packets_json.ok()) {
        std::fprintf(stderr, "cannot parse packets.json: %s\n",
                     packets_json.error().detail.c_str());
        return 5;
    }
    auto blocks_json = json::parse(*blocks_text);
    if (!blocks_json.ok()) {
        std::fprintf(stderr, "cannot parse blocks.json: %s\n", blocks_json.error().detail.c_str());
        return 5;
    }

    std::vector<PacketRow> packets;
    if (!collect_packets(packets_json.value(), packets)) {
        std::fprintf(stderr, "packets.json has an unexpected shape\n");
        return 5;
    }
    std::sort(packets.begin(), packets.end(), [](const PacketRow& a, const PacketRow& b) {
        if (state_rank(a.state) != state_rank(b.state)) {
            return state_rank(a.state) < state_rank(b.state);
        }
        if (a.bound != b.bound) {
            return a.bound < b.bound;
        }
        return a.id < b.id;
    });

    std::vector<std::optional<StateRow>> states;
    if (!collect_block_states(blocks_json.value(), states)) {
        std::fprintf(stderr, "blocks.json has an unexpected shape or holes\n");
        return 5;
    }

    std::string packets_tsv = "# velnoria packets v1\n";
    for (const auto& row : packets) {
        packets_tsv += row.state;
        packets_tsv.push_back('\t');
        packets_tsv += row.bound;
        packets_tsv.push_back('\t');
        packets_tsv += row.name;
        packets_tsv.push_back('\t');
        packets_tsv += std::to_string(row.id);
        packets_tsv.push_back('\n');
    }

    std::string states_tsv = "# velnoria block_states v1\n";
    for (const auto& slot : states) {
        states_tsv += std::to_string(slot->id);
        states_tsv.push_back('\t');
        states_tsv += slot->block;
        states_tsv.push_back('\t');
        states_tsv += slot->properties;
        states_tsv.push_back('\n');
    }

    std::error_code ec;
    fs::create_directories(options.out_dir, ec);
    if (ec) {
        std::fprintf(stderr, "cannot create output dir\n");
        return 5;
    }

    const fs::path packets_path = options.out_dir / "packets.tsv";
    const fs::path states_path = options.out_dir / "block_states.tsv";
    if (!write_file(packets_path, packets_tsv) || !write_file(states_path, states_tsv)) {
        std::fprintf(stderr, "cannot write output files\n");
        return 5;
    }

    const auto packets_digest = hash_file(packets_path);
    const auto states_digest = hash_file(states_path);
    if (!packets_digest.has_value() || !states_digest.has_value()) {
        return 5;
    }

    std::string manifest;
    manifest += "format = 1\n";
    manifest += "version = " + options.version_id + "\n";
    manifest += "client_sha1 = " + client_sha->as_string() + "\n";
    manifest += "server_sha1 = " + server_sha->as_string() + "\n";
    manifest += "packet_count = " + std::to_string(packets.size()) + "\n";
    manifest += "block_state_count = " + std::to_string(states.size()) + "\n";
    manifest += "packets_sha1 = " + to_hex(*packets_digest) + "\n";
    manifest += "block_states_sha1 = " + to_hex(*states_digest) + "\n";

    if (!write_file(options.out_dir / "manifest.txt", manifest)) {
        std::fprintf(stderr, "cannot write manifest\n");
        return 5;
    }

    std::printf("\n  packets        %zu\n", packets.size());
    std::printf("  block states   %zu\n", states.size());
    std::printf("  output         %s\n", options.out_dir.string().c_str());
    std::printf("  packets_sha1   %s\n", to_hex(*packets_digest).c_str());
    std::printf("  states_sha1    %s\n", to_hex(*states_digest).c_str());
    return 0;
}

}  // namespace velnoria::prepare
