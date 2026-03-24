#pragma once

#include <memory>

#include "editor/core/scene_document.h"

namespace Hybrid
{
    class Scene;

    class DocumentService
    {
    public:
        void setActiveDocument(std::shared_ptr<SceneDocument> document)
        {
            m_active_document = std::move(document);
            m_scene_override = nullptr;
        }

        void setSceneOverride(Scene* scene)
        {
            m_scene_override = scene;
        }

        void clearSceneOverride()
        {
            m_scene_override = nullptr;
        }

        void clear()
        {
            m_active_document.reset();
            m_scene_override = nullptr;
        }

        const std::shared_ptr<SceneDocument>& activeDocument() const
        {
            return m_active_document;
        }

        std::shared_ptr<SceneDocument>& activeDocument()
        {
            return m_active_document;
        }

        Scene* activeScene() const
        {
            if (m_scene_override != nullptr)
                return m_scene_override;
            return m_active_document && m_active_document->scene ? m_active_document->scene.get() : nullptr;
        }

        bool hasActiveDocument() const
        {
            return static_cast<bool>(m_active_document);
        }

        bool hasActiveScene() const
        {
            return activeScene() != nullptr;
        }

        void markDirty()
        {
            if (m_active_document)
                m_active_document->dirty = true;
        }

    private:
        std::shared_ptr<SceneDocument> m_active_document;
        Scene* m_scene_override = nullptr;
    };
} // namespace Hybrid
