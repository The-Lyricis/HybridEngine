#include "render_system.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/gl.h>

#include <algorithm>
#include <cstddef>
#include <array>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include "runtime/modules/render/public/renderer.h"
#include "runtime/modules/render/public/render_command.h"
#include "runtime/modules/render/public/vertex_array.h"
#include "runtime/modules/render/public/shader.h"
#include "runtime/modules/render/public/framebuffer.h"
#include "runtime/modules/render/public/texture.h"
#include "runtime/modules/scene/scene.h"
#include "runtime/modules/scene/components.h"
#include "runtime/core/base/macro.h"
#include "runtime/core/base/math_util.h"
#include "runtime/modules/asset/asset_manager.h"
#include "runtime/modules/asset/material.h"
#include "runtime/modules/asset/mesh.h"
#include <runtime/modules/scene/components/collider_component.h>


namespace Hybrid
{

    namespace
    {
        uint32_t encodeEntityID(uint32_t entity_id)
        {
            return entity_id + 1u;
        }

        uint32_t decodeEntityID(uint32_t encoded_id)
        {
            return (encoded_id == 0) ? kInvalidEntityID : (encoded_id - 1u);
        }

        void setSceneFramebufferDrawBuffers(bool write_entity_id)
        {
            if (write_entity_id)
            {
                constexpr GLenum buffers[2] = {GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1};
                glDrawBuffers(2, buffers);
            }
            else
            {
                constexpr GLenum buffer = GL_COLOR_ATTACHMENT0;
                glDrawBuffers(1, &buffer);
            }
        }

        static glm::vec3 lightDirectionFromTransform(const Hybrid::TransformComponent &tr)
        {
            const glm::vec3 dir = glm::vec3(tr.WorldMatrix * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
            const float len = glm::length(dir);
            if (len < 1e-4f)
                return glm::vec3(0.0f, -1.0f, 0.0f);
            return dir / len;
        }
        // 鍖垮悕鍛藉悕绌洪棿閲屾柊澧烇細杈撳嚭 view/proj/camPos
        static bool getSceneCameraMatrices(Hybrid::Scene& scene,
            float aspect,
            glm::mat4& outView,
            glm::mat4& outProj,
            glm::vec3& outCamPos,
            glm::vec4* outClearColor = nullptr,
            bool* outUseSkyboxClear = nullptr)
        {
            auto& reg = scene.getRegistry();
            auto view = reg.view<Hybrid::TransformComponent, Hybrid::CameraComponent>();

            entt::entity mainCam = entt::null;
            for (auto e : view)
            {
                auto& cam = view.get<Hybrid::CameraComponent>(e);
                if (!cam.Enabled)
                    continue;
                if (cam.Primary) { mainCam = e; break; }
            }
            if (mainCam == entt::null)
            {
                for (auto e : view)
                {
                    auto& cam = view.get<Hybrid::CameraComponent>(e);
                    if (cam.Enabled)
                    {
                        mainCam = e;
                        break;
                    }
                }
            }
            if (mainCam == entt::null) return false;

            const auto& tr = reg.get<Hybrid::TransformComponent>(mainCam);
            const auto& cam = reg.get<Hybrid::CameraComponent>(mainCam);

            outProj = glm::perspective(glm::radians(cam.FovY), aspect, cam.Near, cam.Far);

            outView = glm::inverse(tr.WorldMatrix);

            outCamPos = glm::vec3(tr.WorldMatrix[3]);
            if (outClearColor)
                *outClearColor = cam.ClearColor;
            if (outUseSkyboxClear)
                *outUseSkyboxClear = (cam.ClearMode == CameraClearMode::Skybox);
            return true;
        }
    }

    void RenderSystem::MaterialGPU::bind(Shader &shader) const
    {
        shader.setVec4("u_AlbedoColor", data.albedo_color);
        shader.setFloat("u_Metallic", data.metallic);
        shader.setFloat("u_Roughness", data.roughness);
        shader.setFloat("u_AO", data.ao);
        shader.setFloat("u_Emissive", data.emissive);

        // Use the asset id to decide normal-map enablement.
        shader.setInt("u_HasNormalMap", (data.normal_map.value != 0) ? 1 : 0);

        if (albedo)
            albedo->bind(0);
        if (normal)
            normal->bind(1);
        if (mr)
            mr->bind(2);
        if (ao)
            ao->bind(3);
        if (emissive)
            emissive->bind(4);
    }

