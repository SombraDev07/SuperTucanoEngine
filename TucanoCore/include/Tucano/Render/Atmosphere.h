#pragma once
#include <glm/glm.hpp>

namespace Tucano {

// Aligned structure matching std140 GLSL layout.
// In std140, every vec4 is aligned to 16 bytes. We force the same alignment in C++
// so the binary layout of the UBO is identical between CPU and GPU.
struct AtmosphereParameters {
    // Planet Dimensions
    float PlanetRadius;
    float AtmosphereHeight;
    alignas(16) glm::vec4 _Pad0;

    // Rayleigh scattering
    alignas(16) glm::vec4 RayleighScattering;    // rgb = scattering coeff (km^-1), w = scale height (km)
    
    // Mie scattering & extinction
    alignas(16) glm::vec4 MieScattering;         // rgb = scattering coeff (km^-1), w = scale height (km)
    alignas(16) glm::vec4 MieExtinction;         // rgb = extinction coeff (km^-1), w = phase anisotropy (g)

    // Ozone absorption / other absorption
    alignas(16) glm::vec4 AbsorptionExtinction;  // rgb = extinction coeff (km^-1), w = ozone layer center height (km)
    alignas(16) glm::vec4 AbsorptionDensity;     // x = ozone layer width (km), yzw = padding

    // Ground
    alignas(16) glm::vec4 GroundAlbedo;          // rgb = albedo

    // Lighting (Sun)
    alignas(16) glm::vec4 SunDirectionAndIntensity; // xyz = normalized direction to sun, w = intensity
    alignas(16) glm::vec4 SunColor;                 // rgb = color
};

struct AtmosphereSettings {
    float SunIntensity = 10.0f;
    glm::vec3 SunColor{1.0f, 1.0f, 1.0f};
    float SunElevation = 15.0f; // degrees (-90 to 90)
    float SunAzimuth = 0.0f;    // degrees (0 to 360)

    float PlanetRadius = 6360.0f;       // km
    float AtmosphereHeight = 80.0f;     // km

    float RayleighDensity = 1.0f;
    float RayleighScaleHeight = 8.0f;   // km

    float MieDensity = 1.0f;
    float MieScaleHeight = 1.2f;        // km
    float MieAnisotropy = 0.8f;         // g parameter for Henyey-Greenstein

    float OzoneDensity = 1.0f;
    glm::vec3 GroundAlbedo{0.1f, 0.1f, 0.1f};

    float SkyIntensity = 1.0f;
    float Exposure = 1.0f;
    float Gamma = 2.2f;

    bool EnableMultiScattering = true;
    bool EnableAerialPerspective = true;
};

} // namespace Tucano
