#include <algorithm>
#include <chrono>
#include <string>

#include "vklive/camera.h"

glm::quat utils_quat_from_vectors(glm::vec3 u, glm::vec3 v)
{
    float norm_u_norm_v = sqrt(dot(u, u) * dot(v, v));
    float real_part = norm_u_norm_v + dot(u, v);
    glm::vec3 w;

    if (real_part < 1.e-6f * norm_u_norm_v)
    {
        /* If u and v are exactly opposite, rotate 180 degrees
         * around an arbitrary orthogonal axis. Axis normalisation
         * can happen later, when we normalise the quaternion. */
        real_part = 0.0f;
        w = std::abs(u.x) > std::abs(u.z) ? glm::vec3(-u.y, u.x, 0.f)
                                : glm::vec3(0.f, -u.z, u.y);
    }
    else
    {
        /* Otherwise, build quaternion the standard way. */
        w = cross(u, v);
    }

    return glm::normalize(glm::quat(real_part, w.x, w.y, w.z));
}

void camera_set_pos_lookat(Camera& cam, const glm::vec3& pos, const glm::vec3& point)
{
    // From
    cam.position = pos;

    // Focal
    cam.focalPoint = point;

    // Work out direction
    cam.viewDirection = cam.focalPoint - cam.position;
    cam.viewDirection = glm::normalize(cam.viewDirection);

    // Get camera orientation relative to -z
    cam.orientation = utils_quat_from_vectors(cam.viewDirection, glm::vec3(0.0f, 0.0f, -1.0f));
    cam.orientation = glm::normalize(cam.orientation);

    camera_update_right_up(cam);
}

void camera_set_ortho(Camera& cam, const glm::vec2& x, const glm::vec2& y, const glm::vec2& z)
{
    cam.orthoX = x;
    cam.orthoY = y;
    cam.orthoZ = z;
    cam.m_type = CameraType::Ortho;
}

void camera_set_film_size(Camera& cam, const glm::ivec2& displaySize)
{
    cam.filmWidth = (float)displaySize.x;
    cam.filmHeight = (float)displaySize.y;
    cam.aspectRatio = cam.filmWidth / cam.filmHeight;
}

bool camera_pre_render(Camera& cam)
{
    // The half-width of the viewport, in world space
    cam.halfAngle = float(tan(glm::radians(cam.fieldOfView) / 2.0));

    auto time = utils_get_time();

    int64_t delta;
    if (cam.lastTime == 0)
    {
        cam.lastTime = time;
        delta = 0;
    }
    else
    {
        delta = time - cam.lastTime;
        cam.lastTime = time;
    }

    bool changed = false;
    if (cam.orbitDelta != glm::vec2(0.0f))
    {
        camera_update_orbit(cam, delta);
        changed = true;
    }

    if (cam.positionDelta != glm::vec3(0.0f))
    {
        camera_update_position(cam, delta);
        changed = true;
    }
    camera_update_right_up(cam);
    return changed;
}

int64_t utils_get_time()
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now().time_since_epoch()).count();
}

// Given a screen coordinate, return a ray leaving the camera and entering the world at that 'pixel'
Ray camera_get_world_ray(Camera& cam, const glm::vec2& imageSample)
{
    // Could move some of this maths out of here for speed, but this isn't time critical
    // glm::vec3 dir(viewDirection);

    auto lensRand = glm::circularRand(0.14f);

    auto dir = cam.viewDirection;
    float x = ((imageSample.x * 2.0f) / cam.filmWidth) - 1.0f;
    float y = ((imageSample.y * 2.0f) / cam.filmHeight) - 1.0f;

    // Take the view direction and adjust it to point at the given sample, based on the
    // the frustum
    dir += (cam.right * (cam.halfAngle * cam.aspectRatio * x));
    dir -= (cam.up * (cam.halfAngle * y));
    // dir = normalize(dir);
    float ft = (glm::length(cam.focalPoint - cam.position) - 1.0f) / glm::length(dir);
    glm::vec3 focasPoint = cam.position + dir * ft;

    glm::vec3 lensPoint = cam.position;
    lensPoint += (cam.right * lensRand.x);
    lensPoint += (cam.up * lensRand.y);
    dir = glm::normalize(focasPoint - lensPoint);

    return Ray{ lensPoint, dir };
}

