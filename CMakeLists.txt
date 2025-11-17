cmake_minimum_required(VERSION 3.13)
set(DEVKITPRO $ENV{DEVKITPRO})
set(CMAKE_CXX_STANDARD 23)

set(UNIQUE_ID 0x49FE2)
set(PRODUCT_CODE CTR-HB-SVSC)

list(APPEND CMAKE_MODULE_PATH "${DEVKITPRO}/cmake")
include(${DEVKITPRO}/cmake/3DS.cmake)
include(${DEVKITPRO}/cmake/Platform/Nintendo3DS.cmake)

project(SaveSync VERSION 1.0.0 LANGUAGES C CXX ASM)

add_subdirectory(cmake_timestamp)
include_directories(SYSTEM ${DEVKITPRO}/portlibs/3ds/include ext/include)

add_executable(${PROJECT_NAME}
	src/Util/LeakDetector.c

	ext/src/md5.c
	ext/src/clay_renderer_C2D.cpp

	src/Util/StringUtil.cpp
	src/Util/Profiler.cpp
	src/Util/Logger.cpp

	src/Theme.cpp
	src/Config.cpp

	src/Util/Keyboard.cpp
	src/Util/Worker.cpp
	src/Util/CURLEasy.cpp
	src/Util/SMDH.cpp

	src/FS/FSUtil.cpp
	src/FS/Archive.cpp
	src/FS/Directory.cpp
	src/FS/File.cpp

	src/Title.cpp
	src/TitleLoader.cpp

	src/Client/Client.cpp
	src/Client/RequestWorker.cpp
	src/Client/Info.cpp
	src/Client/Upload.cpp
	src/Client/Download.cpp

	src/UI/SettingsScreen.cpp
	src/UI/MainScreen.cpp

	src/Application.cpp
	src/main.cpp
)

dkp_target_generate_symbol_list(${PROJECT_NAME})
target_compile_options(${PROJECT_NAME} PRIVATE
	-Wall
	-Wno-psabi

	-pedantic-errors
	-Wall
	-Wextra
	-Wcast-qual
	-Wctor-dtor-privacy
	-Wdisabled-optimization
	-Wformat=2
	-Winit-self
	-Wlogical-op
	-Wmissing-include-dirs
	-Wnoexcept
	-Wold-style-cast
	-Woverloaded-virtual
	-Wredundant-decls
	-Wshadow
	-Wsign-conversion
	-Wsign-promo
	-Wstrict-null-sentinel
	-Wswitch-default
	-Wundef
	-Werror
	-Wtrampolines
	-Wmaybe-uninitialized
	
	-fstrict-flex-arrays=3
	-Wno-missing-field-initializers
	$<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions -fno-rtti>
)

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
	target_compile_options(${PROJECT_NAME} PRIVATE
		-O0
		-ggdb3
	)

	target_link_options(${PROJECT_NAME} PRIVATE
		-Wl,--wrap,malloc
		-Wl,--wrap,calloc
		-Wl,--wrap,realloc
		-Wl,--wrap,memalign
		-Wl,--wrap,strdup
		-Wl,--wrap,strndup
		
		-Wl,--wrap,free
		-Wl,--wrap,main
	)
else()
	target_compile_options(${PROJECT_NAME} PRIVATE
		-D_FORTIFY_SOURCE=1
	)
endif()

target_compile_definitions(${PROJECT_NAME} PUBLIC
	VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
	VERSION_MINOR=${PROJECT_VERSION_MINOR}
	VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

set(BUILD_TIME_HEADER "${CMAKE_BINARY_DIR}/build_time.h")
add_custom_command(
    TARGET ${PROJECT_NAME}
    PRE_LINK
    COMMAND ${CMAKE_COMMAND} -E echo "#pragma once" >  "${CMAKE_BINARY_DIR}/build_time.h"
    COMMAND ${CMAKE_COMMAND} -E echo "#define BUILD_TIME \"$(date -u +%Y-%m-%dT%H:%M:%SZ)\"" >> "${CMAKE_BINARY_DIR}/build_time.h"
    COMMENT "Regenerating build timestamp before link"
)

target_link_directories(${PROJECT_NAME} PRIVATE ${DEVKITPRO}/portlibs/3ds/lib)
target_include_directories(${PROJECT_NAME} PRIVATE include ${CMAKE_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} PRIVATE timestamp citro2d citro3d curl mbedtls mbedx509 mbedcrypto ctru z)

ctr_generate_smdh(${PROJECT_NAME}.smdh
	NAME "${PROJECT_NAME}"
	AUTHOR "coolguy1842"
	DESCRIPTION "v${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}.${PROJECT_VERSION_PATCH}"
	ICON "${PROJECT_SOURCE_DIR}/assets/icon.png"
)

ctr_create_3dsx(${PROJECT_NAME} SMDH ${PROJECT_NAME}.smdh)

if(CMAKE_BUILD_TYPE STREQUAL Release)
	if(EXISTS ${PROJECT_SOURCE_DIR}/makerom)
		set(CIA_FILE ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.cia)
		set(BANNER_FILE ${CMAKE_CURRENT_BINARY_DIR}/banner.bnr)

		set(MAKEROM_ARGS
			-f cia
			-o ${CIA_FILE}
			-rsf ${PROJECT_SOURCE_DIR}/assets/app.rsf
			-target t
			-exefslogo
			-elf ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
			-icon ${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh
			-major ${PROJECT_VERSION_MAJOR} -minor ${PROJECT_VERSION_MINOR} -micro ${PROJECT_VERSION_PATCH}
			-DAPP_TITLE="${PROJECT_NAME}" -DAPP_PRODUCT_CODE="${PRODUCT_CODE}" -DAPP_UNIQUE_ID="${UNIQUE_ID}"
		)

		set(MAKEROM_DEPS
			${PROJECT_SOURCE_DIR}/assets/app.rsf
			${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}
			${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}.smdh
		)

		if(EXISTS ${PROJECT_SOURCE_DIR}/bannertool)
			add_custom_command(
				OUTPUT ${BANNER_FILE}
				COMMAND ${PROJECT_SOURCE_DIR}/bannertool$<$<BOOL:${WIN32}>:.exe>
						makebanner
						-o ${BANNER_FILE}
						-i "${PROJECT_SOURCE_DIR}/assets/banner.png"
						-a "${PROJECT_SOURCE_DIR}/assets/audio.wav"
				DEPENDS
					${PROJECT_SOURCE_DIR}/assets/audio.wav
					${PROJECT_SOURCE_DIR}/assets/banner.png
			)

			list(APPEND MAKEROM_ARGS -banner ${BANNER_FILE})
			list(APPEND MAKEROM_DEPS ${BANNER_FILE})
		endif()


		add_custom_command(
			OUTPUT ${CIA_FILE}
			COMMAND ${PROJECT_SOURCE_DIR}/makerom$<$<BOOL:${WIN32}>:.exe> ${MAKEROM_ARGS}
			DEPENDS ${MAKEROM_DEPS}
		)

		add_custom_target(${PROJECT_NAME}_cia ALL DEPENDS ${CIA_FILE})
		install(FILES ${CMAKE_CURRENT_BINARY_DIR}/${BINARY_NAME}.cia DESTINATION cia COMPONENT ${PROJECT_NAME})
	endif()
endif()