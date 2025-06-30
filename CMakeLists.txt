cmake_minimum_required(VERSION 3.13)

set(DEVKITPRO $ENV{DEVKITPRO})

list(APPEND CMAKE_MODULE_PATH "${DEVKITPRO}/cmake")
include(${DEVKITPRO}/cmake/3DS.cmake)
include(${DEVKITPRO}/cmake/Platform/Nintendo3DS.cmake)

project(SaveSync LANGUAGES C CXX ASM)

add_executable(${PROJECT_NAME}
	source/smdh.cpp
	source/main.cpp
)

# Declare an asset target for the executable's RomFS (optional)
#dkp_add_asset_target(${PROJECT_NAME}_romfs romfs)

# Generate symbol list and map information (useful for debugging)
dkp_target_generate_symbol_list(${PROJECT_NAME})

# Generate a SMDH file for the executable
ctr_generate_smdh(${PROJECT_NAME}.smdh
	NAME "${PROJECT_NAME}"

	# Other options available:
	DESCRIPTION "Built with devkitARM & libctru"
	AUTHOR      "coolguy1842"
	#ICON        myIcon.png
)

# Specify that the executable is a 3DSX file
ctr_create_3dsx(${PROJECT_NAME}
	SMDH ${PROJECT_NAME}.smdh
	#ROMFS ${PROJECT_NAME}_romfs  # Uncomment this if using RomFS
)

# Options for code generation
target_compile_options(${PROJECT_NAME} PRIVATE
	# Common C/C++ options
	-Wall

	# C++ specific options
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
)

# Options for linking to libraries (example)
target_link_libraries(${PROJECT_NAME} PRIVATE
	citro2d
	citro3d
	ctru
)

# Source code files
target_include_directories(${PROJECT_NAME} PRIVATE
	include
)

# PICA200 shader source code files (example)
#ctr_add_shader_library(${PROJECT_NAME}_shaders
#	source/vertex_shader.pica
#)

# Converted texture (example)
#ctr_add_graphics_target(my_tex IMAGE
#	INPUTS gfx/my_tex.png
#)

# Texture atlas (example)
#ctr_add_graphics_target(my_atlas ATLAS
#	INPUTS gfx/sprite1.png gfx/sprite2.png
#)

# Install assets to RomFS (uncomment as needed)
#dkp_install_assets (${PROJECT_NAME}_romfs TARGETS
#	${PROJECT_NAME}_shaders
#	my_tex
#	my_atlas
#)