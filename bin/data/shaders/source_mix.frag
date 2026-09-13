uniform sampler2D prev;
uniform sampler2D src;

uniform float uMix;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    vec2 uv = vTexCoord;

    vec4 processed = texture(prev, uv);
    vec4 original = texture(src, uv);

    fragColor = vec4(mix(processed.rgb, original.rgb, uMix), 1.0);
}