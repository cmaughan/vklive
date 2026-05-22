from linalg import vec2, vec4

def draw():
    circle(vec2(0.0, 0.0), vec4(0.95, 0.35, 0.10, 1.0), 64.0, 1.0)
    line(vec2(-0.75, -0.75), vec2(0.75, 0.75), vec4(0.15, 0.65, 1.0, 1.0), 6.0)
    rect(vec2(48.0, 48.0), vec2(196.0, 144.0), vec4(0.2, 1.0, 0.55, 1.0), 5.0, 0.0)
    text(vec2(64.0, 220.0), "Metal NanoVG", vec4(1.0, 1.0, 1.0, 1.0), 32)
