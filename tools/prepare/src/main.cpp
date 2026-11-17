// prepare: reads the user's own installation and produces the local cache the
// core library consumes. It never writes into the game directory, and it never
// ships any extracted data: everything lands in var/, which is ignored.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <vector>

#include "export.hpp"

namespace fs = std::filesystem;

namespace {

constexpr const char* kDefaultInstallRoot = "D:/GAMES/.minecraft";
constexpr const char* kDefaultVersionId = "26.3-Fabric";

struct Options {
    std::string command;
    fs::path install_root;
    std::string version_id;
    fs::path server_jar;
    fs::path work_dir;
    fs::path out_dir;
    std::string java_command;
};

void print_usage() {
    std::printf(
        "velnoria_prepare - local data pipeline\n"
        "\n"
        "Usage:\n"
        "  velnoria_prepare inspect [options]\n"
        "  velnoria_prepare export  [options]\n"
        "\n"
        "  inspect   Report which required files are present locally. Read only.\n"
        "  export    Run the data generator and write packets.tsv, block_states.tsv\n"
        "            and manifest.txt into the output directory.\n"
        "\n"
        "Options:\n"
        "  --root <dir>        Game install root. Default: %s or VELNORIA_MC_DIR.\n"
        "  --version <id>      Version directory name. Default: %s or VELNORIA_VERSION.\n"
        "  --server-jar <file> Official server jar. Default: var/inputs/server.jar.\n"
        "  --java <exe>        Java executable. Default: %s or VELNORIA_JAVA.\n"
        "  --gen-dir <dir>     Generator working directory. Default: var/build/<version>.\n"
        "  --out <dir>         Output directory. Default: var/prepared/<version>.\n"
        "\n"
        "Exit codes:\n"
        "  0  success\n"
        "  1  bad arguments\n"
        "  2  not implemented\n"
        "  3  required input missing\n"
        "  4  input rejected (hash or format mismatch)\n"
        "  5  generator or output failure\n",
        kDefaultInstallRoot, kDefaultVersionId, "java");
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

FileReport probe(std::string label, const fs::path& path, bool required) {
    FileReport item;
    item.label = std::move(label);
    item.path = path;
    item.required = required;
    std::error_code ec;
    item.present = fs::exists(path, ec) && !ec;
    if (item.present && fs::is_regular_file(path, ec)) {
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

    const std::vector<FileReport> items{
        probe("version json", version_dir / (options.version_id + ".json"), true),
        probe("client jar", version_dir / (options.version_id + ".jar"), true),
        probe("server jar", options.server_jar, true),
        probe("assets", options.install_root / "assets", true),
        probe("libraries", options.install_root / "libraries", false),
        probe("prepared", options.out_dir, false),
    };

    bool missing_required = false;
    for (const auto& item : items) {
        if (item.present) {
            std::printf("  [ ok ] %-14s %s (%llu bytes)\n", item.label.c_str(),
                        item.path.string().c_str(),
                        static_cast<unsigned long long>(item.size));
        } else {
            std::printf("  [%s] %-14s %s\n", item.required ? "MISS" : " -- ", item.label.c_str(),
                        item.path.string().c_str());
            if (item.required) {
                missing_required = true;
            }
        }
    }

    std::printf(
        "\nThe data generator stage needs the official server jar. This tool never\n"
        "downloads, bundles or proxies it; obtain it through official channels and\n"
        "place it at the path reported above.\n");

    return missing_required ? 3 : 0;
}

}  // namespace

int main(int argc, char** argv) {
    Options options;
    options.install_root = env_or("VELNORIA_MC_DIR", kDefaultInstallRoot);
    options.version_id = env_or("VELNORIA_VERSION", kDefaultVersionId);
    options.java_command = env_or("VELNORIA_JAVA", "java");
    options.server_jar = "var/inputs/server.jar";

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        const auto take_value = [&](fs::path& target) {
            if (i + 1 < argc) {
                target = argv[++i];
                return true;
            }
            return false;
        };

        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        }
        if (arg == "--root") {
            take_value(options.install_root);
            continue;
        }
        if (arg == "--version") {
            if (i + 1 < argc) {
                options.version_id = argv[++i];
            }
            continue;
        }
        if (arg == "--server-jar") {
            take_value(options.server_jar);
            continue;
        }
        if (arg == "--java") {
            if (i + 1 < argc) {
                options.java_command = argv[++i];
            }
            continue;
        }
        if (arg == "--gen-dir") {
            take_value(options.work_dir);
            continue;
        }
        if (arg == "--out") {
            take_value(options.out_dir);
            continue;
        }
        if (options.command.empty()) {
            options.command = arg;
            continue;
        }
        std::fprintf(stderr, "unexpected argument: %s\n", arg.c_str());
        return 1;
    }

    if (options.work_dir.empty()) {
        options.work_dir = fs::path("var") / "build" / options.version_id;
    }
    if (options.out_dir.empty()) {
        options.out_dir = fs::path("var") / "prepared" / options.version_id;
    }

    if (options.command.empty()) {
        print_usage();
        return 1;
    }

    if (options.command == "inspect") {
        return run_inspect(options);
    }
    if (options.command == "export") {
        velnoria::prepare::ExportOptions export_options;
        export_options.install_root = options.install_root;
        export_options.version_id = options.version_id;
        export_options.server_jar = options.server_jar;
        export_options.work_dir = options.work_dir;
        export_options.out_dir = options.out_dir;
        export_options.java_command = options.java_command;
        return velnoria::prepare::run_export(export_options);
    }

    std::fprintf(stderr, "unknown command: %s\n", options.command.c_str());
    print_usage();
    return 1;
}
