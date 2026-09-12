#pragma once
#include "runtime/modules/render/public/framebuffer.h"

#include <vector>

namespace Hybrid {

    class IRenderDevice;

    // GLFramebuffer: framebuffer object with configurable color attachments + sampleable depth.
    class GLFramebuffer final : public Framebuffer {
    public:
        GLFramebuffer(const FramebufferSpec& spec, IRenderDevice& device);
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

        TextureViewHandle getColorAttachmentView(uint32_t index = 0) const override;
        uint32_t getColorAttachmentCount() const override { return static_cast<uint32_t>(m_ColorAttachmentViews.size()); }
        TextureViewHandle getDepthAttachmentView() const override { return m_DepthAttachmentView; }
        uint32_t getWidth() const override { return m_Spec.width; }
        uint32_t getHeight() const override { return m_Spec.height; }

    private:
        void invalidate();
        void releaseAttachments();

    private:
        uint32_t m_FBO = 0;
        IRenderDevice* m_Device = nullptr;
        std::vector<TextureHandle> m_ColorAttachments;
        std::vector<TextureViewHandle> m_ColorAttachmentViews;
        std::vector<uint32_t> m_ColorAttachmentNativeHandles;
        TextureHandle m_DepthAttachment;
        TextureViewHandle m_DepthAttachmentView;
        uint32_t m_DepthAttachmentNativeHandle = 0;

        FramebufferSpec m_Spec{};
    };

} // namespace Hybrid
