/**************************************************************************
*
* Tint2 : common windows function
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega distribution
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version 2
* as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**************************************************************************/

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
#include <X11/extensions/Xrender.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <cctype>

#include "common.h"
#include "../server.h"


namespace {

unsigned int hex_char_to_int(char c) {
    c = std::tolower(c);

    if (c >= '0' && c <= '9') {
        return c - '0';
    }

    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }

    return 0;
}

bool hex_to_rgb(char const* hex, unsigned int* r, unsigned int* g,
                unsigned int* b) {
    if (hex == nullptr || hex[0] != '#') {
        return false;
    }

    size_t len = strlen(hex);

    if (len == 3 + 1) {
        (*r) = hex_char_to_int(hex[1]);
        (*g) = hex_char_to_int(hex[2]);
        (*b) = hex_char_to_int(hex[3]);
    } else if (len == 6 + 1) {
        (*r) = hex_char_to_int(hex[1]) * 16 + hex_char_to_int(hex[2]);
        (*g) = hex_char_to_int(hex[3]) * 16 + hex_char_to_int(hex[4]);
        (*b) = hex_char_to_int(hex[5]) * 16 + hex_char_to_int(hex[6]);
    } else if (len == 12 + 1) {
        (*r) = hex_char_to_int(hex[1]) * 16 + hex_char_to_int(hex[2]);
        (*g) = hex_char_to_int(hex[5]) * 16 + hex_char_to_int(hex[6]);
        (*b) = hex_char_to_int(hex[9]) * 16 + hex_char_to_int(hex[10]);
    } else {
        return false;
    }

    return true;
}

} // namespace


int const ALLDESKTOP = 0xFFFFFFFF;

std::string GetEnvironment(std::string const& variable_name) {
    char* value = getenv(variable_name.c_str());
    std::string result;

    if (value != nullptr) {
        result.assign(value);
    }

    return result;
}

std::string& StringTrim(std::string& str) {
    static char const* space_chars = " \f\n\r\t\v";

    auto first = str.find_first_not_of(space_chars);

    if (first == std::string::npos) {
        str.clear();
        return str;
    }

    auto last = str.find_last_not_of(space_chars);

    str.erase(0, first);
    str.erase(last - first + 1, std::string::npos);
    return str;
}

void tint_exec(char const* command) {
    if (command) {
        if (fork() == 0) {
            // change for the fork the signal mask
            //          sigset_t sigset;
            //          sigprocmask(SIG_SETMASK, &sigset, 0);
            //          sigprocmask(SIG_UNBLOCK, &sigset, 0);
            execlp(command, command, NULL);
            _exit(1);
        }
    }
}

bool get_color(char const* hex, double* rgb) {
    unsigned int r, g, b;

    if (!hex_to_rgb(hex, &r, &g, &b)) {
        return false;
    }

    rgb[0] = (r / 255.0);
    rgb[1] = (g / 255.0);
    rgb[2] = (b / 255.0);
    return true;
}


void extract_values(char* value, char** value1, char** value2, char** value3) {
    if (*value1) {
        free(*value1);
    }

    if (*value2) {
        free(*value2);
    }

    if (*value3) {
        free(*value3);
    }

    char* b = 0;

    if ((b = strchr(value, ' '))) {
        b[0] = '\0';
        b++;
    } else {
        *value2 = 0;
        *value3 = 0;
    }

    *value1 = strdup(value);
    g_strstrip(*value1);

    char* c = 0;

    if (b) {
        if ((c = strchr(b, ' '))) {
            c[0] = '\0';
            c++;
        } else {
            c = 0;
            *value3 = 0;
        }

        *value2 = strdup(b);
        g_strstrip(*value2);
    }

    if (c) {
        *value3 = strdup(c);
        g_strstrip(*value3);
    }
}


