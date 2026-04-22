Import("env")

# M5GFX ships LVGL 8.x compatible font .c files in src/lgfx/Fonts/lvgl/.
# When building with real LVGL 9.x these files cause struct layout conflicts.
# Exclude them from compilation without touching libdeps.
def skip_m5gfx_lvgl_fonts(node):
    path = node.path.replace("\\", "/")
    if "M5GFX" in path and "/lgfx/Fonts/lvgl/" in path:
        return None
    return node

env.AddBuildMiddleware(skip_m5gfx_lvgl_fonts, "*.c")
