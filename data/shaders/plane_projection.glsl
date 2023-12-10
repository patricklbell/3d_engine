#begin VERTEX
layout (location = 0) in vec3 vertex;

uniform mat4 model;

void main()
{
    // transforms vertex into grid space
    gl_Position = model * vec4(vertex, 1.0);
}

#end
#begin GEOMETRY

layout (triangles) in;
layout (triangle_strip, max_vertices = 4) out;

out vec2 texcoord;

vec2 getIntersectionWithPlane(vec4 p1, vec4 p2) {
    float t = abs(p1.y / (p2.y - p1.y));
    vec2 i =  p1.xz + (p2.xz - p1.xz)*t;
    i = -i;
    i.x = (1.0 - 2.0*i.x)/2.0f - 0.5f;
    return i;
}

#define THICKNESS 0.002

void makeLine(vec2 p1, vec2 p2) {
    vec2 d = normalize(p2 - p1);
    vec2 n = vec2(-d.y, d.x)*THICKNESS;
    gl_Position = vec4(p1, 0.0, 1.0);
    texcoord = (p1 + vec2(1.0)) / 2.0;
    EmitVertex();
    gl_Position = vec4(p1 + n, 0.0, 1.0);
    texcoord = ((p1 + n) + vec2(1.0)) / 2.0;
    EmitVertex();
    gl_Position = vec4(p2 + n, 0.0, 1.0);
    texcoord = ((p2 + n) + vec2(1.0)) / 2.0;
    EmitVertex();
    gl_Position = vec4(p2, 0.0, 1.0);
    texcoord = (p2 + vec2(1.0)) / 2.0;
    EmitVertex();
    EndPrimitive();
}

void main() {
    bool is_below0 = gl_in[0].gl_Position.y <= 0;
    bool is_below1 = gl_in[1].gl_Position.y <= 0;
    bool is_below2 = gl_in[2].gl_Position.y <= 0;
    // Triangle passes through plane
    if(is_below0 != is_below1 || is_below0 != is_below2) {
        int i = 0;
        vec2 points[2];
        if(is_below0 != is_below1) {
            points[i] = getIntersectionWithPlane(gl_in[0].gl_Position, gl_in[1].gl_Position);
            i++;
        }
        if (is_below1 != is_below2) {
            points[i] = getIntersectionWithPlane(gl_in[1].gl_Position, gl_in[2].gl_Position);
            i++;
        }
        if (is_below2 != is_below0) {
            points[i] = getIntersectionWithPlane(gl_in[2].gl_Position, gl_in[0].gl_Position);
            i++;
        }

        makeLine(points[0], points[1]);
    }
}  

#end
#begin FRAGMENT

in vec2 texcoord;
out vec4 out_color;

void main()
{
    out_color = vec4(texcoord, 1.0, 1.0);
}

#end