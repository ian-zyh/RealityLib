#ifndef REALITYLIB_AUDIO_SPATIALIZER_SIMPLE_H
#define REALITYLIB_AUDIO_SPATIALIZER_SIMPLE_H

#include "../realitylib_vr.h"

typedef struct AudioSpatialGains {
    float left;
    float right;
    float distanceGain;
} AudioSpatialGains;

AudioSpatialGains AudioSpatializerSimple_ComputeGains(
    Vector3 listenerPos,
    Quaternion listenerOri,
    Vector3 sourcePos,
    float baseGain,
    float minDist,
    float maxDist,
    float rolloff
);

#endif