    void RenderSystem::initialize(void *glfwWindowHandle)
    {
        if (m_Initialized)
            return;

        Renderer::initialize();
        createMeshShader();
        createDebugBoxShader();

        GLFWwindow *window = static_cast<GLFWwindow *>(glfwWindowHandle);
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        FramebufferSpec spec;
        spec.width = (uint32_t)std::max(1, w);
        spec.height = (uint32_t)std::max(1, h);
        m_SceneFB = Framebuffer::Create(spec);
        m_GameFB = Framebuffer::Create(spec);

        m_Initialized = true;

        HBD_CORE_TRACE("RenderSystem initialized");
    }

    RenderSystem::MeshGPU *RenderSystem::getOrCreateMeshGPU(AssetID id, const std::shared_ptr<Mesh> &mesh)
    {
        if (!mesh)
            return nullptr;
        if (auto it = m_MeshCache.find(id); it != m_MeshCache.end())
            return &it->second;

        MeshGPU mgpu;
        const auto &verts = mesh->getVertices();
        const auto &inds = mesh->getIndices();

        mgpu.vb = VertexBuffer::Create(verts.data(), static_cast<uint32_t>(verts.size() * sizeof(MeshVertex)));
        mgpu.ib = IndexBuffer::Create(inds.data(), static_cast<uint32_t>(inds.size()));
        mgpu.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(MeshVertex);
        layout.attributes = {
            {0, 3, static_cast<uint32_t>(offsetof(MeshVertex, position)), false},
            {1, 3, static_cast<uint32_t>(offsetof(MeshVertex, normal)), false},
            {2, 2, static_cast<uint32_t>(offsetof(MeshVertex, uv)), false},
            {3, 4, static_cast<uint32_t>(offsetof(MeshVertex, tangent)), false},
        };
        mgpu.vao->setVertexBuffer(mgpu.vb, layout);
        mgpu.vao->setIndexBuffer(mgpu.ib);
        mgpu.submeshes = mesh->getSubmeshes();

        auto [it, inserted] = m_MeshCache.emplace(id, std::move(mgpu));
        return &it->second;
    }

    RenderSystem::MaterialGPU *RenderSystem::getOrCreateMaterialGPU(AssetID id, const std::shared_ptr<Material> &mat)
    {
        if (!mat)
            return nullptr;

        if (!m_DefaultAlbedoTex)
            m_DefaultAlbedoTex = createSolidTexture(255, 255, 255, 255);
        if (!m_DefaultNormalTex)
            m_DefaultNormalTex = createSolidTexture(128, 128, 255, 255); // normal blue
        if (!m_DefaultMRTex)
            m_DefaultMRTex = createSolidTexture(255, 255, 0, 255); // mr=(1,1), neutral for multiplicative combine
        if (!m_DefaultAOTex)
            m_DefaultAOTex = createSolidTexture(255, 255, 255, 255); // ao=1
        if (!m_DefaultEmissiveTex)
            m_DefaultEmissiveTex = createSolidTexture(0, 0, 0, 255); // black

        auto texOrDefault = [&](AssetID tid, const TexturePtr &fallback) -> TexturePtr
        {
            if (!m_AssetManager)
                return fallback;
            if (tid.value == 0)
                return fallback;
            auto t = m_AssetManager->loadSync<Texture>(tid);
            return t ? t : fallback;
        };

        auto refreshMaterialGPU = [&](MaterialGPU& mgpu) {
            mgpu.data = mat->getData();
            mgpu.albedo = texOrDefault(mgpu.data.albedo_map, m_DefaultAlbedoTex);
            mgpu.normal = texOrDefault(mgpu.data.normal_map, m_DefaultNormalTex);
            mgpu.mr = texOrDefault(mgpu.data.metallic_roughness_map, m_DefaultMRTex);
            mgpu.ao = texOrDefault(mgpu.data.ao_map, m_DefaultAOTex);
            mgpu.emissive = texOrDefault(mgpu.data.emissive_map, m_DefaultEmissiveTex);
        };

        if (auto it = m_MatCache.find(id); it != m_MatCache.end())
        {
            refreshMaterialGPU(it->second);
            return &it->second;
        }

        MaterialGPU mgpu;
        refreshMaterialGPU(mgpu);

        auto [it, inserted] = m_MatCache.emplace(id, std::move(mgpu));
        return &it->second;
    }

