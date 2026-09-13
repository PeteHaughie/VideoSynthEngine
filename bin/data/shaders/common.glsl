#define VSE_PI 3.14159265359
#define VSE_TWO_PI 6.28318530718
#define VSE_HALF_PI 1.57079632679

const float VSE_EPSILON = 0.0001;

float vse_remap(float value, float inMin, float inMax, float outMin, float outMax)
{
    return outMin + (outMax - outMin) * (value - inMin) / max(inMax - inMin, VSE_EPSILON);
}

vec2 vse_rotate(vec2 uv, float radians)
{
    float c = cos(radians);
    float s = sin(radians);
    return vec2(uv.x * c - uv.y * s, uv.x * s + uv.y * c);
}