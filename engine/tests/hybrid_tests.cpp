#include <atomic>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <future>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include "runtime/core/job/job_system.h"
#include "runtime/core/log/log_system.h"
#include "runtime/core/time/frame_clock.h"
#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/asset_registry.h"
#include "runtime/modules/input/input_layer.h"
#include "runtime/modules/project/project_file.h"
#include "runtime/modules/render/rhi/render_device.h"
#include "runtime/modules/render/runtime/pipeline/render_graph.h"
#include "runtime/modules/render/runtime/pipeline/render_graph_resources.h"
#include "runtime/modules/render/runtime/pipeline/render_pipeline.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/scene_serializer.h"
#include "runtime/runtime/engine.h"

namespace
{
    int failures = 0;

    void expect(bool condition, const char* expression, int line)
    {
        if (!condition)
        {
            std::cerr << "FAILED line " << line << ": " << expression << '\n';
            ++failures;
        }
    }

#define EXPECT(expression) expect(static_cast<bool>(expression), #expression, __LINE__)

    class NullVfs final : public Hybrid::IVirtualFileSystem
    {
    public:
        void mount(const std::string&, const std::filesystem::path&, int) override {}
        bool exists(const std::string&) const override { return false; }
        std::optional<std::filesystem::path> resolve(const std::string&) const override { return std::nullopt; }
        std::optional<std::filesystem::path> resolveForWrite(const std::string&) const override { return std::nullopt; }
        std::vector<char> readAll(const std::string&) const override { return {}; }
    };

    Hybrid::AssetMetadata metadata(uint64_t id, const char* path)
    {
        Hybrid::AssetMetadata value{};
        value.id = Hybrid::AssetID::FromRaw(id);
        value.type = Hybrid::AssetType::Script;
        value.source_path = path;
        value.is_valid = true;
        return value;
    }

    void testJobSystem()
    {
        Hybrid::JobSystem jobs;
        EXPECT(jobs.initialize({2}));
        auto answer = jobs.submit([] { return 42; });
        auto exceptional = jobs.submit([]() -> int { throw std::runtime_error("expected"); });
        EXPECT(answer.get() == 42);
        bool exception_seen = false;
        try { (void)exceptional.get(); }
        catch (const std::runtime_error&) { exception_seen = true; }
        EXPECT(exception_seen);
        jobs.waitIdle();
        jobs.shutdown();
        jobs.shutdown();
        bool rejected = false;
        try { (void)jobs.submit([] {}); }
        catch (const std::runtime_error&) { rejected = true; }
        EXPECT(rejected);
    }

    void testFrameClock()
    {
        Hybrid::FrameClock clock;
        clock.configure({60.0f, 4});
        int calls = 0;
        auto partial = clock.advance(1.0f / 120.0f, [&calls](float) { ++calls; });
        EXPECT(partial.fixed_steps == 0);
        auto complete = clock.advance(1.0f / 120.0f, [&calls](float) { ++calls; });
        EXPECT(complete.fixed_steps == 1);
        EXPECT(calls == 1);
        const auto capped = clock.advance(1.0f, [&calls](float) { ++calls; });
        EXPECT(capped.fixed_steps == 4);
        EXPECT(capped.dropped_time);
    }

    void testBufferedLog()
    {
        const auto log_path = std::filesystem::temp_directory_path() / "hybrid_buffered_log_test.log";
        Hybrid::LogSystem::Config config{};
        const std::string log_path_string = log_path.string();
        config.logfile = log_path_string.c_str();
        config.buffered_entries = 3;
        Hybrid::LogSystem::initialize(config);
        Hybrid::LogSystem::core()->info("entry-one");
        std::thread worker([] { Hybrid::LogSystem::client()->warn("entry-two"); });
        worker.join();
        Hybrid::LogSystem::core()->error("entry-three");
        Hybrid::LogSystem::core()->debug("entry-four");

        const auto snapshot = Hybrid::LogSystem::bufferedEntries();
        EXPECT(snapshot.entries.size() == 3);
        EXPECT(snapshot.entries.front().message == "entry-two");
        EXPECT(snapshot.entries.back().message == "entry-four");
        EXPECT(snapshot.entries.front().sequence < snapshot.entries.back().sequence);
        Hybrid::LogSystem::clearBufferedEntries();
        EXPECT(Hybrid::LogSystem::bufferedEntries().entries.empty());
        Hybrid::LogSystem::shutdown();
        std::error_code ignored;
        std::filesystem::remove(log_path, ignored);
    }

