import os
import shutil
import subprocess
import platform
import argparse

def main():
    """
    Builds the singularity library with options for build type, code coverage,
    documentation checks, and clang-tidy analysis.

    Examples:
      - Build a standard release version:
        python build.py

      - Build a debug version:
        python build.py --build-type debug

      - Build a debug version with clang-tidy enabled:
        python build.py -b debug --check-clang-tidy

      - Build a debug version with all checks enabled:
        python build.py -b debug --check-documentation --check-clang-tidy
    """
    parser = argparse.ArgumentParser(description="Build script for the singularity library.")
    parser.add_argument(
        '-b', '--build-type',
        type=str,
        choices=['release', 'debug'],
        default='release',
        help='Specify the build type (release or debug). Default is release.'
    )
    parser.add_argument(
        '--check-documentation',
        action='store_true',
        help='Enable documentation check (sets CHECK_DOCUMENTATION=ON).'
    )
    parser.add_argument(
        '--check-clang-tidy',
        action='store_true',
        help='Enable clang-tidy checks (sets CHECK_CLANG_TIDY=ON).'
    )
    args = parser.parse_args()

    # Use the capitalized version for CMAKE_BUILD_TYPE
    cmake_build_type = args.build_type.capitalize()

    print(f"Building {cmake_build_type} version of singularity library.")
    if args.check_documentation:
        print("Documentation check enabled.")
    if args.check_clang_tidy:
        print("Clang-Tidy check enabled.")

    # Get operating system and architecture
    os_name = platform.system()
    architecture = platform.machine()

    # Use Ninja generator for all hosts
    cmake_generator = "Ninja"
    cmake_extra_args = []

    print(f"Detected {os_name}. Using Ninja generator for all hosts.")
    print("Please ensure that Ninja is installed and available in your system's PATH.")

    if os_name == "Windows":
        # Add specific compiler and make program for Windows when using Ninja
        cmake_extra_args.extend([
            '-DCMAKE_C_COMPILER=clang.exe',
            '-DCMAKE_CXX_COMPILER=clang++.exe',
            '-DCMAKE_MAKE_PROGRAM=ninja.exe'
        ])

        print("Ensure clang, clang++, and ninja are in your PATH or run from a suitable environment.")
    elif os_name == "Linux":
        # Explicitly set Ninja as the make program on Linux for clarity with the Ninja generator
        os.environ["CC"] = "clang"
        os.environ["CXX"] = "clang++"
        cmake_extra_args.append('-DCMAKE_MAKE_PROGRAM=ninja')

    # Add optional CMake flags based on arguments
    if args.check_documentation:
        cmake_extra_args.append('-DCHECK_DOCUMENTATION=ON')

    if args.check_clang_tidy:
        cmake_extra_args.append('-DCHECK_CLANG_TIDY=ON')

    # Create the build directory name based on build type, OS, and architecture
    build_dir = f"build/{args.build_type.lower()}_{os_name}_{architecture}"

    print(f"Using build directory: {build_dir}")

    # Create the build directory structure if it does not exist
    if not os.path.exists(build_dir):
        os.makedirs(build_dir, exist_ok=True)

    # Change into the build directory to run commands
    original_cwd = os.getcwd()
    os.chdir(build_dir)

    try:
        # Construct the CMake command with generator, build type, and extra arguments
        cmake_command = [
            "cmake",
            f"-DCMAKE_BUILD_TYPE={cmake_build_type}",
            "-G", cmake_generator
        ]
        cmake_command.extend(cmake_extra_args)
        cmake_command.append("../..") # Source directory relative to the build directory

        print(f"Running CMake command: {' '.join(cmake_command)}")
        subprocess.run(cmake_command, check=True)

        # Move compile_commands file for use with clangd
        compile_commands_path = "compile_commands.json"
        if os.path.exists(compile_commands_path):
            shutil.copyfile(compile_commands_path, os.path.join(original_cwd, "compile_commands.json"))
            print(f"Copied {compile_commands_path} to {original_cwd}")

        # Compile code using CMake's build tool mode
        print(f"Running CMake build command: cmake --build . --parallel")
        subprocess.run(["cmake", "--build", ".", "--parallel"], check=True)

    except subprocess.CalledProcessError as e:
        print(f"Error during build process: {e}")
    finally:
        # Always change back to the original directory
        os.chdir(original_cwd)

if __name__ == "__main__":
    main()
