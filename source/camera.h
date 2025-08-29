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

    void processInputEvent(SDL_Event* ev);
    void updateCamera(float deltaSeconds);

    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix(bool bReverse = true) const;
    glm::mat4 getRotationMatrix() const;
};
