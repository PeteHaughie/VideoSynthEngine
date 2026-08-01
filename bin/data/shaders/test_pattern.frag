uniform float uFrameCount;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    vec2 uv = vTexCoord;
    float t = uFrameCount * 0.02;
    float bars = floor(uv.x * 8.0);
    float phase = bars * 0.3 + t;
    vec3 color = 0.5 + 0.5 * cos(phase + vec3(0.0, 2.0, 4.0));

    float bar = smoothstep(0.02, 0.0, abs(uv.x - fract(t * 0.5)));
    color = mix(color, vec3(1.0), bar);

    vec2 grid = fract(uv * 20.0);
    float line = 1.0 - step(0.95, max(grid.x, grid.y));
    color *= 0.85 + 0.15 * line;

    fragColor = vec4(color, 1.0);
}
