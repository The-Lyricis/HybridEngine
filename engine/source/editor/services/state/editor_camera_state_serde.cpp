#include "editor_camera_state_serde.h"

#include <nlohmann/json.hpp>

namespace Hybrid
{
    namespace
    {
        nlohmann::json vec3ToJson(const glm::vec3& value)
        {
            return nlohmann::json::array({value.x, value.y, value.z});
        }

        bool vec3FromJson(const nlohmann::json& value, glm::vec3& out)
        {
            if (!value.is_array() || value.size() != 3)
                return false;

            out.x = value[0].get<float>();
            out.y = value[1].get<float>();
            out.z = value[2].get<float>();
            return true;
        }
    } // namespace

    namespace EditorCameraStateSerde
    {
        nlohmann::json toJson(const EditorCameraState& state)
        {
            return {
                {"version", 1},
                {"camera", {
                    {"position", vec3ToJson(state.position)},
                    {"focal_point", vec3ToJson(state.focal_point)},
                    {"distance", state.distance},
                    {"yaw_deg", state.yaw_deg},
                    {"pitch_deg", state.pitch_deg}
                }}
            };
        }

        bool fromJson(const nlohmann::json& root, EditorCameraState& out_state)
        {
            if (!root.is_object())
                return false;

            const auto version = root.value("version", 0);
            if (version != 1)
                return false;

            const auto camera_it = root.find("camera");
            if (camera_it == root.end() || !camera_it->is_object())
                return false;

            EditorCameraState next = out_state;
            if (!vec3FromJson((*camera_it)["position"], next.position))
                return false;
            if (!vec3FromJson((*camera_it)["focal_point"], next.focal_point))
                return false;

            next.distance = camera_it->value("distance", next.distance);
            next.yaw_deg = camera_it->value("yaw_deg", next.yaw_deg);
            next.pitch_deg = camera_it->value("pitch_deg", next.pitch_deg);

            out_state = next;
            return true;
        }
    } // namespace EditorCameraStateSerde
} // namespace Hybrid