void adjust_asb(DATA32* data, unsigned int w, unsigned int h, int alpha,
                float satur,
                float bright) {
    for (unsigned int y = 0; y < h; ++y) {
        unsigned int id = y * w;

        for (unsigned int x = 0; x < w; ++x, ++id) {
            unsigned int argb = data[id];
            unsigned int a = (argb >> 24) & 0xff;

            // transparent => nothing to do.
            if (a == 0) {
                continue;
            }

            unsigned int r = (argb >> 16) & 0xff;
            unsigned int g = (argb >> 8) & 0xff;
            unsigned int b = (argb) & 0xff;

            // convert RGB to HSB
            auto cmax = (r > g) ? r : g;

            if (b > cmax) {
                cmax = b;
            }

            auto cmin = (r < g) ? r : g;

            if (b < cmin) {
                cmin = b;
            }

            auto hue = 0.0f;
            auto saturation = 0.0f;
            auto brightness = (cmax / 255.0f);

            if (cmax != 0) {
                saturation = (static_cast<float>(cmax - cmin) / static_cast<float>(cmax));
            }

            if (saturation != 0) {
                auto redc = (static_cast<float>(cmax - r) / static_cast<float>(cmax - cmin));
                auto greenc = (static_cast<float>(cmax - g) / static_cast<float>(cmax - cmin));
                auto bluec = (static_cast<float>(cmax - b) / static_cast<float>(cmax - cmin));

                if (r == cmax) {
                    hue = bluec - greenc;
                } else if (g == cmax) {
                    hue = 2.0f + redc - bluec;
                } else {
                    hue = 4.0f + greenc - redc;
                }

                hue = hue / 6.0f;

                if (hue < 0) {
                    hue = hue + 1.0f;
                }
            }

            // adjust
            saturation += satur;

            if (saturation < 0.0) {
                saturation = 0.0;
            }

            if (saturation > 1.0) {
                saturation = 1.0;
            }

            brightness += bright;

            if (brightness < 0.0) {
                brightness = 0.0;
            }

            if (brightness > 1.0) {
                brightness = 1.0;
            }

            if (alpha != 100) {
                a = (a * alpha) / 100;
            }

            // convert HSB to RGB
            if (saturation == 0) {
                r = g = b = (int)(brightness * 255.0f + 0.5f);
            } else {
                float h2 = (hue - (int)hue) * 6.0f;
                float f = h2 - (int)(h2);
                float p = brightness * (1.0f - saturation);
                float q = brightness * (1.0f - saturation * f);
                float t = brightness * (1.0f - (saturation * (1.0f - f)));

                switch ((int) h2) {
                    case 0:
                        r = (int)(brightness * 255.0f + 0.5f);
                        g = (int)(t * 255.0f + 0.5f);
                        b = (int)(p * 255.0f + 0.5f);
                        break;

                    case 1:
                        r = (int)(q * 255.0f + 0.5f);
                        g = (int)(brightness * 255.0f + 0.5f);
                        b = (int)(p * 255.0f + 0.5f);
                        break;

                    case 2:
                        r = (int)(p * 255.0f + 0.5f);
                        g = (int)(brightness * 255.0f + 0.5f);
                        b = (int)(t * 255.0f + 0.5f);
                        break;

                    case 3:
                        r = (int)(p * 255.0f + 0.5f);
                        g = (int)(q * 255.0f + 0.5f);
                        b = (int)(brightness * 255.0f + 0.5f);
                        break;

                    case 4:
                        r = (int)(t * 255.0f + 0.5f);
                        g = (int)(p * 255.0f + 0.5f);
                        b = (int)(brightness * 255.0f + 0.5f);
                        break;

                    case 5:
                        r = (int)(brightness * 255.0f + 0.5f);
                        g = (int)(p * 255.0f + 0.5f);
                        b = (int)(q * 255.0f + 0.5f);
                        break;
                }
            }

            argb = a;
            argb = (argb << 8) + r;
            argb = (argb << 8) + g;
            argb = (argb << 8) + b;
            data[id] = argb;
        }
    }
}


void createHeuristicMask(DATA32* data, int w, int h) {
    // first we need to find the mask color, therefore we check all 4 edge pixel and take the color which
    // appears most often (we only need to check three edges, the 4th is implicitly clear)
    unsigned int topLeft = data[0], topRight = data[w - 1],
                 bottomLeft = data[w * h - w], bottomRight = data[w * h - 1];
    int max = (topLeft == topRight) + (topLeft == bottomLeft) +
              (topLeft == bottomRight);
    int maskPos = 0;

    if (max < (topRight == topLeft) + (topRight == bottomLeft) +
        (topRight == bottomRight)) {
        max = (topRight == topLeft) + (topRight == bottomLeft) +
              (topRight == bottomRight);
        maskPos = w - 1;
    }

    if (max < (bottomLeft == topRight) + (bottomLeft == topLeft) +
        (bottomLeft == bottomRight)) {
        maskPos = w * h - w;
    }

    // now mask out every pixel which has the same color as the edge pixels
    unsigned char* udata = (unsigned char*)data;
    unsigned char b = udata[4 * maskPos];
    unsigned char g = udata[4 * maskPos + 1];
    unsigned char r = udata[4 * maskPos + 1];
    int i;

    for (i = 0; i < h * w; ++i) {
        if (b - udata[0] == 0 && g - udata[1] == 0 && r - udata[2] == 0) {
            udata[3] = 0;
        }

        udata += 4;
    }
}


void render_image(Drawable d, int x, int y, int w, int h) {
    // in real_transparency mode imlib_render_image_on_drawable does not the right thing, because
    // the operation is IMLIB_OP_COPY, but we would need IMLIB_OP_OVER (which does not exist)
    // Therefore we have to do it with the XRender extension (i.e. copy what imlib is doing internally)
    // But first we need to render the image onto itself with PictOpIn to adjust the colors to the alpha channel
    Pixmap pmap_tmp = XCreatePixmap(server.dsp, server.root_win, w, h, 32);
    imlib_context_set_drawable(pmap_tmp);
    imlib_context_set_blend(0);
    imlib_render_image_on_drawable(0, 0);
    Picture pict_image = XRenderCreatePicture(server.dsp, pmap_tmp,
                         XRenderFindStandardFormat(server.dsp, PictStandardARGB32), 0, 0);
    Picture pict_drawable = XRenderCreatePicture(server.dsp, d,
                            XRenderFindVisualFormat(server.dsp, server.visual), 0, 0);
    XRenderComposite(server.dsp, PictOpIn, pict_image, None, pict_image, 0, 0, 0, 0,
                     0, 0, w, h);
    XRenderComposite(server.dsp, PictOpOver, pict_image, None, pict_drawable, 0, 0,
                     0, 0, x, y, w, h);
    imlib_context_set_blend(1);
    XFreePixmap(server.dsp, pmap_tmp);
    XRenderFreePicture(server.dsp, pict_image);
    XRenderFreePicture(server.dsp, pict_drawable);
}
