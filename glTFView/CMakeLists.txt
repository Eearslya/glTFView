include(FetchContent)

add_executable(glTFView)
target_link_libraries(glTFView PRIVATE fastgltf stb Tsuki)

target_sources(glTFView PRIVATE
	Environment.cpp
	Files.cpp
	mikktspace.cpp
	Model.cpp
	glTFView.cpp)

add_custom_target(Run
	COMMAND glTFView
	DEPENDS glTFView
	WORKING_DIRECTORY ${CMAKE_SOURCE_DIR})