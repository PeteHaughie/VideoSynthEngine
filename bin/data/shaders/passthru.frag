uniform sampler2D src;

in vec2 vTexCoord;
out vec4 fragColor;

void main()
{
    fragColor = texture(src, vTexCoord);
}
