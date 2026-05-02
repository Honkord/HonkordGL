# Wayland protocol codegen — XML under ${HONKORDGL_ROOT}/wayland/xml (see wayland/protocols.json).
# Rows use '|' so CMake list parsing does not split on ';' inside paths.

set(HONKORD_WAYLAND_GEN_DIR "${HONKORDGL_ROOT}/src/Wayland/generated")
file(MAKE_DIRECTORY "${HONKORD_WAYLAND_GEN_DIR}")

set(HONKORD_WAYLAND_XML_ROOT "${HONKORDGL_ROOT}/wayland/xml")

set(HONKORD_WAYLAND_PROTOCOLS
    "tablet-v2|${HONKORD_WAYLAND_XML_ROOT}/stable/tablet/tablet-v2.xml|tablet-v2-client-protocol.h|tablet-v2-protocol.c"
    "cursor-shape-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/cursor-shape/cursor-shape-v1.xml|cursor-shape-v1-client-protocol.h|cursor-shape-v1-protocol.c"
    "fractional-scale-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/fractional-scale/fractional-scale-v1.xml|fractional-scale-v1-client-protocol.h|fractional-scale-v1-protocol.c"
    "linux-drm-syncobj-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/linux-drm-syncobj/linux-drm-syncobj-v1.xml|linux-drm-syncobj-v1-client-protocol.h|linux-drm-syncobj-v1-protocol.c"
    "single-pixel-buffer-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/single-pixel-buffer/single-pixel-buffer-v1.xml|single-pixel-buffer-v1-client-protocol.h|single-pixel-buffer-v1-protocol.c"
    "text-input-v3|${HONKORD_WAYLAND_XML_ROOT}/unstable/text-input/text-input-unstable-v3.xml|text-input-v3-client-protocol.h|text-input-v3-protocol.c"
    "viewporter|${HONKORD_WAYLAND_XML_ROOT}/stable/viewporter/viewporter.xml|viewporter-client-protocol.h|viewporter-protocol.c"
    "xdg-shell|${HONKORD_WAYLAND_XML_ROOT}/stable/xdg-shell/xdg-shell.xml|xdg-shell-client-protocol.h|xdg-shell-protocol.c"
    "xdg-activation-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/xdg-activation/xdg-activation-v1.xml|xdg-activation-v1-client-protocol.h|xdg-activation-v1-protocol.c"
    "xdg-decoration-unstable-v1|${HONKORD_WAYLAND_XML_ROOT}/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml|xdg-decoration-unstable-v1-client-protocol.h|xdg-decoration-unstable-v1-protocol.c"
    "xdg-dialog-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/xdg-dialog/xdg-dialog-v1.xml|xdg-dialog-v1-client-protocol.h|xdg-dialog-v1-protocol.c"
    "xdg-foreign-unstable-v2|${HONKORD_WAYLAND_XML_ROOT}/unstable/xdg-foreign/xdg-foreign-unstable-v2.xml|xdg-foreign-unstable-v2-client-protocol.h|xdg-foreign-unstable-v2-protocol.c"
    "xdg-toplevel-icon-v1|${HONKORD_WAYLAND_XML_ROOT}/staging/xdg-toplevel-icon/xdg-toplevel-icon-v1.xml|xdg-toplevel-icon-v1-client-protocol.h|xdg-toplevel-icon-v1-protocol.c"
)

set(HONKORD_WAYLAND_GENERATED_C "")
foreach(entry IN LISTS HONKORD_WAYLAND_PROTOCOLS)
    string(REPLACE "|" ";" row "${entry}")
    list(GET row 0 pid)
    list(GET row 1 xml)
    list(GET row 2 hdr)
    list(GET row 3 src)

    set(out_h "${HONKORD_WAYLAND_GEN_DIR}/${hdr}")
    set(out_c "${HONKORD_WAYLAND_GEN_DIR}/${src}")

    add_custom_command(
        OUTPUT "${out_h}" "${out_c}"
        COMMAND wayland-scanner client-header "${xml}" "${out_h}"
        COMMAND wayland-scanner private-code "${xml}" "${out_c}"
        DEPENDS "${xml}"
        COMMENT "Generating Wayland protocol: ${pid}"
        VERBATIM
    )
    list(APPEND HONKORD_WAYLAND_GENERATED_C "${out_c}")
endforeach()
