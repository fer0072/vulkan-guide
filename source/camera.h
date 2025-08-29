#include "SDL_events.h"
#include <vk_types.h>

class Camera {
public:
    glm::vec3 velocity = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 inputAxis = glm::vec3(0.0f, 0.0f, 0.0f);

    float pitch = 0.0f; // vertical rotation
    float yaw = 0.f; // horizontal rotation

    bool bSprint = false;
    bool bLocked = false;

    void process_input_event(SDL_Event* ev);
    void update_camera(float deltaSeconds);

    glm::mat4 get_view_matrix() const;
    glm::mat4 get_projection_matrix(bool bReverse = true) const;
    glm::mat4 get_rotation_matrix() const;
};
