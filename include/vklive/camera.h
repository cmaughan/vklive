#pragma once

#include "glm/gtc/quaternion.hpp"
#include "glm/gtc/random.hpp"
#include "glm/gtx/rotate_vector.hpp"

struct Ray
{
    glm::vec3 position;
    glm::vec3 direction;
};

enum class CameraType
{
    Perspective,
    Ortho
};

struct Camera
{
    Camera(const std::string& n)
        : name(n)
    {
    }

    glm::vec3 position = glm::vec3(0.0f); // Position of the camera in world space
    glm::vec3 focalPoint = glm::vec3(0.0f); // Look at point
    glm::vec2 nearFar = glm::vec2(.1f, 100.0f);

    float filmWidth = 1.0f; // Width/height of the film
    float filmHeight = 1.0f;

    glm::vec2 orthoX;
    glm::vec2 orthoY;
    glm::vec2 orthoZ;

    glm::vec3 viewDirection = glm::vec3(0.0f, 0.0f, 1.0f); // The direction the camera is looking in
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f); // The vector to the right
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f); // The vector up

    float fieldOfView = 60.0f; // Field of view
    float halfAngle = 30.0f; // Half angle of the view frustum
    float aspectRatio = 1.0f; // Ratio of x to y of the viewport

    glm::quat orientation; // A quaternion representing the camera rotation

    glm::vec2 orbitDelta = glm::vec2(0.0f);
    glm::vec3 positionDelta = glm::vec3(0.0f);

    int64_t lastTime = 0;
    CameraType m_type = CameraType::Perspective;

    std::string name;
};

void camera_set_pos_lookat(Camera& cam, const glm::vec3& pos, const glm::vec3& point);
void camera_set_ortho(Camera& cam, const glm::vec2& x, const glm::vec2& y, const glm::vec2& z);
void camera_set_film_size(Camera& cam, const glm::ivec2& displaySize);
bool camera_pre_render(Camera& cam);
Ray camera_get_world_ray(Camera& cam, const glm::vec2& imageSample);
void camera_dolly(Camera& cam, float distance);
void camera_orbit(Camera& cam, const glm::vec2& angle);
void camera_update_position(Camera& cam, int64_t timeDelta);
void camera_update_orbit(Camera& cam, int64_t timeDelta);
glm::mat4 camera_get_lookat(Camera& cam);
glm::mat4 camera_get_projection(Camera& cam);
void camera_set_near_far(Camera& cam, const glm::vec2& nf);
void camera_update_right_up(Camera& cam);

int64_t utils_get_time();
float utils_smooth_step(float val);
glm::quat utils_quat_from_vectors(glm::vec3 u, glm::vec3 v);
