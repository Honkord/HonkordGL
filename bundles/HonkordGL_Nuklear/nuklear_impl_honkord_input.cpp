/**
 * HonkordGL → Nuklear input bridge.
 */

#include "nuklear_impl_honkord_input.h"

#include <HonkordGL/WindowApplication.h>

#include <cfloat>

using HonkordGL::Events::EventType;
using HonkordGL::Graphics::ApplicationContextSettings;

namespace {

static void KeyToNk(HonkordGL::Events::KeyCode key, enum nk_keys * out_nk, bool * out_char_only) noexcept
{
    using KC = HonkordGL::Events::KeyCode;
    *out_char_only = false;
    *out_nk = NK_KEY_NONE;
    switch (key) {
    case KC::SHIFT:
        *out_nk = NK_KEY_SHIFT;
        return;
    case KC::CTRL:
        *out_nk = NK_KEY_CTRL;
        return;
    case KC::DELETE:
        *out_nk = NK_KEY_DEL;
        return;
    case KC::ENTER:
        *out_nk = NK_KEY_ENTER;
        return;
    case KC::TAB:
        *out_nk = NK_KEY_TAB;
        return;
    case KC::UP:
        *out_nk = NK_KEY_UP;
        return;
    case KC::DOWN:
        *out_nk = NK_KEY_DOWN;
        return;
    case KC::LEFT:
        *out_nk = NK_KEY_LEFT;
        return;
    case KC::RIGHT:
        *out_nk = NK_KEY_RIGHT;
        return;
    default:
        break;
    }
    *out_char_only = true;
}

} // namespace

void Nuklear_ImplHonkord_InputBegin(struct nk_context * ctx) noexcept
{
    if (ctx)
        nk_input_begin(ctx);
}

void Nuklear_ImplHonkord_InputEnd(struct nk_context * ctx) noexcept
{
    if (ctx)
        nk_input_end(ctx);
}

void Nuklear_ImplHonkord_ProcessEvent(
    ApplicationContextSettings * app,
    struct nk_context * ctx,
    HonkordGL::Events::Event const & ev) noexcept
{
    (void)app;
    if (!ctx)
        return;

    using ET = EventType;

    switch (ev.type) {
    case ET::QUIT:
        break;
    case ET::RESIZE:
        break;
    case ET::FOCUS:
        break;
    case ET::MOUSE_ENTER:
        nk_input_motion(ctx, ev.x, ev.y);
        break;
    case ET::MOUSE_LEAVE:
        nk_input_motion(ctx, -1, -1);
        break;
    case ET::MOUSE_MOVE:
        nk_input_motion(ctx, ev.x, ev.y);
        break;
    case ET::MOUSE_BUTTON_PRESS: {
        if (ev.mouse_button == 4u) {
            nk_input_scroll(ctx, nk_vec2(0.f, 1.f));
            break;
        }
        if (ev.mouse_button == 5u) {
            nk_input_scroll(ctx, nk_vec2(0.f, -1.f));
            break;
        }
        int btn = NK_BUTTON_LEFT;
        if (ev.mouse_button == 2u)
            btn = NK_BUTTON_MIDDLE;
        else if (ev.mouse_button == 3u)
            btn = NK_BUTTON_RIGHT;
        nk_input_button(ctx, static_cast<enum nk_buttons>(btn), ev.x, ev.y, 1);
        break;
    }
    case ET::MOUSE_BUTTON_RELEASE: {
        if (ev.mouse_button == 4u || ev.mouse_button == 5u)
            break;
        int btn = NK_BUTTON_LEFT;
        if (ev.mouse_button == 2u)
            btn = NK_BUTTON_MIDDLE;
        else if (ev.mouse_button == 3u)
            btn = NK_BUTTON_RIGHT;
        nk_input_button(ctx, static_cast<enum nk_buttons>(btn), ev.x, ev.y, 0);
        break;
    }
    case ET::KEY_PRESS: {
        enum nk_keys nk = NK_KEY_NONE;
        bool char_only = false;
        KeyToNk(ev.key, &nk, &char_only);
        if (nk != NK_KEY_NONE)
            nk_input_key(ctx, nk, 1);
        if (ev.character != 0u)
            nk_input_unicode(ctx, static_cast<nk_rune>(ev.character));
        (void)char_only;
        break;
    }
    case ET::KEY_RELEASE: {
        enum nk_keys nk = NK_KEY_NONE;
        bool char_only = false;
        KeyToNk(ev.key, &nk, &char_only);
        if (nk != NK_KEY_NONE)
            nk_input_key(ctx, nk, 0);
        break;
    }
    default:
        break;
    }
}
