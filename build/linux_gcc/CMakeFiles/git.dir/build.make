# CMAKE generated file: DO NOT EDIT!
# Generated by "Unix Makefiles" Generator, CMake Version 3.3

# Delete rule output on recipe failure.
.DELETE_ON_ERROR:


#=============================================================================
# Special targets provided by cmake.

# Disable implicit rules so canonical targets will work.
.SUFFIXES:


# Remove some rules from gmake that .SUFFIXES does not remove.
SUFFIXES =

.SUFFIXES: .hpux_make_needs_suffix_list


# Suppress display of executed commands.
$(VERBOSE).SILENT:


# A target that is always out of date.
cmake_force:

.PHONY : cmake_force

#=============================================================================
# Set environment variables for the build.

# The shell in which to execute make rules.
SHELL = /bin/sh

# The CMake executable.
CMAKE_COMMAND = /usr/bin/cmake

# The command to remove a file.
RM = /usr/bin/cmake -E remove -f

# Escaping for special characters.
EQUALS = =

# The top-level source directory on which CMake was run.
CMAKE_SOURCE_DIR = /home/voges/git/calq

# The top-level build directory on which CMake was run.
CMAKE_BINARY_DIR = /home/voges/git/calq/build/linux_gcc

# Utility rule file for git.

# Include the progress variables for this target.
include CMakeFiles/git.dir/progress.make

CMakeFiles/git:
	/usr/bin/cmake -P /home/voges/git/calq/build/linux_gcc/git.cmake ADD_DEPENDENCIES /home/voges/git/calq/.git/logs/HEAD

git: CMakeFiles/git
git: CMakeFiles/git.dir/build.make

.PHONY : git

# Rule to build all files generated by this target.
CMakeFiles/git.dir/build: git

.PHONY : CMakeFiles/git.dir/build

CMakeFiles/git.dir/clean:
	$(CMAKE_COMMAND) -P CMakeFiles/git.dir/cmake_clean.cmake
.PHONY : CMakeFiles/git.dir/clean

CMakeFiles/git.dir/depend:
	cd /home/voges/git/calq/build/linux_gcc && $(CMAKE_COMMAND) -E cmake_depends "Unix Makefiles" /home/voges/git/calq /home/voges/git/calq /home/voges/git/calq/build/linux_gcc /home/voges/git/calq/build/linux_gcc /home/voges/git/calq/build/linux_gcc/CMakeFiles/git.dir/DependInfo.cmake --color=$(COLOR)
.PHONY : CMakeFiles/git.dir/depend

