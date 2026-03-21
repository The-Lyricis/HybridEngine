#pragma once
#include "runtime/modules/render/public/framebuffer.h"

#include <vector>

namespace Hybrid {

    // GLFramebuffer: framebuffer object with configurable color attachments + sampleable depth.
    class GLFramebuffer final : public Framebuffer {
    public:
        explicit GLFramebuffer(const FramebufferSpec& spec);
        ~GLFramebuffer() override;

        void bind() const override;
        void unbind() const override;
        void resize(uint32_t w, uint32_t h) override;
        void setDrawColorAttachments(std::initializer_list<uint32_t> indices) const override;
        void clearColorAttachmentUInt(uint32_t index, uint32_t value) const override;
        uint32_t readPixelUInt(uint32_t attachment_index, int x, int y) const override;
        void copyColorAttachmentTo(const Framebuffer& dst,
                                   uint32_t src_index,
                                   uint32_t dst_index) const override;
        void copyDepthAttachmentTo(const Framebuffer& dst) const override;
        void bindColorAttachmentTexture(uint32_t index, uint32_t slot) const override;
        void bindDepthAttachmentTexture(uint32_t slot) const override;

        uint32_t getColorAttachmentRendererID(uint32_t index = 0) const override;
        uint32_t getColorAttachmentCount() const override { return static_cast<uint32_t>(m_ColorAttachments.size()); }
        uint32_t getDepthAttachmentRendererID() const override { return m_DepthAttachment; }
        uint32_t getWidth() const override { return m_Spec.width; }
        uint32_t getHeight() const override { return m_Spec.height; }

    private:
        void invalidate();

    private:
        uint32_t m_FBO = 0;
        std::vector<uint32_t> m_ColorAttachments;
        uint32_t m_DepthAttachment = 0;

        FramebufferSpec m_Spec{};
    };

} // namespace Hybrid
