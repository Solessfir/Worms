workspace "Worms"
    architecture "x64"
    configurations { "Debug", "Release" }
    startproject "Worms"

    outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

group "vendor"
    project "raylib"
        kind "StaticLib"
        language "C"
        staticruntime "off"
        warnings "off"

        targetdir ("bin/" .. outputdir .. "/%{prj.name}")
        objdir ("bin/" .. outputdir .. "/%{prj.name}/obj")
        files { "vendor/raylib/*.c" }

        includedirs {
            "vendor/raylib",
            "vendor/raylib/external/glfw/include",
        }

        filter "system:windows"
            systemversion "latest"
            -- Flags taken from: https://github.com/raysan5/raylib/wiki/Working-on-Windows
            defines { "PLATFORM_DESKTOP" }

            -- Disable: 'APIENTRY': macro redefinition"
            -- glfw3.h defines APIENTRY, then windows.h redefines it
            disablewarnings { "4005" }

        filter "system:linux"
            -- Flags taken from: https://github.com/raysan5/raylib/wiki/Working-on-GNU-Linux
            defines { "PLATFORM_DESKTOP", "GRAPHICS_API_OPENGL_33", "_GLFW_WAYLAND" }

            includedirs {
                "bin/wayland",
            }

            -- Bundled GLFW needs Wayland protocol headers generated from external/glfw/deps/wayland/*.xml
            prebuildcommands {
                '{MKDIR} "%{wks.location}/bin/wayland"',
                'for x in "%{wks.location}"/vendor/raylib/external/glfw/deps/wayland/*.xml; do ' ..
                'n=$$(basename "$$x" .xml); g="%{wks.location}/bin/wayland/$$n-client-protocol"; ' ..
                '[ -f "$$g.h" ] || wayland-scanner client-header "$$x" "$$g.h"; ' ..
                '[ -f "$$g-code.h" ] || wayland-scanner private-code "$$x" "$$g-code.h"; done'
            }

        filter "configurations:Debug"
            runtime "Debug"
            symbols "On"

        filter "configurations:Release"
            runtime "Release"
            optimize "On"
group ""

project "Worms"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    debugdir "%{wks.location}"
    objdir ("bin/" .. outputdir .. "/%{prj.name}/obj")
    postbuildcommands { '{COPYDIR} "%{wks.location}/assets" "%{cfg.targetdir}/assets"' }

    files {
        "src/**.h",
        "src/**.hpp",
        "src/**.cpp",
        "vendor/spdlog/**.h",
        "vendor/doctest/**.h",
    }

    includedirs { "src" }
    externalincludedirs { "vendor" }
    dependson { "raylib" }

    filter "system:windows"
        systemversion "latest"
        -- raylib and Win32 both define CloseWindow. NOGDI/NOUSER fix the compilation
        -- /FORCE:MULTIPLE picks raylib's at link time, /IGNORE:4006 disables the "second definition ignored" warning
        -- https://github.com/raysan5/raylib/issues/729
        defines { "NOGDI", "NOUSER" }
        links { "raylib", "winmm" }
        linkoptions { "/FORCE:MULTIPLE", "/IGNORE:4006" }
        buildoptions {
            -- Flags taken from: https://learn.microsoft.com/en-us/cpp/build/reference/compiler-options-listed-alphabetically?view=msvc-170
            "/EHsc",
            "/Zc:preprocessor",
            "/Zc:__cplusplus",
            "/permissive-",
            "/utf-8",
        }

    filter "system:linux"
        -- raylib's required system libs on desktop Linux (Wayland backend; GLFW dlopens libwayland/libxkbcommon)
        links { "raylib", "GL", "m", "pthread", "dl", "rt" }
        buildoptions {
            -- Flags taken from: https://gcc.gnu.org/onlinedocs/gcc/Warning-Options.html
            "-Wall",
            "-Wextra",
            "-Wshadow",
            "-Wconversion",
            "-Wno-unused-parameter",
        }

    filter "configurations:Debug"
        kind "ConsoleApp"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter { "system:windows", "configurations:Release" }
        -- WindowedApp sets /SUBSYSTEM:WINDOWS which expects WinMain, but our entry is int main()
        -- Redirect the entry point back to the CRT's main() startup. Or #if WIN WinMain() #else, etc.
        -- https://github.com/raysan5/raylib/issues/691
        linkoptions { "/ENTRY:mainCRTStartup" }

    filter "configurations:Release"
        kind "WindowedApp"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "Off"

project "Tests"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin/" .. outputdir .. "/%{prj.name}/obj")

    files { "tests/**.h", "tests/**.cpp", "src/**.h", "src/**.hpp", "src/**.cpp" }
    removefiles { "src/Main.cpp" }

    includedirs { "src" }
    externalincludedirs { "vendor" }
    dependson { "raylib" }

    filter "system:windows"
        systemversion "latest"
        -- raylib and Win32 both define CloseWindow. NOGDI/NOUSER fix the compilation
        -- /FORCE:MULTIPLE picks raylib's at link time, /IGNORE:4006 mutes the "second definition ignored" warning
        -- https://github.com/raysan5/raylib/issues/729
        defines { "NOGDI", "NOUSER" }
        links { "raylib", "winmm" }
        linkoptions { "/FORCE:MULTIPLE", "/IGNORE:4006" }
        buildoptions {
            "/EHsc",
            "/Zc:preprocessor",
            "/Zc:__cplusplus",
            "/permissive-",
            "/utf-8",
        }

    filter "system:linux"
        links { "raylib", "GL", "m", "pthread", "dl", "rt" }
        buildoptions {
            "-Wall",
            "-Wextra",
            "-Wno-unused-parameter",
        }

    filter "configurations:Debug"
        defines { "DEBUG" }
        runtime "Debug"
        symbols "On"

    filter "configurations:Release"
        defines { "RELEASE" }
        runtime "Release"
        optimize "On"
        symbols "Off"
