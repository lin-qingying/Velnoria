using System;
using System.IO;
using UnrealBuildTool;

// Links the standalone core library. The core is built by CMake, not by UBT, so
// its location is injected through VELNORIA_CORE_ROOT (or the default relative
// layout ../build). UE is never bundled into this repository and neither is the
// core build output.
public class VelnoriaPlugin : ModuleRules
{
    public VelnoriaPlugin(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "RenderCore",
            "RHI",
            "Projects",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            "Niagara",
        });

        string coreRoot = Environment.GetEnvironmentVariable("VELNORIA_CORE_ROOT");
        if (string.IsNullOrEmpty(coreRoot))
        {
            coreRoot = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "..", "core"));
        }

        string includeDir = Path.Combine(coreRoot, "include");
        if (Directory.Exists(includeDir))
        {
            PublicIncludePaths.Add(includeDir);
        }

        string libDir = Path.Combine(coreRoot, "..", "build", "core");
        string libName = (Target.Platform == UnrealTargetPlatform.Win64) ? "velnoria_core.lib" : "libvelnoria_core.a";
        string libPath = Path.Combine(libDir, libName);
        if (File.Exists(libPath))
        {
            PublicAdditionalLibraries.Add(libPath);
        }
        else
        {
            // Fail loudly instead of silently producing an editor module that
            // cannot do anything: the slice cannot proceed without the core.
            throw new BuildException(
                "velnoria_core not found at " + libPath +
                ". Build the core with CMake first and set VELNORIA_CORE_ROOT.");
        }
    }
}
