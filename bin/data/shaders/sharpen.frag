uniform sampler2D src;
uniform float uAmount;
uniform vec2 uSourceSize;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    vec2 px = vec2(1.0) / uSourceSize;

    vec4 center = texture(src, vTexCoord);
    vec4 left   = texture(src, vTexCoord - vec2(px.x, 0.0));
    vec4 right  = texture(src, vTexCoord + vec2(px.x, 0.0));
    vec4 up     = texture(src, vTexCoord - vec2(0.0, px.y));
    vec4 down   = texture(src, vTexCoord + vec2(0.0, px.y));

    vec4 laplacian = 4.0 * center - left - right - up - down;
    fragColor = center + laplacian * uAmount;
}