    TexturePtr RenderSystem::createSolidTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a)
    {
        TextureDesc desc;
        desc.type = TextureType::Tex2D;
        desc.format = TextureFormat::RGBA8;
        desc.width = desc.height = 1;
        desc.layers = 1;
        desc.mipLevels = 1;
        uint8_t data[4] = {r, g, b, a};
        return Texture::Create(desc, data, sizeof(data));
    }

    

    void RenderSystem::createMeshShader()
    {
        const std::string vs = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec4 aTangent;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec4 vTangent;

void main() {
    mat3 normalMat = transpose(inverse(mat3(u_Model)));

    vWorldPos = vec3(u_Model * vec4(aPos, 1.0));
    vNormal   = normalize(normalMat * aNormal);

    vec3 T = normalize(normalMat * aTangent.xyz);
    vTangent = vec4(T, aTangent.w);

    vUV = aUV;
    gl_Position = u_ViewProjection * vec4(vWorldPos, 1.0);
}
)";

        const std::string fs = R"(
#version 330 core
struct DirLight {
    vec3 color;
    float intensity;
    vec3 direction;
    float pad0;
};

struct PointLight {
    vec3 color;
    float intensity;
    vec3 position;
    float range;
};

const int MAX_POINT_LIGHTS = 16;

in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in vec4 vTangent;

layout(location=0) out vec4 FragColor;
layout(location=1) out uint EntityID;

uniform uint u_EntityID;
uniform int u_Selected;

uniform vec3 u_CameraPos;
uniform DirLight u_DirLight;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int u_PointCount;

uniform vec4 u_AlbedoColor;
uniform vec4 u_TintColor;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform float u_Emissive;

uniform sampler2D u_AlbedoMap;
uniform sampler2D u_NormalMap;
uniform sampler2D u_MRMap;   // R=metallic, G=roughness
uniform sampler2D u_AOMap;
uniform sampler2D u_EmissiveMap;
uniform int u_HasNormalMap;

vec3 getNormal() {
    vec3 N = normalize(vNormal);
    if (u_HasNormalMap == 0)
        return N;

    if (length(vTangent.xyz) < 1e-4)
        return N;
    vec3 T = normalize(vTangent.xyz);
    T = normalize(T - N * dot(N, T)); // Gram-Schmidt
    vec3 B = normalize(cross(N, T)) * vTangent.w;

    vec3 nMap = texture(u_NormalMap, vUV).xyz * 2.0 - 1.0;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nMap);
}

void main() {
    vec3 albedo = (u_AlbedoColor * u_TintColor).rgb * texture(u_AlbedoMap, vUV).rgb;
    vec2 mrTex = texture(u_MRMap, vUV).rg;
    float metallic  = clamp(u_Metallic * mrTex.r, 0.0, 1.0);
    float roughness = clamp(u_Roughness * mrTex.g, 0.04, 1.0);
    float ao        = clamp(u_AO * texture(u_AOMap, vUV).r, 0.0, 1.0);
    vec3 emissiveColor = texture(u_EmissiveMap, vUV).rgb * max(0.0, u_Emissive);

    vec3 N = getNormal();
    vec3 V = normalize(u_CameraPos - vWorldPos);

    vec3 ambient = 0.05 * albedo * ao;
    vec3 color = ambient;

    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ks = F0;
    vec3 kd = (vec3(1.0) - ks) * (1.0 - metallic);
    float specPow = mix(8.0, 128.0, 1.0 - roughness);

    // Directional light
    vec3 Ld = normalize(-u_DirLight.direction);
    float ndl = max(dot(N, Ld), 0.0);
    vec3 H = normalize(Ld + V);
    float spec = pow(max(dot(N, H), 0.0), specPow);
    color += (kd * ndl * albedo + ks * spec) * u_DirLight.color * u_DirLight.intensity;

    // Point lights
    for (int i = 0; i < u_PointCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 Lp = u_PointLights[i].position - vWorldPos;
        float dist = length(Lp);
        if (dist > u_PointLights[i].range) continue;

        float invDist = 1.0 / max(dist, 1e-4);
        Lp *= invDist;

        float att = 1.0 - clamp(dist / u_PointLights[i].range, 0.0, 1.0);
        float ndl_p = max(dot(N, Lp), 0.0);

        vec3 Hp = normalize(Lp + V);
        float specp = pow(max(dot(N, Hp), 0.0), specPow);

        color += att * ((kd * ndl_p * albedo + ks * specp) *
                        u_PointLights[i].color * u_PointLights[i].intensity);
    }

    color += emissiveColor;
    vec3 finalColor = color;
    if (u_Selected == 1) {
        finalColor = min(finalColor * 1.25 + vec3(0.10), vec3(1.0));
    }

    FragColor = vec4(finalColor, u_AlbedoColor.a * u_TintColor.a);
    EntityID  = u_EntityID;
}
)";

        m_MeshShader = Shader::Create(vs, fs);
    }
    void RenderSystem::createDebugBoxShader()
    {
        const std::string vs = R"(
#version 330 core
layout(location = 0) in vec3 aPos;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

void main()
{
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}
)";

        const std::string fs = R"(
