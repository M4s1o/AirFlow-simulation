#version 460 core

in vec4 cellColor;
in vec2 velocity;

out vec4 color;

void main()
{
    vec2 p = gl_PointCoord - vec2(0.5);

    // center dot (debug + reference point)
    if (length(p) < 0.05)
    {
        color = vec4(1.0);
        return;
    }

    float speed = length(velocity);
    if (speed < 0.0001)
    {
        color = cellColor;
        return;
    }

    vec2 dir = normalize(velocity);
    vec2 perp = vec2(-dir.y, dir.x);

    float along = dot(p, dir);
    float side  = abs(dot(p, perp));

    float arrowLength = 0.45; // FIXED for debugging

    bool shaft =
        along > 0.0 &&
        along < arrowLength &&
        side < 0.06; // wider so you SEE it

    if (shaft)
        color = vec4(0.7, 0.7, 0.7, 1.0);
    else
        color = cellColor;
}