    void testProjectFileUpdate()
    {
        const auto path = std::filesystem::temp_directory_path() / "hybrid_project_file_update.hyproj";
        {
            std::ofstream output(path, std::ios::binary | std::ios::trunc);
            output << "# Keep this comment\nformat_version=2\nunknown_key=keep-me\nstartup_scene=asset:Old.scene\n";
        }

        std::string error;
        EXPECT(Hybrid::ProjectFile::updateValue(path, "startup_scene", "asset:New.scene", error));
        std::ifstream input(path, std::ios::binary);
        const std::string updated((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        EXPECT(updated.find("# Keep this comment") != std::string::npos);
        EXPECT(updated.find("unknown_key=keep-me") != std::string::npos);
        EXPECT(updated.find("startup_scene=asset:New.scene") != std::string::npos);
        EXPECT(updated.find("# Keep this comment") < updated.find("format_version=2"));
        EXPECT(updated.find("format_version=2") < updated.find("unknown_key=keep-me"));
        EXPECT(updated.find("unknown_key=keep-me") < updated.find("startup_scene=asset:New.scene"));
        EXPECT(!Hybrid::ProjectFile::updateValue(path, "bad=key", "value", error));
        std::ifstream unchanged_input(path, std::ios::binary);
        const std::string unchanged((std::istreambuf_iterator<char>(unchanged_input)),
                                    std::istreambuf_iterator<char>());
        EXPECT(unchanged == updated);

        std::error_code ignored;
        std::filesystem::remove(path, ignored);
    }

    class CountingLayer final : public Hybrid::Layer
    {
    public:
        CountingLayer(int& events, int& detaches, bool handles)
            : m_events(events), m_detaches(detaches), m_handles(handles) {}
        void onEvent(Hybrid::Event& event) override
        {
            ++m_events;
            if (m_handles)
                event.Handled = true;
        }
        void onDetach() override { ++m_detaches; }
    private:
        int& m_events;
        int& m_detaches;
        bool m_handles;
    };

    void testEngineLifecycleAndEvents()
    {
        Hybrid::HybridEngine engine;
        Hybrid::EngineConfig config{};
        config.project_path = HYBRID_TEST_PROJECT_PATH;
        config.headless = true;
        config.worker_count = 2;
        EXPECT(engine.initialize(config));
        EXPECT(engine.initialize(config));

        int exit_requests = 0;
        engine.setExitRequestHandler([&exit_requests] { ++exit_requests; });
        Hybrid::WindowCloseEvent close;
        engine.onEvent(close);
        EXPECT(close.Handled);
        EXPECT(exit_requests == 1);
        engine.setExitRequestHandler({});

        int lower_events = 0;
        int upper_events = 0;
        int lower_detaches = 0;
        int upper_detaches = 0;
        engine.pushLayer(std::make_unique<CountingLayer>(lower_events, lower_detaches, false));
        engine.pushOverlay(std::make_unique<CountingLayer>(upper_events, upper_detaches, true));
        Hybrid::MouseScrolledEvent scroll(0.5f, 2.0f);
        engine.onEvent(scroll);
        EXPECT(engine.getInputLayer().getState().getScrollDeltaY() == 2.0f);
        EXPECT(upper_events == 1);
        EXPECT(lower_events == 0);

        Hybrid::KeyTypedEvent typed('x');
        engine.onEvent(typed);
        EXPECT(engine.getInputLayer().getState().getTextInput() == U"x");
        Hybrid::KeyPressedEvent key(65);
        engine.onEvent(key);
        EXPECT(engine.getInputLayer().getState().isKeyDown(65));
        EXPECT(engine.getInputLayer().getState().wasKeyPressed(65));
        Hybrid::MouseMovedEvent mouse_origin(10.0f, 10.0f);
        Hybrid::MouseMovedEvent mouse_delta(13.0f, 14.0f);
        engine.onEvent(mouse_origin);
        engine.onEvent(mouse_delta);
        EXPECT(engine.getInputLayer().getState().getMouseDeltaX() == 3.0f);
        EXPECT(engine.getInputLayer().getState().getMouseDeltaY() == 4.0f);

        engine.run(2);
        engine.shutdown();
        engine.shutdown();
        EXPECT(lower_detaches == 1);
        EXPECT(upper_detaches == 1);

        Hybrid::EngineConfig bad{};
        bad.project_path = std::filesystem::path(HYBRID_TEST_PROJECT_PATH).parent_path() / "missing.hyproj";
        bad.headless = true;
        EXPECT(!engine.initialize(bad));
        engine.shutdown();
        EXPECT(engine.initialize(config));
        engine.shutdown();
    }

    void testAssetRegistryAndManager()
    {
        auto registry = std::make_shared<Hybrid::AssetRegistry>();
        registry->registerAsset(metadata(1, "asset:one.test"));
        const auto copy = registry->findByPath("asset:one.test");
        EXPECT(copy && copy->id.value == 1);
        registry->unregisterAsset({1});
        EXPECT(copy && copy->id.value == 1);
        EXPECT(!registry->find({1}));

        auto jobs = std::make_shared<Hybrid::JobSystem>();
        jobs->initialize({2});
        registry->registerAsset(metadata(2, "asset:two.test"));
        auto manager = std::make_shared<Hybrid::AssetManager>(std::make_shared<NullVfs>(), registry, jobs);
        std::atomic<int> load_count{0};
        manager->registerLoader<int>(Hybrid::AssetType::Script,
            [&load_count](const Hybrid::AssetMetadata&, Hybrid::IVirtualFileSystem&) {
                ++load_count;
                return std::make_shared<int>(7);
            });
        const auto first = manager->loadAsync<int>({2});
        const auto second = manager->loadAsync<int>({2});
        EXPECT(first.get() && second.get());
        EXPECT(load_count.load() == 1);

        registry->registerAsset(metadata(3, "asset:three.test"));
        std::promise<void> release_promise;
        auto release = release_promise.get_future().share();
        manager->registerLoader<std::string>(Hybrid::AssetType::Script,
            [release](const Hybrid::AssetMetadata&, Hybrid::IVirtualFileSystem&) {
                release.wait();
                return std::make_shared<std::string>("stale");
            });
        auto stale = manager->loadAsync<std::string>({3});
        manager->unload({3});
        release_promise.set_value();
        EXPECT(!stale.get());

        registry->registerAsset(metadata(4, "asset:four.test"));
        std::atomic<bool> drained_job_finished{false};
        manager->registerLoader<double>(Hybrid::AssetType::Script,
            [&drained_job_finished](const Hybrid::AssetMetadata&, Hybrid::IVirtualFileSystem&) {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
                drained_job_finished = true;
                return std::make_shared<double>(4.0);
            });
        const auto draining = manager->loadAsync<double>({4});
        manager->shutdown();
        EXPECT(drained_job_finished.load());
        EXPECT(!draining.get());
        manager->shutdown();
        jobs->shutdown();
    }

    void testInputState()
    {
        Hybrid::InputLayer input;
        Hybrid::KeyPressedEvent key(12);
        Hybrid::MouseScrolledEvent scroll(1.0f, -2.0f);
        Hybrid::KeyTypedEvent text('A');
        Hybrid::MouseMovedEvent first_move(4.0f, 5.0f);
        Hybrid::MouseMovedEvent second_move(7.0f, 9.0f);
        input.onEvent(key);
        input.onEvent(scroll);
        input.onEvent(text);
        input.onEvent(first_move);
        input.onEvent(second_move);
        EXPECT(input.getState().wasKeyPressed(12));
        EXPECT(input.getState().getScrollDeltaX() == 1.0f);
        EXPECT(input.getState().getScrollDeltaY() == -2.0f);
        EXPECT(input.getState().getTextInput() == U"A");
        EXPECT(input.getState().getMouseDeltaX() == 3.0f);
        EXPECT(input.getState().getMouseDeltaY() == 4.0f);

        Hybrid::MouseButtonPressedEvent mouse_button(1);
        input.onEvent(mouse_button);
        Hybrid::WindowLostFocusEvent focus_lost;
        input.onEvent(focus_lost);
        EXPECT(!input.getState().isKeyDown(12));
        EXPECT(input.getState().wasKeyReleased(12));
        EXPECT(!input.getState().isMouseDown(1));
        EXPECT(input.getState().wasMouseReleased(1));
        EXPECT(input.getState().getMouseDeltaX() == 0.0f);
        EXPECT(input.getState().getMouseDeltaY() == 0.0f);
        input.onEndFrame();
        EXPECT(input.getState().getTextInput().empty());
        EXPECT(input.getState().getScrollDeltaY() == 0.0f);
    }

    void testSceneRoundTrip()
    {
        Hybrid::Scene source;
        auto parent = source.createEntity("Parent");
        auto child = source.createEntity("Child");
        EXPECT(source.SetParent(child, parent, false));
        EXPECT(source.IsDescendant(child, parent));
        auto runtime = source.cloneRuntime();
        EXPECT(runtime && runtime->getRootEntities().size() == 1);

        const auto path = std::filesystem::temp_directory_path() / "hybrid_scene_roundtrip_test.scene";
        EXPECT(Hybrid::SceneSerializer::SerializeToFile(source, path));
        Hybrid::Scene restored;
        EXPECT(Hybrid::SceneSerializer::DeserializeFromFile(restored, path));
        EXPECT(restored.getRootEntities().size() == 1);
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    void testRenderGraph()
    {
        const auto valid = Hybrid::CompileRenderGraph(Hybrid::CreateDefaultRenderGraphBuild());
        EXPECT(valid.isValid());

        Hybrid::RenderGraphBuilder builder;
        builder.addResource({"TransientColor", Hybrid::RenderResourceId::SceneColor,
                             Hybrid::RenderGraphResourceKind::Texture2D,
                             Hybrid::RenderGraphResourceFormat::RGBA8,
                             Hybrid::RenderGraphResourceLifetime::Transient});
        builder.addPass("BadRead", Hybrid::RenderPassType::PostProcess, Hybrid::RenderFlags::PostProcess)
            .read(Hybrid::RenderResourceId::SceneColor);
        EXPECT(Hybrid::CompileRenderGraph(builder.build()).validation.hasErrors());

        Hybrid::RenderGraphBuilder duplicate_write;
        duplicate_write.addResource({"SceneColor", Hybrid::RenderResourceId::SceneColor,
                                     Hybrid::RenderGraphResourceKind::Texture2D,
                                     Hybrid::RenderGraphResourceFormat::RGBA8,
                                     Hybrid::RenderGraphResourceLifetime::External});
        duplicate_write.addPass("FirstWrite", Hybrid::RenderPassType::Scene, Hybrid::RenderFlags::Scene)
            .write(Hybrid::RenderResourceId::SceneColor);
        duplicate_write.addPass("SecondWrite", Hybrid::RenderPassType::Skybox, Hybrid::RenderFlags::Scene)
            .write(Hybrid::RenderResourceId::SceneColor);
        EXPECT(Hybrid::CompileRenderGraph(duplicate_write.build()).validation.hasErrors());

        Hybrid::RenderGraphBuilder dynamic_graph;
        dynamic_graph.addTextureResource("FeatureMask", Hybrid::RenderGraphResourceFormat::R8,
                                         Hybrid::RenderGraphResourceLifetime::Transient);
        dynamic_graph.addPass("FeatureMaskProducer", Hybrid::RenderPassType::Custom, Hybrid::RenderFlags::Scene)
            .write("FeatureMask");
        dynamic_graph.addPass("FeatureMaskConsumer", Hybrid::RenderPassType::Custom, Hybrid::RenderFlags::Scene)
            .read("FeatureMask");
        EXPECT(Hybrid::CompileRenderGraph(dynamic_graph.build()).isValid());
    }

    class ProbeRenderFeature final : public Hybrid::IRenderFeature
    {
    public:
        explicit ProbeRenderFeature(std::vector<std::string>& execution) : m_execution(execution) {}

        Hybrid::RenderFeatureDesc describe() const override
        {
            return {"ProbeFeature", Hybrid::RenderFeatureInjectionPoint::AfterScene,
                    Hybrid::RenderFlags::Scene, false};
        }

        bool declareResources(Hybrid::RenderGraphBuilder& builder,
                              Hybrid::RenderGraphBlackboard& blackboard) const override
        {
            builder.addTextureResource("ProbeMask", Hybrid::RenderGraphResourceFormat::R8,
                                       Hybrid::RenderGraphResourceLifetime::Transient);
            return blackboard.publish("ProbeMask", "ProbeMask");
        }

        void declarePass(Hybrid::RenderGraphPassBuilder& pass,
                         const Hybrid::RenderGraphBlackboard& blackboard) const override
        {
            const auto scene_color = blackboard.find("SceneColor");
            const auto probe_mask = blackboard.find("ProbeMask");
            if (scene_color)
                pass.read(*scene_color);
            if (probe_mask)
                pass.write(*probe_mask);
        }

        void execute(Hybrid::RenderContext&) override { m_execution.push_back("feature"); }

    private:
        std::vector<std::string>& m_execution;
    };

    void testRenderFeatureInjection()
    {
        std::vector<std::string> execution;
        Hybrid::RenderPipeline pipeline;
        EXPECT(pipeline.registerFeature(std::make_shared<ProbeRenderFeature>(execution)));
        EXPECT(!pipeline.registerFeature(std::make_shared<ProbeRenderFeature>(execution)));
        EXPECT(pipeline.getCompiledGraph().isValid());

        bool found_feature_pass = false;
        bool found_feature_resource = false;
        for (const auto& resource : pipeline.getCompiledGraph().resources)
            found_feature_resource = found_feature_resource || resource.name == "ProbeMask";
        for (const auto& pass : pipeline.getCompiledGraph().passes)
            found_feature_pass = found_feature_pass || pass.desc.name == "ProbeFeature";
        EXPECT(found_feature_resource);
        EXPECT(found_feature_pass);

        Hybrid::RenderContext context{};
        context.flags = Hybrid::RenderFlags::Scene;
        Hybrid::RenderPipelineCallbacks callbacks{};
        callbacks.scene = [&execution](Hybrid::RenderContext&) { execution.push_back("scene"); };
        callbacks.skybox = [&execution](Hybrid::RenderContext&) { execution.push_back("skybox"); };
        pipeline.execute(context, callbacks);
        EXPECT(execution.size() == 3);
        EXPECT(execution[0] == "scene");
        EXPECT(execution[1] == "feature");
        EXPECT(execution[2] == "skybox");
        EXPECT(pipeline.unregisterFeature("ProbeFeature"));
        EXPECT(!pipeline.unregisterFeature("ProbeFeature"));
    }

    void testRenderGraphResourceMaterialization()
    {
        auto device = Hybrid::CreateNullRenderDevice();
        Hybrid::RenderGraphBuilder builder;
        builder.addTextureResource("FeatureHistory", Hybrid::RenderGraphResourceFormat::RGBA8,
                                   Hybrid::RenderGraphResourceLifetime::Transient);
        builder.addPass("FeatureWrite", Hybrid::RenderPassType::Custom, Hybrid::RenderFlags::Scene)
            .write("FeatureHistory");
        builder.addPass("FeatureRead", Hybrid::RenderPassType::Custom, Hybrid::RenderFlags::Scene)
            .read("FeatureHistory");
        const auto graph = Hybrid::CompileRenderGraph(builder.build());
        EXPECT(graph.isValid());

        Hybrid::RenderGraphResourceRegistry resources;
        EXPECT(resources.prepare(*device, graph, 64, 32));
        const auto first = resources.texture("FeatureHistory");
        EXPECT(first);
        const auto first_desc = device->textureDesc(first.value);
        EXPECT(first_desc);
        EXPECT(first_desc.value.width == 64 && first_desc.value.height == 32);
        EXPECT(first_desc.value.format == Hybrid::RhiFormat::RGBA8Unorm);

        EXPECT(resources.prepare(*device, graph, 128, 32));
        const auto second = resources.texture("FeatureHistory");
        EXPECT(second);
        EXPECT(second.value != first.value);
        EXPECT(!device->textureDesc(first.value));
        const auto second_desc = device->textureDesc(second.value);
        EXPECT(second_desc && second_desc.value.width == 128);
        resources.shutdown();
        EXPECT(!device->textureDesc(second.value));
    }

    void testNullRenderDevice()
    {
        auto device = Hybrid::CreateNullRenderDevice();
        EXPECT(device != nullptr);
        EXPECT(device->backend() == Hybrid::GraphicsBackend::Null);
        EXPECT(device->capabilities().max_texture_dimension_2d >= 1);

        Hybrid::BufferDesc buffer_desc{};
        buffer_desc.size = sizeof(uint32_t) * 3;
        buffer_desc.usage = Hybrid::RhiBufferUsage::Vertex;
        const uint32_t vertices[] = {1, 2, 3};
        const auto vertex_buffer = device->createBuffer(buffer_desc, vertices, sizeof(vertices));
        EXPECT(vertex_buffer);

        const uint32_t replacement = 4;
        EXPECT(device->updateBuffer(vertex_buffer.value, sizeof(uint32_t),
                                    &replacement, sizeof(replacement)));
        const auto out_of_bounds = device->updateBuffer(vertex_buffer.value, buffer_desc.size,
                                                        &replacement, sizeof(replacement));
        EXPECT(!out_of_bounds);
        EXPECT(out_of_bounds.error.code == Hybrid::RhiErrorCode::OutOfBounds);

        EXPECT(device->destroyBuffer(vertex_buffer.value));
        const auto stale_update = device->updateBuffer(vertex_buffer.value, 0,
                                                       &replacement, sizeof(replacement));
        EXPECT(!stale_update);
        EXPECT(stale_update.error.code == Hybrid::RhiErrorCode::InvalidHandle);
        const auto replacement_buffer = device->createBuffer(buffer_desc);
        EXPECT(replacement_buffer);
        EXPECT(replacement_buffer.value.index == vertex_buffer.value.index);
        EXPECT(replacement_buffer.value.generation != vertex_buffer.value.generation);

        Hybrid::RhiTextureDesc texture_desc{};
        texture_desc.width = 16;
        texture_desc.height = 16;
        texture_desc.render_target = true;
        const auto texture = device->createTexture(texture_desc);
        EXPECT(texture);
        const auto color_view = device->createTextureView(texture.value);
        EXPECT(color_view);
        const auto queried_texture_desc = device->textureDesc(color_view.value);
        EXPECT(queried_texture_desc);
        EXPECT(queried_texture_desc.value.width == texture_desc.width);
        EXPECT(queried_texture_desc.value.height == texture_desc.height);

        Hybrid::ShaderModuleDesc vertex_shader_desc{};
        vertex_shader_desc.stage = Hybrid::RhiShaderStage::Vertex;
        vertex_shader_desc.code = {1};
        const auto vertex_shader = device->createShaderModule(vertex_shader_desc);
        EXPECT(vertex_shader);
        Hybrid::ShaderModuleDesc fragment_shader_desc{};
        fragment_shader_desc.stage = Hybrid::RhiShaderStage::Fragment;
        fragment_shader_desc.code = {2};
        const auto fragment_shader = device->createShaderModule(fragment_shader_desc);
        EXPECT(fragment_shader);

        Hybrid::GraphicsPipelineDesc pipeline_desc{};
        pipeline_desc.vertex_shader = vertex_shader.value;
        pipeline_desc.fragment_shader = fragment_shader.value;
        pipeline_desc.texture_bindings.push_back({{2, 0}, "TestTexture"});
        pipeline_desc.uniform_buffer_bindings.push_back({{3, 0}, "TestUniforms"});
        const auto pipeline = device->createGraphicsPipeline(pipeline_desc);
        EXPECT(pipeline);

        Hybrid::SamplerDesc sampler_desc{};
        const auto sampler = device->createSampler(sampler_desc);
        EXPECT(sampler);
        Hybrid::BufferDesc uniform_desc{};
        uniform_desc.size = 16;
        uniform_desc.usage = Hybrid::RhiBufferUsage::Uniform;
        const auto uniform_buffer = device->createBuffer(uniform_desc);
        EXPECT(uniform_buffer);
        const auto copy_texture = device->createTexture(texture_desc);
        EXPECT(copy_texture);
        const auto copy_view = device->createTextureView(copy_texture.value);
        EXPECT(copy_view);

        auto commands = device->createCommandList();
        EXPECT(commands != nullptr);
        EXPECT(!commands->draw(3));
        EXPECT(commands->begin());
        EXPECT(!commands->beginRenderPass({}));
        Hybrid::RenderPassDesc pass_desc{};
        pass_desc.colors.push_back({color_view.value});
        EXPECT(commands->beginRenderPass(pass_desc));
        EXPECT(commands->setViewport({0.0f, 0.0f, 16.0f, 16.0f}));
        EXPECT(commands->bindPipeline(pipeline.value));
        EXPECT(commands->bindTexture(color_view.value, sampler.value, {2, 0}));
        EXPECT(commands->bindUniformBuffer(uniform_buffer.value, {3, 0}));
        EXPECT(commands->bindVertexBuffer(replacement_buffer.value));
        EXPECT(commands->draw(3));
        EXPECT(commands->endRenderPass());
        EXPECT(commands->copyTexture(color_view.value, copy_view.value));
        EXPECT(commands->end());
        EXPECT(device->submit(*commands));

        EXPECT(device->destroyTexture(texture.value));
        const auto stale_texture_desc = device->textureDesc(color_view.value);
        EXPECT(!stale_texture_desc);
        EXPECT(stale_texture_desc.error.code == Hybrid::RhiErrorCode::InvalidHandle);
        auto stale_view_commands = device->createCommandList();
        EXPECT(stale_view_commands->begin());
        const auto stale_view_pass = stale_view_commands->beginRenderPass(pass_desc);
        EXPECT(!stale_view_pass);
        EXPECT(stale_view_pass.error.code == Hybrid::RhiErrorCode::InvalidHandle);

        EXPECT(device->destroyTextureView(color_view.value));
        EXPECT(device->destroyTextureView(copy_view.value));
        EXPECT(device->destroyTexture(copy_texture.value));
        EXPECT(device->destroySampler(sampler.value));
        EXPECT(device->destroyBuffer(uniform_buffer.value));
        EXPECT(device->destroyPipeline(pipeline.value));
        EXPECT(device->destroyShaderModule(vertex_shader.value));
        EXPECT(device->destroyShaderModule(fragment_shader.value));
        EXPECT(device->destroyBuffer(replacement_buffer.value));
    }
} // namespace

int main()
{
    testBufferedLog();
    testProjectFileUpdate();
    testJobSystem();
    testFrameClock();
    testEngineLifecycleAndEvents();
    testAssetRegistryAndManager();
    testInputState();
    testSceneRoundTrip();
    testRenderGraph();
    testRenderFeatureInjection();
    testRenderGraphResourceMaterialization();
    testNullRenderDevice();
    if (failures != 0)
    {
        std::cerr << failures << " test assertion(s) failed\n";
        return 1;
    }
    std::cout << "HybridTests: all checks passed\n";
    return 0;
}
