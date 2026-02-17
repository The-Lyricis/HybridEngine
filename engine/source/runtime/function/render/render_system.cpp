#include "render_system.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstddef>
#include <array>
#include <string>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/euler_angles.hpp>

#include "runtime/function/render/renderer.h"
#include "runtime/function/render/render_command.h"
#include "runtime/function/render/vertex_array.h"
#include "runtime/function/render/shader.h"
#include "runtime/function/render/framebuffer.h"
#include "runtime/function/render/texture.h"
#include "runtime/function/scene/scene.h"
#include "runtime/function/scene/components.h"
#include "runtime/core/base/macro.h"
#include "runtime/function/asset/asset_manager.h"
#include "runtime/function/asset/material.h"
#include "runtime/function/asset/mesh.h"

namespace Hybrid
{

    namespace
    {
        // TransformComponent -> Model Matrix
        static glm::mat4 BuildModel(const Hybrid::TransformComponent &tr)
        {
            glm::mat4 T = glm::translate(glm::mat4(1.0f), tr.Position);
            glm::mat4 R = glm::yawPitchRoll(tr.Rotation.y, tr.Rotation.x, tr.Rotation.z); // yaw, pitch, roll
            glm::mat4 S = glm::scale(glm::mat4(1.0f), tr.Scale);
            return T * R * S;
        }

        // Scene Camera (Transform + CameraComponent) -> ViewProjection
        static bool TryGetSceneViewProj(Hybrid::Scene &scene, float aspect, glm::mat4 &outViewProj)
        {
            auto &reg = scene.GetRegistry();
            auto view = reg.view<Hybrid::TransformComponent, Hybrid::CameraComponent>();

            entt::entity mainCam = entt::null;
            for (auto e : view)
            {
                auto &cam = view.get<Hybrid::CameraComponent>(e);
                if (cam.Primary)
                {
                    mainCam = e;
                    break;
                }
            }
            if (mainCam == entt::null)
                return false;

            const auto &tr = reg.get<Hybrid::TransformComponent>(mainCam);
            const auto &cam = reg.get<Hybrid::CameraComponent>(mainCam);

            // Projection
            glm::mat4 proj = glm::perspective(glm::radians(cam.FovY), aspect, cam.Near, cam.Far);

            // View: inverse of camera world transform
            glm::mat4 T = glm::translate(glm::mat4(1.0f), tr.Position);
            glm::mat4 R = glm::yawPitchRoll(tr.Rotation.y, tr.Rotation.x, tr.Rotation.z);
            glm::mat4 viewMat = glm::inverse(T * R);

            outViewProj = proj * viewMat;
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
        shader.setInt("u_HasNormalMap", normal ? 1 : 0);
        int slot = 0;
        if (albedo)
            albedo->bind(slot++);
        if (normal)
            normal->bind(slot++);
        if (mr)
            mr->bind(slot++);
        if (ao)
            ao->bind(slot++);
        if (emissive)
            emissive->bind(slot++);
    }

    void RenderSystem::initialize(void *glfwWindowHandle)
    {
        if (m_Initialized)
            return;

        Renderer::initialize();
        createCubeResources();
        createMeshShader();

        GLFWwindow *window = static_cast<GLFWwindow *>(glfwWindowHandle);
        int w = 0, h = 0;
        glfwGetFramebufferSize(window, &w, &h);

        FramebufferSpec spec;
        spec.width = (uint32_t)std::max(1, w);
        spec.height = (uint32_t)std::max(1, h);
        m_SceneFB = Framebuffer::Create(spec);

        // 相机初始投影（后续每帧会随 viewport 变化）
        m_Camera.setViewportSize((float)spec.width, (float)spec.height);

        m_Initialized = true;

        HBD_CORE_TRACE("RenderSystem initialized");
    }