void camera_dolly(Camera& cam, float distance)
{
    cam.positionDelta += cam.viewDirection * distance;
}

// Orbit around the focal point, keeping y 'Up'
void camera_orbit(Camera& cam, const glm::vec2& angle)
{
    cam.orbitDelta += angle;
}

float utils_smooth_step(float val)
{
    return val * val * (3.0f - 2.0f * val);
}

void camera_update_position(Camera& cam, int64_t timeDelta)
{
    const float settlingTimeMs = 50;
    float frac = std::min(timeDelta / settlingTimeMs, 1.0f);
    frac = utils_smooth_step(frac);
    glm::vec3 distance = frac * cam.positionDelta;
    cam.positionDelta *= (1.0f - frac);

    cam.position += distance;
}

void camera_update_orbit(Camera& cam, int64_t timeDelta)
{
    const float settlingTimeMs = 50;
    float frac = std::min(timeDelta / settlingTimeMs, 1.0f);
    frac = utils_smooth_step(frac);

    // Get a proportion of the remaining turn angle, based on the time delta
    glm::vec2 angle = frac * cam.orbitDelta;

    // Reduce the orbit delta remaining for next time
    cam.orbitDelta *= (1.0f - frac);
    if (glm::all(glm::lessThan(glm::abs(cam.orbitDelta), glm::vec2(.1f))))
    {
        cam.orbitDelta = glm::vec2(0.0f);
    }

    // 2 rotations, about right and world up, for the camera
    glm::quat rotY = glm::angleAxis(glm::radians(angle.y), glm::vec3(cam.right));
    glm::quat rotX = glm::angleAxis(glm::radians(angle.x), glm::vec3(0.0f, 1.0f, 0.0f));

    // Concatentation of the current rotations with the new one
    cam.orientation = cam.orientation * rotY * rotX;
    cam.orientation = glm::normalize(cam.orientation);

    // Recalculate position from the new view direction, relative to the focal point
    float distance = glm::length(cam.focalPoint - cam.position);
    cam.viewDirection = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f) * cam.orientation);
    cam.position = cam.focalPoint - (cam.viewDirection * distance);

    camera_update_right_up(cam);
}

// For setting up 3D scenes
// The current lookat matrix
glm::mat4 camera_get_lookat(Camera& cam)
{
    glm::vec3 up = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) * cam.orientation);
    return glm::lookAt(cam.position, cam.focalPoint, up);
}

// The current projection matrix
// This is different depending on the API
glm::mat4 camera_get_projection(Camera& cam)
{
    glm::mat4 projection;
    if (cam.m_type == CameraType::Perspective)
    {
        projection = glm::perspectiveFov(glm::radians(cam.fieldOfView), float(cam.filmWidth), float(cam.filmHeight), cam.nearFar.x, cam.nearFar.y);
    }
    else
    {
        projection = glm::ortho(cam.orthoX.x, cam.orthoX.y, cam.orthoY.x, cam.orthoY.y, cam.orthoZ.x, cam.orthoZ.y);
    }

    return projection;
}

void camera_set_near_far(Camera& cam, const glm::vec2& nf)
{
    cam.nearFar = nf;
}

void camera_update_right_up(Camera& cam)
{
    // Right and up vectors updated based on the quaternion orientation
    cam.right = glm::normalize(glm::vec3(1.0f, 0.0f, 0.0f) * cam.orientation);
    cam.up = glm::normalize(glm::vec3(0.0f, 1.0f, 0.0f) * cam.orientation);
}
