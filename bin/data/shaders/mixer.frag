uniform sampler2D src;
uniform float uMix;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    vec4 col = texture(src, vTexCoord);

    vec2 inv = vec2(1.0) - vTexCoord;
    vec4 mirrored = texture(src, inv);

    fragColor = mix(col, mirrored, uMix);
}
