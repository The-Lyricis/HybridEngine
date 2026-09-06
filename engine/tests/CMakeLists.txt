add_executable(HybridTests hybrid_tests.cpp)
set_target_properties(HybridTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "Tests")
target_link_libraries(HybridTests PRIVATE HybridEngine)
target_include_directories(HybridTests PRIVATE $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)
set(HYBRID_TEST_DATA_DIR "${CMAKE_CURRENT_BINARY_DIR}/data/player")
file(MAKE_DIRECTORY "${HYBRID_TEST_DATA_DIR}")
configure_file(data/player/PlayerTest.hyproj "${HYBRID_TEST_DATA_DIR}/PlayerTest.hyproj" COPYONLY)
configure_file(data/player/NoScene.hyproj "${HYBRID_TEST_DATA_DIR}/NoScene.hyproj" COPYONLY)
file(COPY data/player/Assets DESTINATION "${HYBRID_TEST_DATA_DIR}")
target_compile_definitions(HybridTests PRIVATE
    HYBRID_TEST_PROJECT_PATH="${HYBRID_TEST_DATA_DIR}/PlayerTest.hyproj")

add_test(NAME HybridTests COMMAND HybridTests)

add_executable(HybridEditorServiceTests
    editor_service_tests.cpp
    ${ENGINE_ROOT_DIR}/source/editor/services/asset/editor_resource_system.cpp
    ${ENGINE_ROOT_DIR}/source/editor/services/import/import_manager.cpp
    ${ENGINE_ROOT_DIR}/source/editor/services/import/audio_importer.cpp
    ${ENGINE_ROOT_DIR}/source/editor/services/import/mesh_importer.cpp
    ${ENGINE_ROOT_DIR}/source/editor/services/import/scene_importer.cpp
    ${ENGINE_ROOT_DIR}/source/editor/services/import/texture_importer.cpp)
set_target_properties(HybridEditorServiceTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "Tests")
target_link_libraries(HybridEditorServiceTests PRIVATE
    HybridEngine stb tinyobjloader nlohmann_json::nlohmann_json)
target_include_directories(HybridEditorServiceTests PRIVATE
    $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)
add_test(NAME HybridEditorServiceTests COMMAND HybridEditorServiceTests)

if(WIN32)
    add_executable(HybridRenderLifecycleTests render_lifecycle_tests.cpp)
    set_target_properties(HybridRenderLifecycleTests PROPERTIES CXX_STANDARD 17 CXX_STANDARD_REQUIRED ON FOLDER "Tests")
    target_link_libraries(HybridRenderLifecycleTests PRIVATE HybridEngine glad)
    target_include_directories(HybridRenderLifecycleTests PRIVATE $<BUILD_INTERFACE:${ENGINE_ROOT_DIR}/source>)
    target_compile_definitions(HybridRenderLifecycleTests PRIVATE
        HYBRID_TEST_PROJECT_PATH="${HYBRID_TEST_DATA_DIR}/PlayerTest.hyproj")
    add_test(NAME HybridRenderLifecycleTests COMMAND HybridRenderLifecycleTests)
endif()

if(TARGET HybridPlayer)
    set(HYBRID_PLAYER_TEST_PROJECT "${HYBRID_TEST_DATA_DIR}/PlayerTest.hyproj")
    add_test(NAME HybridPlayer.Headless
        COMMAND HybridPlayer --project "${HYBRID_PLAYER_TEST_PROJECT}" --headless --max-frames 2)
    add_test(NAME HybridPlayer.SceneOverride
        COMMAND HybridPlayer --project "${HYBRID_TEST_DATA_DIR}/NoScene.hyproj" --scene asset:Startup.scene --headless --max-frames 1)
    add_test(NAME HybridPlayer.MissingProject
        COMMAND HybridPlayer --project "${HYBRID_TEST_DATA_DIR}/Missing.hyproj" --headless --max-frames 1)
    add_test(NAME HybridPlayer.MissingScene
        COMMAND HybridPlayer --project "${HYBRID_TEST_DATA_DIR}/NoScene.hyproj" --headless --max-frames 1)
    set_tests_properties(HybridPlayer.MissingProject HybridPlayer.MissingScene PROPERTIES WILL_FAIL TRUE)
endif()
