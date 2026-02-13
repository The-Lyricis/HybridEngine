#pragma once
#include <cstdint>
#include <memory>

namespace Hybrid {

    struct FramebufferSpec {
        uint32_t width = 1280;
        uint32_t height = 720;
        // future: formats, samples, attachments
    };

    // Framebuffer: off-screen render target abstraction.
    class Framebuffer {
    public:
        virtual ~Framebuffer() = default;

        virtual void bind() const = 0;
        virtual void unbind() const = 0;
        virtual void resize(uint32_t w, uint32_t h) = 0;

        virtual uint32_t getColorAttachmentRendererID() const = 0;
        virtual uint32_t getWidth() const = 0;
        virtual uint32_t getHeight() const = 0;

        static std::shared_ptr<Framebuffer> Create(const FramebufferSpec& spec);
    };

} // namespace Hybrid
