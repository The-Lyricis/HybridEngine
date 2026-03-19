#pragma once
#include <cstdint>
#include <initializer_list>
#include <memory>
#include <vector>

namespace Hybrid {

    enum class FramebufferTextureFormat
    {
        None = 0,
        RGBA8,
        R8,
        R32UI,
        Depth24Stencil8,
        Depth32F
    };

    struct FramebufferAttachmentSpec
    {
        FramebufferAttachmentSpec() = default;
        FramebufferAttachmentSpec(std::initializer_list<FramebufferTextureFormat> list)
            : attachments(list)
        {
        }

        std::vector<FramebufferTextureFormat> attachments;
    };

    struct FramebufferSpec {
        uint32_t width = 1280;
        uint32_t height = 720;
        FramebufferAttachmentSpec attachment_spec = {
            FramebufferTextureFormat::RGBA8,
            FramebufferTextureFormat::R32UI,
            FramebufferTextureFormat::Depth32F
        };
    };

    // Framebuffer: off-screen render target abstraction.
    class Framebuffer {
    public:
        virtual ~Framebuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;
        virtual void resize(uint32_t w, uint32_t h) = 0;

        virtual uint32_t getColorAttachmentRendererID(uint32_t index = 0) const = 0;
        virtual uint32_t getColorAttachmentCount() const = 0;
        virtual uint32_t getDepthAttachmentRendererID() const = 0;

        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;

        static std::shared_ptr<Framebuffer> Create(const FramebufferSpec& spec);


    };

} // namespace Hybrid