    void RenderSystem::createCubeResources()
    {
        static float s_CubeVertices[] = {
            // x,    y,    z,    r,   g,   b,   a
            -0.5f, -0.5f, -0.5f, 1.f, 0.f, 0.f, 1.f, // 0
            0.5f, -0.5f, -0.5f, 0.f, 1.f, 0.f, 1.f,  // 1
            0.5f, 0.5f, -0.5f, 0.f, 0.f, 1.f, 1.f,   // 2
            -0.5f, 0.5f, -0.5f, 1.f, 1.f, 0.f, 1.f,  // 3
            -0.5f, -0.5f, 0.5f, 1.f, 0.f, 1.f, 1.f,  // 4
            0.5f, -0.5f, 0.5f, 0.f, 1.f, 1.f, 1.f,   // 5
            0.5f, 0.5f, 0.5f, 1.f, 1.f, 1.f, 1.f,    // 6
            -0.5f, 0.5f, 0.5f, 0.2f, 0.2f, 0.2f, 1.f // 7
        };

        static uint32_t s_CubeIndices[] = {
            // back (-Z)
            0, 1, 2, 2, 3, 0,
            // front (+Z)
            4, 5, 6, 6, 7, 4,
            // left (-X)
            0, 3, 7, 7, 4, 0,
            // right (+X)
            1, 5, 6, 6, 2, 1,
            // bottom (-Y)
            0, 1, 5, 5, 4, 0,
            // top (+Y)
            3, 2, 6, 6, 7, 3};

        m_CubeVAO = VertexArray::Create();
        auto vb = VertexBuffer::Create(
            s_CubeVertices,
            static_cast<uint32_t>(sizeof(s_CubeVertices)));
        auto ib = IndexBuffer::Create(
            s_CubeIndices,
            (uint32_t)(sizeof(s_CubeIndices) / sizeof(uint32_t)));

        VertexLayout cubeLayout;
        cubeLayout.stride = (3 + 4) * sizeof(float);
        cubeLayout.attributes = {
            {0, 3, 0, false},
            {1, 4, static_cast<uint32_t>(3 * sizeof(float)), false}
        };
        m_CubeVAO->setVertexBuffer(vb, cubeLayout);
        m_CubeVAO->setIndexBuffer(ib);

        const std::string vs = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec4 aColor;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec4 vColor;

void main() {
    vColor = aColor;
    gl_Position = u_ViewProjection * u_Model * vec4(aPos, 1.0);
}
)";

        const std::string fs = R"(
#version 330 core
in vec4 vColor;
out vec4 FragColor;
void main() {
    FragColor = vColor;
}
)";

        m_CubeShader = Shader::Create(vs, fs);
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
            {3, 3, static_cast<uint32_t>(offsetof(MeshVertex, tangent)), false},
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
        if (auto it = m_MatCache.find(id); it != m_MatCache.end())
            return &it->second;

        ensureDefaultTextures();

        MaterialGPU mgpu;
        mgpu.data = mat->getData();

        auto texOrDefault = [&](AssetID tid, const TexturePtr &fallback) -> TexturePtr
        {
            if (!m_AssetManager)
                return fallback;
            if (tid.value == 0)
                return fallback;
            auto t = m_AssetManager->loadSync<Texture>(tid);
            return t ? t : fallback;
        };

        mgpu.albedo = texOrDefault(mgpu.data.albedo_map, m_DefaultAlbedoTex);
        mgpu.normal = texOrDefault(mgpu.data.normal_map, m_DefaultNormalTex);
        mgpu.mr = texOrDefault(mgpu.data.metallic_roughness_map, m_DefaultMRTex);
        mgpu.ao = texOrDefault(mgpu.data.ao_map, m_DefaultAOTex);
        mgpu.emissive = texOrDefault(mgpu.data.emissive_map, m_DefaultEmissiveTex);