#version 330 core
layout(location = 0) out vec4 FragColor;

uniform vec4 u_Color;

void main()
{
    FragColor = u_Color;
}
)";

        m_DebugBoxShader = Shader::Create(vs, fs);
    }

    void RenderSystem::ensureFramebufferSize(std::shared_ptr<Framebuffer>& framebuffer, uint32_t w, uint32_t h)
    {
        w = std::max(1u, w);
        h = std::max(1u, h);

        if (!framebuffer)
        {
            FramebufferSpec spec{w, h};
            framebuffer = Framebuffer::Create(spec);
        }
        else
        {
            framebuffer->resize(w, h);
        }
    }

    uint32_t RenderSystem::getSceneColorTexture() const
    {
        return m_SceneFB ? m_SceneFB->getColorAttachmentRendererID() : 0;
    }

    uint32_t RenderSystem::getGameColorTexture() const
    {
        return m_GameFB ? m_GameFB->getColorAttachmentRendererID() : 0;
    }

    void RenderSystem::onWindowResize(uint32_t width, uint32_t height)
    {
        if (!m_Initialized)
            return;
        ensureFramebufferSize(m_SceneFB, width, height);
        ensureFramebufferSize(m_GameFB, width, height);
    }

    void RenderSystem::invalidateAsset(AssetID id, AssetType type)
    {
        if (id.value == 0)
            return;

        switch (type)
        {
        case AssetType::Mesh:
            m_MeshCache.erase(id);
            break;
        case AssetType::Material:
            m_MatCache.erase(id);
            break;
        default:
            break;
        }
    }

    
    RenderSystem::RenderPacket RenderSystem::buildRenderPacket(const FrameContext& frame_context,
                                                               RenderFlags flags,
                                                               const EditorRenderExt* editor_ext,
                                                               bool cache_editor_camera_state)
    {
        RenderPacket pkt;

        const glm::vec2 viewport_size = frame_context.viewport_size;

        std::shared_ptr<Scene> scene = frame_context.scene ? frame_context.scene : m_Scene;

        bool use_game_camera = true;
        uint32_t selected_entity_id = kInvalidEntityID;
        if (editor_ext)
        {
            use_game_camera = editor_ext->use_game_camera;
            if (HasFlag(flags, RenderFlags::SelectionOutline))
            {
                selected_entity_id = editor_ext->selected_entity_id;
            }
        }

        // A) resolve active camera (scene game camera or editor-provided camera).
        glm::mat4 viewM(1.0f), projM(1.0f);
        glm::vec3 cameraPos(0.0f, 0.0f, 3.0f);
        const float aspect = (viewport_size.y > 0.0f) ? (viewport_size.x / viewport_size.y) : 1.0f;
        bool has_camera = false;

        pkt.frame.clearColor = glm::vec4(0.1f, 0.1f, 0.12f, 1.0f);
        pkt.frame.useSkyboxClear = false;

        if (use_game_camera && scene)
        {
            has_camera = getSceneCameraMatrices(
                *scene,
                aspect,
                viewM,
                projM,
                cameraPos,
                &pkt.frame.clearColor,
                &pkt.frame.useSkyboxClear);
        }
        else if (!use_game_camera && editor_ext && editor_ext->has_editor_camera)
        {
            viewM = editor_ext->editor_view;
            projM = editor_ext->editor_proj;
            cameraPos = editor_ext->editor_camera_pos;
            has_camera = true;
        }

        if (!has_camera && scene)
        {
            has_camera = getSceneCameraMatrices(*scene, aspect, viewM, projM, cameraPos);
        }

        if (!has_camera)
        {
            viewM = glm::mat4(1.0f);
            projM = glm::mat4(1.0f);
            cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
        }

        pkt.scene = scene;
        pkt.showColliderDebug = editor_ext ? editor_ext->show_collider_debug : false;

        // B) write packet
        pkt.frame.viewProj = projM * viewM;
        pkt.frame.cameraPos = cameraPos;
        pkt.frame.time = frame_context.dt;

        if (cache_editor_camera_state)
        {
            m_LastView = viewM;
            m_LastProj = projM;
        }

        // C) collect lights
        pkt.lights.dir.intensity = 0.0f;
        pkt.lights.points.reserve(kMaxPointLights);
        if (scene)
        {
            auto &reg = scene->getRegistry();

            auto dirView = reg.view<Hybrid::TransformComponent, Hybrid::DirectionalLightComponent>();
            for (auto e : dirView)
            {
                const auto &tc = dirView.get<Hybrid::TransformComponent>(e);
                const auto &dl = dirView.get<Hybrid::DirectionalLightComponent>(e);
                if (!dl.Enabled)
                    continue;
                pkt.lights.dir.color = dl.Color;
                pkt.lights.dir.intensity = dl.Intensity;
                pkt.lights.dir.direction = lightDirectionFromTransform(tc);
                break;
            }

            auto ptView = reg.view<Hybrid::TransformComponent, Hybrid::PointLightComponent>();
            for (auto e : ptView)
            {
                if ((int)pkt.lights.points.size() >= kMaxPointLights)
                    break;
                const auto &tc = ptView.get<Hybrid::TransformComponent>(e);
                const auto &pl = ptView.get<Hybrid::PointLightComponent>(e);
                if (!pl.Enabled)
                    continue;

                PointLightData p;
                p.color = pl.Color;
                p.intensity = pl.Intensity;
                p.position = tc.Position;
                p.range = pl.Range;
                pkt.lights.points.push_back(p);
            }
        }

        // D) collect draw items (pure ECS->packet)
        if (scene)
        {
            auto &registry = scene->getRegistry();
            auto renderView = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();

            pkt.items.reserve(renderView.size_hint());
            for (auto e : renderView)
            {
                const auto &tr = renderView.get<Hybrid::TransformComponent>(e);
                const auto &mr = renderView.get<Hybrid::MeshRendererComponent>(e);
                if (!mr.Enabled)
                    continue;

                DrawItem item;
                item.meshId = mr.Mesh;
                item.materialId = mr.Material;
                item.model = tr.WorldMatrix;
                item.tint = mr.Tint;
                item.entityID = (uint32_t)entt::to_integral(e);
                item.selected = (selected_entity_id != kInvalidEntityID && item.entityID == selected_entity_id);
                pkt.items.push_back(item);
            }
        }

        return pkt;
    }

    void RenderSystem::executePasses(const RenderPacket& packet,
                                     RenderFlags flags,
                                     void* glfwWindowHandle,
                                     const std::shared_ptr<Framebuffer>& framebuffer)
    {
        const bool needs_forward = HasFlag(flags, RenderFlags::Forward) ||
                                   HasFlag(flags, RenderFlags::PickingID) ||
                                   HasFlag(flags, RenderFlags::SelectionOutline);
        if (needs_forward)
        {
            executeForwardPass(packet, glfwWindowHandle, framebuffer);
        }

        if (HasFlag(flags, RenderFlags::PickingID))
            executePickingPass(packet, glfwWindowHandle);
        if (HasFlag(flags, RenderFlags::SelectionOutline))
            executeSelectionOutlinePass(packet, glfwWindowHandle);
        if (HasFlag(flags, RenderFlags::Gizmos))
            executeGizmoPass(packet, glfwWindowHandle, framebuffer);
        if (HasFlag(flags, RenderFlags::Grid))
            executeGridPass(packet, glfwWindowHandle);
        if (HasFlag(flags, RenderFlags::Shadows))
            executeShadowPass(packet, glfwWindowHandle);
        if (HasFlag(flags, RenderFlags::PostProcess))
            executePostProcessPass(packet, glfwWindowHandle);
        if (HasFlag(flags, RenderFlags::DebugNormals))
            executeDebugNormalsPass(packet, glfwWindowHandle);
    }

    void RenderSystem::executeForwardPass(const RenderPacket &packet,
                                          void *glfwWindowHandle,
                                          const std::shared_ptr<Framebuffer>& framebuffer)
    {
        if (!framebuffer)
            return;

        // 1) begin pass
        framebuffer->bind();
        setSceneFramebufferDrawBuffers(true);
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());
        // Skybox clear mode currently falls back to solid clear until a dedicated skybox pass exists.
        Renderer::beginFrame(packet.frame.clearColor);
        // Clear color to black and EntityID to 0 for picking/outline.
        uint32_t zero = 0;
        glClearBufferuiv(GL_COLOR, 1, &zero);

        if (m_AssetManager && m_MeshShader)
        {
            m_MeshShader->bind();
            m_MeshShader->setMat4("u_ViewProjection", packet.frame.viewProj);
            m_MeshShader->setVec3("u_CameraPos", packet.frame.cameraPos);
            m_MeshShader->setVec3("u_DirLight.color", packet.lights.dir.color);
            m_MeshShader->setFloat("u_DirLight.intensity", packet.lights.dir.intensity);
            m_MeshShader->setVec3("u_DirLight.direction", packet.lights.dir.direction);
            m_MeshShader->setInt("u_PointCount", static_cast<int>(packet.lights.points.size()));

            for (int i = 0; i < (int)packet.lights.points.size() && i < kMaxPointLights; ++i)
            {
                const auto &p = packet.lights.points[i];
                std::string base = "u_PointLights[" + std::to_string(i) + "]";
                m_MeshShader->setVec3(base + ".color", p.color);
                m_MeshShader->setFloat(base + ".intensity", p.intensity);
                m_MeshShader->setVec3(base + ".position", p.position);
                m_MeshShader->setFloat(base + ".range", p.range);
            }

            m_MeshShader->setInt("u_AlbedoMap", 0);
            m_MeshShader->setInt("u_NormalMap", 1);
            m_MeshShader->setInt("u_MRMap", 2);
            m_MeshShader->setInt("u_AOMap", 3);
            m_MeshShader->setInt("u_EmissiveMap", 4);

            for (const auto &item : packet.items)
            {
                AssetID meshId = item.meshId;
                if (meshId.value == 0)
                    continue;

                std::shared_ptr<Mesh> cpuMesh = m_AssetManager->loadSync<Mesh>(meshId);
                if (!cpuMesh)
                    continue;

                auto *meshGPU = getOrCreateMeshGPU(meshId, cpuMesh);
                if (!meshGPU)
                    continue;

                std::shared_ptr<Material> cpuMat;
                AssetID matId = item.materialId;
                if (matId.value != 0)
                    cpuMat = m_AssetManager->loadSync<Material>(matId);
                if (!cpuMat && !meshGPU->submeshes.empty() && meshGPU->submeshes[0].material.value != 0)
                {
                    matId = meshGPU->submeshes[0].material;
                    cpuMat = m_AssetManager->loadSync<Material>(matId);
                }
                if (!cpuMat)
                {
                    cpuMat = m_AssetManager->getDefault<Material>();
                    matId = AssetID{};
                }

                auto *matGPU = getOrCreateMaterialGPU(matId, cpuMat);
                if (!matGPU)
                    continue;

                for (const auto &sm : meshGPU->submeshes)
                {
                    const MaterialGPU *useMat = matGPU;
                    if (sm.material.value != 0)
                    {
                        auto subMat = m_AssetManager->loadSync<Material>(sm.material);
                        if (subMat)
                            if (auto *mg = getOrCreateMaterialGPU(sm.material, subMat))
                                useMat = mg;
                    }

                    m_MeshShader->setMat4("u_Model", item.model);
                    m_MeshShader->setVec4("u_TintColor", item.tint);
                    m_MeshShader->setUInt("u_EntityID", encodeEntityID(item.entityID));
                    m_MeshShader->setInt("u_Selected", item.selected ? 1 : 0);
                    useMat->bind(*m_MeshShader);

                    meshGPU->vao->bind();
                    RenderCommand::drawIndexed(sm.index_count, sm.index_offset);
                }
            }
        }

        // 5) end pass
        Renderer::endFrame();
        framebuffer->unbind();

        GLFWwindow *window = static_cast<GLFWwindow *>(glfwWindowHandle);
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        RenderCommand::setViewport(0, 0, display_w, display_h);
        RenderCommand::setClearColor({0.08f, 0.08f, 0.09f, 1.0f});
        RenderCommand::clear();
    }

    void RenderSystem::executePickingPass(const RenderPacket& packet, void* glfwWindowHandle)
    {
        // PickingID currently piggybacks on forward pass COLOR1 output.
        (void)packet;
        (void)glfwWindowHandle;
    }

    void RenderSystem::executeSelectionOutlinePass(const RenderPacket& packet, void* glfwWindowHandle)
    {
        // TODO: selection outline pass.
        (void)packet;
        (void)glfwWindowHandle;
    }

    void RenderSystem::executeGizmoPass(const RenderPacket& packet,
        void* glfwWindowHandle,
        const std::shared_ptr<Framebuffer>& framebuffer)
    {
        (void)glfwWindowHandle;

        if (!packet.showColliderDebug)
            return;

        if (!framebuffer)
            return;

        std::shared_ptr<Scene> scene = packet.scene;
        if (!scene)
            return;

        if (!m_DebugBoxShader)
            return;

        auto* debugBoxMeshGPU = getOrCreateDebugBoxMeshGPU();
        if (!debugBoxMeshGPU)
            return;

        auto& registry = scene->getRegistry();
        auto view = registry.view<TransformComponent, ColliderComponent>();

        framebuffer->bind();
        setSceneFramebufferDrawBuffers(false);
        RenderCommand::setViewport(0, 0, framebuffer->getWidth(), framebuffer->getHeight());

        glEnable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glLineWidth(2.0f);

        m_DebugBoxShader->bind();
        m_DebugBoxShader->setMat4("u_ViewProjection", packet.frame.viewProj);

        for (auto e : view)
        {
            const auto& tr = view.get<TransformComponent>(e);
            const auto& col = view.get<ColliderComponent>(e);

            if (!col.Enabled || col.Type != ColliderType::Box)
                continue;

            glm::vec4 color = glm::vec4(0.2f, 0.95f, 0.35f, 1.0f);
            if (col.IsTrigger)
                color = glm::vec4(1.0f, 0.85f, 0.2f, 1.0f);

            glm::mat4 colliderLocal =
                glm::translate(glm::mat4(1.0f), col.Center) *
                glm::scale(glm::mat4(1.0f), col.Box.HalfExtents * 2.0f);

            glm::mat4 model = tr.WorldMatrix * colliderLocal;

            m_DebugBoxShader->setMat4("u_Model", model);
            m_DebugBoxShader->setVec4("u_Color", color);

            debugBoxMeshGPU->vao->bind();
            RenderCommand::drawLinesIndexed(debugBoxMeshGPU->index_count);
        }

        glEnable(GL_CULL_FACE);
        setSceneFramebufferDrawBuffers(true);

        framebuffer->unbind();
    }

    RenderSystem::DebugLineMeshGPU* RenderSystem::getOrCreateDebugBoxMeshGPU()
    {
        if (m_HasDebugBoxMeshGPU)
            return &m_DebugBoxMeshGPU;

        static constexpr std::array<glm::vec3, 8> kBoxVertices = {
            glm::vec3{-0.5f, -0.5f, -0.5f},
            glm::vec3{ 0.5f, -0.5f, -0.5f},
            glm::vec3{ 0.5f,  0.5f, -0.5f},
            glm::vec3{-0.5f,  0.5f, -0.5f},
            glm::vec3{-0.5f, -0.5f,  0.5f},
            glm::vec3{ 0.5f, -0.5f,  0.5f},
            glm::vec3{ 0.5f,  0.5f,  0.5f},
            glm::vec3{-0.5f,  0.5f,  0.5f},
        };

        static constexpr std::array<uint32_t, 24> kBoxLineIndices = {
            0, 1, 1, 2, 2, 3, 3, 0,
            4, 5, 5, 6, 6, 7, 7, 4,
            0, 4, 1, 5, 2, 6, 3, 7,
        };

        m_DebugBoxMeshGPU.vb =
            VertexBuffer::Create(kBoxVertices.data(), static_cast<uint32_t>(kBoxVertices.size() * sizeof(glm::vec3)));
        m_DebugBoxMeshGPU.ib =
            IndexBuffer::Create(kBoxLineIndices.data(), static_cast<uint32_t>(kBoxLineIndices.size()));
        m_DebugBoxMeshGPU.vao = VertexArray::Create();

        VertexLayout layout;
        layout.stride = sizeof(glm::vec3);
        layout.attributes = {
            {0, 3, 0, false},
        };

        m_DebugBoxMeshGPU.vao->setVertexBuffer(m_DebugBoxMeshGPU.vb, layout);
        m_DebugBoxMeshGPU.vao->setIndexBuffer(m_DebugBoxMeshGPU.ib);
        m_DebugBoxMeshGPU.index_count = static_cast<uint32_t>(kBoxLineIndices.size());
        m_HasDebugBoxMeshGPU = true;
        return &m_DebugBoxMeshGPU;
    }

    void RenderSystem::executeGridPass(const RenderPacket& packet, void* glfwWindowHandle)
    {
        // TODO: editor grid render pass.
        (void)packet;
        (void)glfwWindowHandle;
    }

    void RenderSystem::executeShadowPass(const RenderPacket& packet, void* glfwWindowHandle)
    {
        // TODO: shadow map pass.
        (void)packet;
        (void)glfwWindowHandle;
    }

    void RenderSystem::executePostProcessPass(const RenderPacket& packet, void* glfwWindowHandle)
    {
        // TODO: post-process pass chain.
        (void)packet;
        (void)glfwWindowHandle;
    }

    void RenderSystem::executeDebugNormalsPass(const RenderPacket& packet, void* glfwWindowHandle)
    {
        // TODO: debug normal visualization pass.
        (void)packet;
        (void)glfwWindowHandle;
    }

    void RenderSystem::renderFrame(const FrameContext& frame_context,
                                   RenderFlags flags,
                                   const EditorRenderExt* editor_ext)
    {
        void* window_handle = frame_context.window_handle;
        if (!window_handle)
            return;

        if (!m_Initialized)
            initialize(window_handle);

        if (flags == RenderFlags::None)
            return;

        if (editor_ext && (editor_ext->render_scene_view || editor_ext->render_game_view))
        {
            bool rendered_any = false;

            if (editor_ext->render_scene_view &&
                editor_ext->scene_viewport_size.x > 0.0f &&
                editor_ext->scene_viewport_size.y > 0.0f)
            {
                FrameContext scene_frame = frame_context;
                scene_frame.viewport_size = editor_ext->scene_viewport_size;
                ensureFramebufferSize(m_SceneFB,
                                      static_cast<uint32_t>(scene_frame.viewport_size.x),
                                      static_cast<uint32_t>(scene_frame.viewport_size.y));

                EditorRenderExt scene_ext = *editor_ext;
                scene_ext.use_game_camera = false;
                auto scene_packet = buildRenderPacket(scene_frame, flags, &scene_ext, true);
                executePasses(scene_packet, flags, window_handle, m_SceneFB);
                rendered_any = true;
            }

            if (editor_ext->render_game_view &&
                editor_ext->game_viewport_size.x > 0.0f &&
                editor_ext->game_viewport_size.y > 0.0f)
            {
                FrameContext game_frame = frame_context;
                game_frame.viewport_size = editor_ext->game_viewport_size;
                ensureFramebufferSize(m_GameFB,
                                      static_cast<uint32_t>(game_frame.viewport_size.x),
                                      static_cast<uint32_t>(game_frame.viewport_size.y));

                EditorRenderExt game_ext = *editor_ext;
                game_ext.use_game_camera = true;
                game_ext.has_editor_camera = false;
                const RenderFlags game_flags = RenderFlags::Forward;
                auto game_packet = buildRenderPacket(game_frame, game_flags, &game_ext, false);
                executePasses(game_packet, game_flags, window_handle, m_GameFB);
                rendered_any = true;
            }

            if (rendered_any)
                return;
        }

        if (frame_context.viewport_size.x <= 0.0f || frame_context.viewport_size.y <= 0.0f)
            return;

        ensureFramebufferSize(m_SceneFB,
                              static_cast<uint32_t>(frame_context.viewport_size.x),
                              static_cast<uint32_t>(frame_context.viewport_size.y));

        auto packet = buildRenderPacket(frame_context, flags, editor_ext, true);
        executePasses(packet, flags, window_handle, m_SceneFB);
    }
    uint32_t RenderSystem::readEntityID(int x, int y) const
    {
        if (!m_SceneFB)
            return 0;
        if (x < 0 || y < 0)
            return 0;
        if (x >= static_cast<int>(m_SceneFB->getWidth()) ||
            y >= static_cast<int>(m_SceneFB->getHeight()))
            return 0;

        m_SceneFB->bind();

        // 璇?COLOR1
        glReadBuffer(GL_COLOR_ATTACHMENT1);

        uint32_t encoded_id = 0;
        glReadPixels(x, y, 1, 1, GL_RED_INTEGER, GL_UNSIGNED_INT, &encoded_id);

        m_SceneFB->unbind();
        return decodeEntityID(encoded_id);
    }

}
