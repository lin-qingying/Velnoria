// prepare: reads the user's own installation and produces the local cache the
// core library consumes. This binary never writes into the game directory and
// never ships any extracted asset.
//
// Stage S0 of docs/design/vertical-slice-01.md. Only "inspect" is implemented;
// "export" is scaffolded and intentionally returns a distinct exit code so CI
// and the launcher can tell "not implemented" from "failed".

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr const char* kDefaultInstallRoot = "D:/GAMES/.minecraft";
constexpr const char* kDefaultVersionId = "26.3-Fabric";

struct Options {
    std::string command;
    fs::path install_root;
    std::string version_id;
};

void print_usage() {
    std::printf(
        "lumenvox_prepare - local data pipeline\n"
        "\n"
        "Usage:\n"
        "  lumenvox_prepare inspect [--root <dir>] [--version <id>]\n"
        "  lumenvox_prepare export  [--root <dir>] [--version <id>] [--out <dir>]\n"
        "\n"
        "  inspect   Report which required files are present locally. Read only.\n"
        "  export    Produce packet table, registry and asset manifests (not implemented).\n"
        "\n"
        "Options:\n"
        "  --root <dir>     Game install root. Default: %s, or LUMENVOX_MC_DIR.\n"
        "  --version <id>   Version directory name. Default: %s.\n"
        "\n"
        "Exit codes:\n"
        "  0  success\n"
        "  1  bad arguments\n"
        "  2  not implemented\n"
        "  3  required input missing\n"
        "  4  input rejected (hash or version mismatch)\n",
        kDefaultInstallRoot, kDefaultVersionId);
}

std::string env_or(const char* name, const char* fallback) {
    const char* value = std::getenv(name);
    return value != nullptr && value[0] != '\0' ? std::string(value) : std::string(fallback);
}

struct FileReport {
    std::string label;
    fs::path path;
    bool present = false;
    std::uintmax_t size = 0;
    bool required = true;
};

void report(const FileReport& item) {
    if (item.present) {
        std::printf("  [ ok ] %-14s %s (%llu bytes)\n", item.label.c_str(),
                    item.path.string().c_str(),
                    static_cast<unsigned long long>(item.size));
    } else {
        std::printf("  [%s] %-14s %s\n", item.required ? "MISS" : " -- ", item.label.c_str(),
                    item.path.string().c_str());
    }
}

FileReport probe(const char* label, const fs::path& path, bool required) {
    FileReport item;
    item.label = label;
    item.path = path;
    item.required = required;
    std::error_code ec;
    item.present = fs::exists(path, ec) && !ec;
    if (item.present) {
        item.size = fs::file_size(path, ec);
        if (ec) {
            item.size = 0;
        }
    }
    return item;
}

int run_inspect(const Options& options) {
    const fs::path version_dir = options.install_root / "versions" / options.version_id;

    std::printf("install root : %s\n", options.install_root.string().c_str());
    std::printf("version      : %s\n", options.version_id.c_str());
    std::printf("version dir  : %s\n\n", version_dir.string().c_str());

    // The client jar is named after the version directory; for a Fabric install
    // it still holds the unmodified client image.
    const std::vector<FileReport> items{
        probe("version json", version_dir / (options.version_id + ".json"), true),
        probe("client jar", version_dir / (options.version_id + ".jar"), true),
        probe("server jar", version_dir / "server.jar", true),
        probe("assets index", options.install_root / "assets", true),
        probe("libraries", options.install_root / "libraries", false),
    };

    bool missing_required = false;
    for (const auto& item : items) {
        report(item);
        if (item.required && !item.present) {
            missing_required = true;
        }
    }

    std::printf(
        "\nNote: the data generator stage needs the official server jar. It is not\n"
        "downloaded, bundled or proxied by this tool; obtain it through official\n"
        "channels and place it at the path reported above.\n");

    return missing_required ? 3 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    options.install_root = env_or("LUMENVOX_MC_DIR", kDefaultInstallRoot);
    options.version_id = env_or("LUMENVOX_VERSION", kDefaultVersionId);

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--root" && i + 1 < argc) {
            options.install_root = argv[++i];
            continue;
        }
        if (arg == "--version" && i + 1 < argc) {
            options.version_id = argv[++i];
            continue;
        }
        if (arg == "--out" && i + 1 < argc) {
            ++i;  // accepted but unused until export lands
            continue;
        }
        if (options.command.empty()) {
            options.command = arg;
            continue;
        }
        std::fprintf(stderr, "unexpected argument: %s\n", arg.c_str());
        return 1;
    }

    if (options.command.empty()) {
        print_usage();
        return 1;
    }

    if (options.command == "inspect") {
        return run_inspect(options);
    }
    if (options.command == "export") {
        std::fprintf(stderr,
                     "export is not implemented yet. Planned outputs:\n"
                     "  packets.bin      per state/bound/direction id table\n"
                     "  registries.bin   blocks, items, biomes, dimension types, tags\n"
                     "  assets.bin       blockstates, models, textures (colour reference only)\n"
                     "  manifest.toml    version id, input hashes, generation timestamp\n");
        return 2;
    }

    std::fprintf(stderr, "unknown command: %s\n", options.command.c_str());
    print_usage();
    return 1;
}