        auto [it, inserted] = m_MatCache.emplace(id, std::move(mgpu));
        return &it->second;
    }

    RenderSystem::MeshGPU *RenderSystem::getDefaultMeshGPU()
    {
        if (!m_AssetManager)
            return nullptr;
        auto defMesh = m_AssetManager->getDefault<Mesh>();
        if (!defMesh)
            return nullptr;
        return getOrCreateMeshGPU(AssetID{}, defMesh);
    }

    RenderSystem::MaterialGPU *RenderSystem::getDefaultMaterialGPU()
    {
        if (!m_AssetManager)
            return nullptr;
        auto defMat = m_AssetManager->getDefault<Material>();
        if (!defMat)
            return nullptr;
        return getOrCreateMaterialGPU(AssetID{}, defMat);
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

    void RenderSystem::ensureDefaultTextures()
    {
        if (!m_DefaultAlbedoTex)
            m_DefaultAlbedoTex = createSolidTexture(255, 255, 255, 255);
        if (!m_DefaultNormalTex)
            m_DefaultNormalTex = createSolidTexture(128, 128, 255, 255); // normal blue
        if (!m_DefaultMRTex)
            m_DefaultMRTex = createSolidTexture(0, 255, 0, 255);         // metal=0, rough=1
        if (!m_DefaultAOTex)
            m_DefaultAOTex = createSolidTexture(255, 255, 255, 255);     // ao=1
        if (!m_DefaultEmissiveTex)
            m_DefaultEmissiveTex = createSolidTexture(0, 0, 0, 255);     // black
    }

    void RenderSystem::createMeshShader()
    {
        const std::string vs = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec2 aUV;
layout(location=3) in vec3 aTangent;

uniform mat4 u_ViewProjection;
uniform mat4 u_Model;

out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out vec3 vTangent;

void main() {
    mat3 normalMat = transpose(inverse(mat3(u_Model)));
    vWorldPos = vec3(u_Model * vec4(aPos, 1.0));
    vNormal   = normalMat * aNormal;
    vTangent  = normalMat * aTangent;
    vUV       = aUV;
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
in vec3 vTangent;
out vec4 FragColor;

uniform vec3 u_CameraPos;
uniform DirLight u_DirLight;
uniform PointLight u_PointLights[MAX_POINT_LIGHTS];
uniform int  u_PointCount;

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
    if (u_HasNormalMap == 0)
        return normalize(vNormal);
    vec3 N = normalize(vNormal);
    vec3 T = normalize(vTangent);
    vec3 B = normalize(cross(N, T));
    vec3 nMap = texture(u_NormalMap, vUV).xyz * 2.0 - 1.0;
    mat3 TBN = mat3(T, B, N);
    return normalize(TBN * nMap);
}

void main() {
    vec3 albedo = (u_AlbedoColor * u_TintColor).rgb * texture(u_AlbedoMap, vUV).rgb;
    float metallic  = clamp(u_Metallic + texture(u_MRMap, vUV).r, 0.0, 1.0);
    float roughness = clamp(u_Roughness + texture(u_MRMap, vUV).g, 0.04, 1.0);
    float ao        = clamp(u_AO * texture(u_AOMap, vUV).r, 0.0, 1.0);
    float emissive  = u_Emissive + texture(u_EmissiveMap, vUV).r;

    vec3 N = getNormal();
    vec3 V = normalize(u_CameraPos - vWorldPos);

    vec3 ambient = 0.1 * albedo * ao;
    vec3 color = ambient;

    // Directional light
    vec3 Ld = normalize(-u_DirLight.direction);
    float ndl = max(dot(N, Ld), 0.0);
    vec3 H = normalize(Ld + V);
    float specPow = mix(8.0, 128.0, 1.0 - roughness);
    float spec = pow(max(dot(N, H), 0.0), specPow);
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    vec3 ks = F0;
    vec3 kd = (vec3(1.0) - ks) * (1.0 - metallic);
    color += (kd * ndl * albedo + ks * spec) * u_DirLight.color * u_DirLight.intensity;

    // Point lights
    for (int i = 0; i < u_PointCount && i < MAX_POINT_LIGHTS; ++i) {
        vec3 Lp = u_PointLights[i].position - vWorldPos;
        float dist = length(Lp);
        if (dist > u_PointLights[i].range) continue;
        Lp = Lp / dist;
        float att = 1.0 - clamp(dist / u_PointLights[i].range, 0.0, 1.0);
        float ndl_p = max(dot(N, Lp), 0.0);
        vec3 Hp = normalize(Lp + V);
        float specp = pow(max(dot(N, Hp), 0.0), specPow);
        color += att * ((kd * ndl_p * albedo + ks * specp) * u_PointLights[i].color * u_PointLights[i].intensity);
    }

   
    FragColor = vec4(color, u_AlbedoColor.a * u_TintColor.a);
}
)";

        m_MeshShader = Shader::Create(vs, fs);
    }

    void RenderSystem::ensureFramebufferSize(uint32_t w, uint32_t h)
    {
        w = std::max(1u, w);
        h = std::max(1u, h);

        if (!m_SceneFB)
        {
            FramebufferSpec spec{w, h};
            m_SceneFB = Framebuffer::Create(spec);
        }
        else
        {
            m_SceneFB->resize(w, h);
        }

        // 相机投影随 viewport 变化
        m_Camera.setViewportSize((float)w, (float)h);
    }

    uint32_t RenderSystem::getSceneColorTexture() const
    {
        return m_SceneFB ? m_SceneFB->getColorAttachmentRendererID() : 0;
    }

    void RenderSystem::onWindowResize(uint32_t width, uint32_t height)
    {
        if (!m_Initialized)
            return;
        ensureFramebufferSize(width, height);
    }

    void RenderSystem::renderFrame(const glm::vec2 &viewportSize,
                                   void *glfwWindowHandle,
                                   float dt,
                                   bool viewportActive,
                                   const InputState &input,
                                   bool useGameCamera)
    {
        if (!m_Initialized)
            initialize(glfwWindowHandle);

        // 1) FBO 尺寸匹配 viewport
        ensureFramebufferSize((uint32_t)viewportSize.x, (uint32_t)viewportSize.y);

        // 2) 解析输入（只在 Editor 预览且 viewportActive 时更新 EditorCamera）
        const bool rmbDown = (!useGameCamera) && viewportActive && input.isMouseDown(GLFW_MOUSE_BUTTON_RIGHT);

        const float mdx = (!useGameCamera && viewportActive) ? input.getMouseDeltaX() : 0.0f;
        const float mdy = (!useGameCamera && viewportActive) ? input.getMouseDeltaY() : 0.0f;
        const float scrollY = (!useGameCamera && viewportActive) ? input.getScrollDeltaY() : 0.0f;

        const bool keyW = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_W);
        const bool keyA = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_A);
        const bool keyS = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_S);
        const bool keyD = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_D);
        const bool keyQ = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_Q);
        const bool keyE = (!useGameCamera) && viewportActive && input.isKeyDown(GLFW_KEY_E);

        // 3) 更新 Editor 预览相机（游戏相机不吃 Editor 输入）
        if (!useGameCamera)
        {
            m_Camera.update(dt, viewportActive, rmbDown, mdx, mdy,
                            keyW, keyA, keyS, keyD, keyQ, keyE,
                            scrollY);
        }

        // 4) 选择 ViewProjection：EditorCamera 或 Scene Primary Camera
        glm::mat4 viewProj(1.0f);

        const float aspect = (viewportSize.y > 0.0f) ? (viewportSize.x / viewportSize.y) : 1.0f;

        if (useGameCamera)
        {
            if (!m_Scene)
            {
                // 没有场景：退回 editor camera
                viewProj = m_Camera.getViewProj();
            }
            else
            {
                // Scene 相机优先；若没找到 Primary Camera，则退回 editor camera
                if (!TryGetSceneViewProj(*m_Scene, aspect, viewProj))
                    viewProj = m_Camera.getViewProj();
            }
        }
        else
        {
            // Editor 预览
            viewProj = m_Camera.getViewProj();
        }

        glm::vec3 cameraPos = m_Camera.getPosition();
        if (useGameCamera && m_Scene)
        {
            auto& reg = m_Scene->GetRegistry();
            auto view = reg.view<Hybrid::TransformComponent, Hybrid::CameraComponent>();
            for (auto e : view)
            {
                auto& cam = view.get<Hybrid::CameraComponent>(e);
                if (cam.Primary)
                {
                    cameraPos = view.get<Hybrid::TransformComponent>(e).Position;
                    break;
                }
            }
        }

        // 光源收集
        DirLightGPU dirLight{};
        dirLight.intensity = 0.0f; // default no light
        std::array<PointLightGPU, kMaxPointLights> pointLights{};
        int pointCount = 0;

        if (m_Scene)
        {
            auto& reg = m_Scene->GetRegistry();
            auto dirView = reg.view<Hybrid::DirectionalLightComponent>();
            for (auto e : dirView)
            {
                const auto& dl = dirView.get<Hybrid::DirectionalLightComponent>(e);
                dirLight.color = dl.Color;
                dirLight.intensity = dl.Intensity;
                dirLight.direction = glm::normalize(dl.Direction);
                break; // only one dir light for now
            }

            auto ptView = reg.view<Hybrid::TransformComponent, Hybrid::PointLightComponent>();
            for (auto e : ptView)
            {
                if (pointCount >= kMaxPointLights) break;
                const auto& tc = ptView.get<Hybrid::TransformComponent>(e);
                const auto& pl = ptView.get<Hybrid::PointLightComponent>(e);
                pointLights[pointCount].color = pl.Color;
                pointLights[pointCount].intensity = pl.Intensity;
                pointLights[pointCount].position = tc.Position;
                pointLights[pointCount].range = pl.Range;
                ++pointCount;
            }
        }

        // 5) 渲染到 FBO
        m_SceneFB->bind();
        RenderCommand::setViewport(0, 0, m_SceneFB->getWidth(), m_SceneFB->getHeight());

        Renderer::beginFrame({0.1f, 0.1f, 0.12f, 1.0f});

        // 6) 从 Scene 读取可渲染实体并绘制
        if (m_Scene)
        {
            auto &registry = m_Scene->GetRegistry();
            auto renderView = registry.view<Hybrid::TransformComponent, Hybrid::MeshRendererComponent>();

            // 若缺少资产管理或 shader，退回旧的 cube 渲染
            const bool assetPathReady = (m_AssetManager != nullptr) && (m_MeshShader != nullptr);

            if (!assetPathReady)
            {
                if (m_CubeShader && m_CubeVAO)
                {
                    m_CubeShader->bind();
                    m_CubeShader->setMat4("u_ViewProjection", viewProj);
                    for (auto e : renderView)
                    {
                        const auto &tr = renderView.get<Hybrid::TransformComponent>(e);
                        const auto &mr = renderView.get<Hybrid::MeshRendererComponent>(e);
                        if (mr.Primitive != 0)
                            continue;
                        glm::mat4 model = BuildModel(tr);
                        m_CubeShader->setMat4("u_Model", model);
                        Renderer::submit(m_CubeVAO, m_CubeShader);
                    }
                }
            }
            else
            {
                if (m_MeshShader)
                {
                    m_MeshShader->bind();
                    m_MeshShader->setMat4("u_ViewProjection", viewProj);
                    m_MeshShader->setVec3("u_CameraPos", cameraPos);
                    m_MeshShader->setVec3("u_DirLight.color", dirLight.color);
                    m_MeshShader->setFloat("u_DirLight.intensity", dirLight.intensity);
                    m_MeshShader->setVec3("u_DirLight.direction", dirLight.direction);
                    m_MeshShader->setInt("u_PointCount", pointCount);
                    for (int i = 0; i < pointCount && i < kMaxPointLights; ++i)
                    {
                        std::string base = "u_PointLights[" + std::to_string(i) + "]";
                        m_MeshShader->setVec3(base + ".color", pointLights[i].color);
                        m_MeshShader->setFloat(base + ".intensity", pointLights[i].intensity);
                        m_MeshShader->setVec3(base + ".position", pointLights[i].position);
                        m_MeshShader->setFloat(base + ".range", pointLights[i].range);
                    }
                    m_MeshShader->setInt("u_AlbedoMap", 0);
                    m_MeshShader->setInt("u_NormalMap", 1);
                    m_MeshShader->setInt("u_MRMap", 2);
                    m_MeshShader->setInt("u_AOMap", 3);
                    m_MeshShader->setInt("u_EmissiveMap", 4);
                }

                for (auto e : renderView)
                {
                    const auto &tr = renderView.get<Hybrid::TransformComponent>(e);
                    const auto &mr = renderView.get<Hybrid::MeshRendererComponent>(e);

                    std::shared_ptr<Mesh> cpuMesh;
                    AssetID meshId = mr.Mesh;
                    if (meshId.value != 0)
                        cpuMesh = m_AssetManager->loadSync<Mesh>(meshId);
                    if (!cpuMesh)
                    {
                        cpuMesh = m_AssetManager->getDefault<Mesh>();
                        meshId = AssetID{};
                    }
                    auto *meshGPU = getOrCreateMeshGPU(meshId, cpuMesh);
                    if (!meshGPU)
                        continue;

                    // 材质选择：组件指定 > submesh 自带 > 默认
                    AssetID matId = mr.Material;
                    std::shared_ptr<Material> cpuMat;
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

                    glm::mat4 model = BuildModel(tr);

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

                        m_MeshShader->bind();
                        m_MeshShader->setMat4("u_ViewProjection", viewProj);
                        m_MeshShader->setMat4("u_Model", model);
                        m_MeshShader->setVec4("u_TintColor", mr.Tint);
                        useMat->bind(*m_MeshShader);

                        meshGPU->vao->bind();
                        RenderCommand::drawIndexed(sm.index_count, sm.index_offset);
                    }
                }
            }
        }

        Renderer::endFrame();
        m_SceneFB->unbind();

        // 7) 清屏默认帧缓冲（防 UI 拖影）
        GLFWwindow *window = static_cast<GLFWwindow *>(glfwWindowHandle);
        int display_w = 0, display_h = 0;
        glfwGetFramebufferSize(window, &display_w, &display_h);

        RenderCommand::setViewport(0, 0, display_w, display_h);
        RenderCommand::setClearColor({0.08f, 0.08f, 0.09f, 1.0f});
        RenderCommand::clear();
    }

}
