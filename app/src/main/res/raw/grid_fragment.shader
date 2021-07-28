#version 300 es
precision mediump float;
in vec4 v_Color;
in vec3 v_Grid;
out vec4 o_fragColor;

void main() {
    float depth = gl_FragCoord.z / gl_FragCoord.w; // Calculate world-space distance.
    vec4 v_Color_m = (1000.0 - depth) / 1000.0 * v_Color;
    if ((mod(abs(v_Grid.x), 10.0) < 0.1) || (mod(abs(v_Grid.z), 10.0) < 0.1)) {
        o_fragColor = max(0.0, (90.0-depth) / 90.0) * vec4(1.0, 1.0, 1.0, 1.0)
                + min(1.0, depth / 90.0) * v_Color_m;
    } else {
        o_fragColor = v_Color_m;
    }
}
