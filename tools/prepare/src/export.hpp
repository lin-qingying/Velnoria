#pragma once

#include <filesystem>
#include <string>

namespace velnoria::prepare {

struct ExportOptions {
    std::filesystem::path install_root;
    std::string version_id;
    std::filesystem::path server_jar;
    std::filesystem::path work_dir;
    std::filesystem::path out_dir;
    std::string java_command = "java";
};

// Runs the whole pipeline and returns a process exit code:
//   0 success, 3 missing input, 4 rejected input (hash/format), 5 generator failed.
int run_export(const ExportOptions& options);

}  // namespace velnoria::prepare
