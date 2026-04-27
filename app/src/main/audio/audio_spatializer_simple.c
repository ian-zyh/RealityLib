#include "audio_internal.h"
#include "audio_spatializer_simple.h"

#include <math.h>

static float clampf(float x, float a, float b) {
    return (x < a) ? a : (x > b) ? b : x;
}

static const float kPi = 3.14159265358979323846f;

static float dot3(Vector3 a, Vector3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

static Vector3 sub3(Vector3 a, Vector3 b) {
    Vector3 r = {a.x - b.x, a.y - b.y, a.z - b.z};
    return r;
}

static float len3(Vector3 v) {
    return sqrtf(dot3(v, v));
}

static Vector3 norm3(Vector3 v) {
    float l = len3(v);
    if (l <= 0.00001f) return (Vector3){0, 0, -1};
    return (Vector3){v.x / l, v.y / l, v.z / l};
}

// Convert world direction into listener-local right/forward axes using Quaternion helpers already in RealityLib.
static AudioSpatialGains SpatializeSimple(
    Vector3 listenerPos,
    Quaternion listenerOri,
    Vector3 sourcePos,
    float baseGain,
    float minDist,
    float maxDist,
    float rolloff
) {
    Vector3 toSrc = sub3(sourcePos, listenerPos);
    float d = len3(toSrc);

    float distGain = 1.0f;
    if (d <= minDist) {
        distGain = 1.0f;
    } else if (d >= maxDist) {
        distGain = 0.0f;
    } else {
        float nd = (minDist / d);
        distGain = powf(nd, rolloff);
    }
    distGain = clampf(distGain, 0.0f, 1.0f);

    Vector3 dir = norm3(toSrc);
    Vector3 right = QuaternionRight(listenerOri);

    // Pan is projection onto listener right axis, clamped.
    float pan = clampf(dot3(dir, right), -1.0f, 1.0f);

    // Equal-power pan.
    float angle = (pan + 1.0f) * kPi * 0.25f;
    float l = cosf(angle);
    float r = sinf(angle);

    float g = baseGain * distGain;
    AudioSpatialGains gains = {l * g, r * g, distGain};
    return gains;
}

AudioSpatialGains AudioSpatializerSimple_ComputeGains(
    Vector3 listenerPos,
    Quaternion listenerOri,
    Vector3 sourcePos,
    float baseGain,
    float minDist,
    float maxDist,
    float rolloff
) {
    return SpatializeSimple(listenerPos, listenerOri, sourcePos, baseGain, minDist, maxDist, rolloff);
}

