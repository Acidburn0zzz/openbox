#include "font.h"
#include "kernel/openbox.h"
#include "kernel/geom.h"
#include "kernel/gettext.h"
#define _(str) gettext(str)

#include <X11/Xft/Xft.h>
#include <glib.h>
#include <string.h>

#define ELIPSES "..."
#define ELIPSES_LENGTH(font, shadow, offset) \
    (font->elipses_length + (shadow ? offset : 0))

void font_startup(void)
{
#ifdef DEBUG
    int version;
#endif /* DEBUG */
    if (!XftInit(0)) {
        g_warning(_("Couldn't initialize Xft.\n"));
        exit(3);
    }
#ifdef DEBUG
    version = XftGetVersion();
    g_message("Using Xft %d.%d.%d (Built against %d.%d.%d).",
              version / 10000 % 100, version / 100 % 100, version % 100,
              XFT_MAJOR, XFT_MINOR, XFT_REVISION);
#endif
}

static void measure_height(ObFont *f)
{
    XGlyphInfo info;
    char *str;

    /* XXX add some extended UTF8 characters in here? */
    str = "12345678900-qwertyuiopasdfghjklzxcvbnm"
        "!@#$%^&*()_+QWERTYUIOPASDFGHJKLZXCVBNM"
        "`~[]\\;',./{}|:\"<>?";

    XftTextExtentsUtf8(ob_display, f->xftfont,
                       (FcChar8*)str, strlen(str), &info);
    f->height = (signed) info.height;

    /* measure an elipses */
    XftTextExtentsUtf8(ob_display, f->xftfont,
                       (FcChar8*)ELIPSES, strlen(ELIPSES), &info);
    f->elipses_length = (signed) info.xOff;
}

ObFont *font_open(char *fontstring)
{
    ObFont *out;
    XftFont *xf;
    
    if ((xf = XftFontOpenName(ob_display, ob_screen, fontstring))) {
        out = g_new(ObFont, 1);
        out->xftfont = xf;
        measure_height(out);
        return out;
    }
    g_warning(_("Unable to load font: %s\n"), fontstring);
    g_warning(_("Trying fallback font: %s\n"), "sans");

    if ((xf = XftFontOpenName(ob_display, ob_screen, "sans"))) {
        out = g_new(ObFont, 1);
        out->xftfont = xf;
        measure_height(out);
        return out;
    }
    g_warning(_("Unable to load font: %s\n"), "sans");
    g_warning(_("Aborting!.\n"));

    exit(3); /* can't continue without a font */
}

void font_close(ObFont *f)
{
    if (f) {
        XftFontClose(ob_display, f->xftfont);
        g_free(f);
    }
}

int font_measure_string(ObFont *f, char *str, int shadow, int offset)
{
    XGlyphInfo info;

    XftTextExtentsUtf8(ob_display, f->xftfont,
                       (FcChar8*)str, strlen(str), &info);

    return (signed) info.xOff + (shadow ? offset : 0);
}

int font_height(ObFont *f, int shadow, int offset)
{
    return f->height + (shadow ? offset : 0);
}

int font_max_char_width(ObFont *f)
{
    return (signed) f->xftfont->max_advance_width;
}

void font_draw(XftDraw *d, TextureText *t, Rect *position)
{
    int x,y,w,h;
    XftColor c;
    GString *text;
    int m, em;
    size_t l;
    gboolean shortened = FALSE;

    y = position->y;
    w = position->width;
    h = position->height;

    /* accomidate for areas bigger/smaller than Xft thinks the font is tall */
    y -= (2 * (t->font->xftfont->ascent + t->font->xftfont->descent) -
          (t->font->height + h) - 1) / 2;

    text = g_string_new(t->string);
    l = g_utf8_strlen(text->str, -1);
    m = font_measure_string(t->font, text->str, t->shadow, t->offset);
    while (l && m > position->width) {
        shortened = TRUE;
        /* remove a character from the middle */
        text = g_string_erase(text, l-- / 2, 1);
        em = ELIPSES_LENGTH(t->font, t->shadow, t->offset);
        /* if the elipses are too large, don't show them at all */
        if (em > position->width)
            shortened = FALSE;
        m = font_measure_string(t->font, text->str, t->shadow, t->offset) + em;
    }
    if (shortened) {
        text = g_string_insert(text, (l + 1) / 2, ELIPSES);
        l += 3;
    }
    if (!l) return;

    switch (t->justify) {
    case Justify_Left:
        x = position->x;
        break;
    case Justify_Right:
        x = position->x + (w - m);
        break;
    case Justify_Center:
        x = position->x + (w - m) / 2;
        break;
    }

    if (t->shadow) {
        if (t->tint >= 0) {
            c.color.red = 0;
            c.color.green = 0;
            c.color.blue = 0;
            c.color.alpha = 0xffff * t->tint / 100; /* transparent shadow */
            c.pixel = BlackPixel(ob_display, ob_screen);
        } else {
            c.color.red = 0xffff * -t->tint / 100;
            c.color.green = 0xffff * -t->tint / 100;
            c.color.blue = 0xffff * -t->tint / 100;
            c.color.alpha = 0xffff * -t->tint / 100; /* transparent shadow */
            c.pixel = WhitePixel(ob_display, ob_screen);
        }  
        XftDrawStringUtf8(d, &c, t->font->xftfont, x + t->offset,
                          t->font->xftfont->ascent + y + t->offset,
                          (FcChar8*)text->str, strlen(text->str));
    }  
    c.color.red = t->color->r | t->color->r << 8;
    c.color.green = t->color->g | t->color->g << 8;
    c.color.blue = t->color->b | t->color->b << 8;
    c.color.alpha = 0xff | 0xff << 8; /* fully opaque text */
    c.pixel = t->color->pixel;
                     
    XftDrawStringUtf8(d, &c, t->font->xftfont, x,
                      t->font->xftfont->ascent + y,
                      (FcChar8*)text->str, strlen(text->str));
    return;
}
