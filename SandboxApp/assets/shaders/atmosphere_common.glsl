#ifndef ATMOSPHERE_COMMON_GLSL
#define ATMOSPHERE_COMMON_GLSL

const float PI = 3.141592653589793;

struct AtmosphereParameters {
    float PlanetRadius;
    float AtmosphereHeight;
    vec4 _Pad0;

    vec4 RayleighScattering;    // rgb = scattering coeff (km^-1), w = scale height (km)
    vec4 MieScattering;         // rgb = scattering coeff (km^-1), w = scale height (km)
    vec4 MieExtinction;         // rgb = extinction coeff (km^-1), w = phase anisotropy (g)
    vec4 AbsorptionExtinction;  // rgb = extinction coeff (km^-1), w = ozone layer center height (km)
    vec4 AbsorptionDensity;     // x = ozone layer width (km), yzw = padding

    vec4 GroundAlbedo;          // rgb = albedo
    vec4 SunDirectionAndIntensity; // xyz = normalized direction to sun, w = intensity
    vec4 SunColor;                 // rgb = color
};

// Ray-Sphere intersection
// Returns true if ray intersects sphere, and writes distances to t0 and t1.
bool raySphereIntersection(vec3 o, vec3 d, float r, out float t0, out float t1) {
    float b = dot(o, d);
    float c = dot(o, o) - r * r;
    float h = b * b - c;
    if (h < 0.0) {
        t0 = -1.0;
        t1 = -1.0;
        return false;
    }
    h = sqrt(h);
    t0 = -b - h;
    t1 = -b + h;
    return true;
}

// Density profiles
void getAtmosphericDensities(AtmosphereParameters params, float h, out float rayleighDensity, out float mieDensity, out float absorptionDensity) {
    rayleighDensity = exp(-h / max(0.0001, params.RayleighScattering.w));
    mieDensity = exp(-h / max(0.0001, params.MieScattering.w));
    
    // Tent-like profile for ozone
    float ozoneCenter = params.AbsorptionExtinction.w;
    float ozoneWidth = params.AbsorptionDensity.x;
    absorptionDensity = max(0.0, 1.0 - abs(h - ozoneCenter) / max(0.0001, ozoneWidth));
}

// Phase functions
float RayleighPhase(float cosTheta) {
    return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta);
}

float MiePhase(float cosTheta, float g) {
    float g2 = g * g;
    float denom = 1.0 + g2 - 2.0 * g * cosTheta;
    return (1.0 - g2) / (4.0 * PI * denom * sqrt(max(0.000001, denom)));
}

// Transmittance LUT parametrization
void UVToTransmittanceParams(AtmosphereParameters params, vec2 uv, out float h, out float mu) {
    float y = uv.y;
    float x = uv.x;
    h = y * params.AtmosphereHeight;
    mu = x * 2.0 - 1.0;
}

vec2 TransmittanceParamsToUV(AtmosphereParameters params, float h, float mu) {
    float y = h / params.AtmosphereHeight;
    float x = (mu + 1.0) / 2.0;
    return vec2(x, y);
}

// Read Transmittance LUT
vec3 getTransmittance(AtmosphereParameters params, sampler2D transmittanceLUT, float r, float mu) {
    float h = r - params.PlanetRadius;
    vec2 uv = TransmittanceParamsToUV(params, h, mu);
    return texture(transmittanceLUT, uv).rgb;
}

// Integrates optical depth along a ray segment
vec3 integrateTransmittance(AtmosphereParameters params, vec3 o, vec3 d, float limit) {
    float t0, t1;
    float rAtmos = params.PlanetRadius + params.AtmosphereHeight;
    if (!raySphereIntersection(o, d, rAtmos, t0, t1)) {
        return vec3(1.0);
    }
    float start = max(0.0, t0);
    float end = min(limit, t1);
    
    float planetT0, planetT1;
    if (raySphereIntersection(o, d, params.PlanetRadius, planetT0, planetT1)) {
        if (planetT0 > 0.0) {
            end = min(end, planetT0);
        }
    }

    if (start >= end) return vec3(1.0);

    const int steps = 40;
    float stepSize = (end - start) / float(steps);
    vec3 opticalDepth = vec3(0.0);

    for (int i = 0; i < steps; ++i) {
        float t = start + (float(i) + 0.5) * stepSize;
        vec3 p = o + t * d;
        float h = length(p) - params.PlanetRadius;
        
        float rayleigh, mie, absorption;
        getAtmosphericDensities(params, h, rayleigh, mie, absorption);

        vec3 extRayleigh = params.RayleighScattering.rgb * rayleigh;
        vec3 extMie = params.MieExtinction.rgb * mie;
        vec3 extAbsorption = params.AbsorptionExtinction.rgb * absorption;

        opticalDepth += (extRayleigh + extMie + extAbsorption) * stepSize;
    }

    return exp(-opticalDepth);
}

// Sky-View LUT mapping
void UVToSkyViewParams(AtmosphereParameters params, vec2 uv, float r, out float theta, out float phi) {
    phi = uv.x * 2.0 * PI;
    
    float rCur = max(r, params.PlanetRadius + 0.0001);
    float cosHorizon = -sqrt(max(0.0, 1.0 - (params.PlanetRadius * params.PlanetRadius) / (rCur * rCur)));
    
    if (uv.y > 0.5) {
        float y = (uv.y - 0.5) * 2.0;
        y = y * y;
        float cosTheta = mix(cosHorizon, 1.0, y);
        theta = acos(cosTheta);
    } else {
        float y = (0.5 - uv.y) * 2.0;
        y = y * y;
        float cosTheta = mix(cosHorizon, -1.0, y);
        theta = acos(cosTheta);
    }
}

vec2 SkyViewParamsToUV(AtmosphereParameters params, float theta, float phi, float r) {
    float u = phi / (2.0 * PI);
    
    float cosTheta = cos(theta);
    float rCur = max(r, params.PlanetRadius + 0.0001);
    float cosHorizon = -sqrt(max(0.0, 1.0 - (params.PlanetRadius * params.PlanetRadius) / (rCur * rCur)));
    
    float v;
    if (cosTheta >= cosHorizon) {
        float y = (cosTheta - cosHorizon) / (1.0 - cosHorizon);
        y = sqrt(y);
        v = 0.5 + 0.5 * y;
    } else {
        float y = (cosHorizon - cosTheta) / (cosHorizon + 1.0);
        y = sqrt(y);
        v = 0.5 - 0.5 * y;
    }
    return vec2(u, v);
}

#endif // ATMOSPHERE_COMMON_GLSL
