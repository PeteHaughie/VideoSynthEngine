uniform sampler2D src;
uniform sampler2D fb;

uniform float uMix;
uniform float uDecay;
uniform float uSaturation;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    vec2 uv = vTexCoord;

    vec4 incoming = texture(src, uv);
    vec4 feedback = texture(fb, uv);

    // Damp the feedback and add the fresh source on top.
    vec3 color = incoming.rgb * uMix + feedback.rgb * uDecay;

    // Gentle saturation fold to keep the trail alive.
    float luma = dot(color, vec3(0.299, 0.587, 0.114));
    color = mix(vec3(luma), color, 1.0 + uSaturation);

    fragColor = vec4(color, 1.0);
}