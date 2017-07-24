/**************************************************************************
*
* Tint3 : common windows function
*
* Copyright (C) 2007 Pål Staurland (staura@gmail.com)
* Modified (C) 2008 thierry lorthiois (lorthiois@bbsoft.fr) from Omega
*distribution
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
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
*USA.
**************************************************************************/

#include <X11/Xatom.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/Xrender.h>
#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <regex>
#include <tuple>

#include "cxx_features.hh"
#include "server.hh"
#include "util/common.hh"
#include "util/log.hh"

namespace util {

void GObjectUnrefDeleter::operator()(gpointer data) const {
  g_object_unref(data);
}

namespace string {

std::string& Trim(std::string& str) {
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

std::vector<std::string> Split(std::string const& str, char sep) {
  auto beg = str.cbegin();
  auto end = std::find(beg, str.cend(), sep);

  // If we never found the separator, return a vector with the string itself
  // inside it. This is the same behavior as Python's str.split().
  if (end == str.cend()) {
    return {str};
  }

  std::vector<std::string> parts;
  bool found = true;
  do {
    found = (end != str.cend());
    parts.push_back(std::string{beg, end});
    beg = (end != str.cend()) ? (end + 1) : end;
    end = std::find(beg, str.cend(), sep);
  } while (found);
  return parts;
}

bool StartsWith(std::string const& str, std::string const& other) {
  return (str.compare(0, other.length(), other) == 0);
}

bool RegexMatch(std::string const& pattern, std::string const& string) {
  std::smatch matches;
  return std::regex_match(string, matches, std::regex(pattern));
}

void ToLowerCase(std::string* str) {
  std::transform(str->begin(), str->end(), str->begin(),
                 [](char c) -> char { return std::tolower(c); });
}

}  // namespace string
}  // namespace util

unsigned int const kAllDesktops = static_cast<unsigned int>(-1);

bool SignalAction(int signal_number, void signal_handler(int), int flags) {
  struct sigaction sa;
  std::memset(&sa, 0, sizeof(sa));
  sa.sa_handler = signal_handler;
  sa.sa_flags = flags;

  if (sigaction(signal_number, &sa, nullptr) != 0) {
    util::log::Error() << "sigaction: " << std::strerror(errno) << '\n';
    return false;
  }

  return true;
}

void TintShellExec(std::string const& command) {
  if (!command.empty()) {
    if (fork() == 0) {
      // change for the fork the signal mask
      //          sigset_t sigset;
      //          sigprocmask(SIG_SETMASK, &sigset, 0);
      //          sigprocmask(SIG_UNBLOCK, &sigset, 0);

      // "/bin/sh" should be guaranteed to be a POSIX-compliant shell
      // accepting the "-c" flag:
      //   http://pubs.opengroup.org/onlinepubs/9699919799/utilities/sh.html
      execlp("/bin/sh", "sh", "-c", command.c_str(), nullptr);

      // In case execlp() fails and the process image is not replaced
      util::log::Error() << "Failed launching \"" << command << "\".\n";
      _exit(1);
    }
  }
}

namespace {

template <typename T>
constexpr T clamp(T value, T min, T max) {
  return (value < min) ? min : ((max < value) ? max : value);
}

// Returns an ARGB value given the individual A, R, G, B components.
constexpr DATA32 pack_argb(unsigned char a, unsigned char r, unsigned char g,
                           unsigned char b) {
  return (a << 24 | r << 16 | g << 8 | b);
}

// Returns the individual A, R, G, B components from an ARGB value.
std::tuple<char, char, char, char> unpack_argb(DATA32 argb) {
  return std::make_tuple((argb >> 24) & 0xFF, (argb >> 16) & 0xFF,
                         (argb >> 8) & 0xFF, argb & 0xFF);
}

std::tuple<double, double, double> RgbToHsv(double R, double G, double B) {
  double M = std::max({R, G, B});
  double m = std::min({R, G, B});
  double C = (M - m);

  double H_ = 0.0;
  if (C != 0.0) {
    if (R == M) {
      H_ = (G - B) / C;
      if (H_ < 0.0) {
        H_ += 6.0;
      }
    } else if (G == M) {
      H_ = ((B - R) / C) + 2.0;
    } else {  // B == M
      H_ = ((R - G) / C) + 4.0;
    }
  }

  double S_ = 0.0;
  if (M != 0.0) {
    S_ = (C / M);
  }

  // Value returned as the maximum of the R, G, B components in accordance with:
  //  https://en.wikipedia.org/wiki/HSL_and_HSV.
  return std::make_tuple(H_ / 6.0, S_, M);
}

std::tuple<double, double, double> HsvToRgb(double H, double S, double V) {
  double C = (V * S);
  double H_ = (H * 6.0);
  double X = C * (1.0 - std::fabs(std::fmod(H_, 2.0) - 1.0));
  double R_, G_, B_;

  if (H_ <= 1.0) {
    R_ = C;
    G_ = X;
    B_ = 0.0;
  } else if (H_ <= 2.0) {
    R_ = X;
    G_ = C;
    B_ = 0.0;
  } else if (H_ <= 3.0) {
    R_ = 0.0;
    G_ = C;
    B_ = X;
  } else if (H_ <= 4.0) {
    R_ = 0.0;
    G_ = X;
    B_ = C;
  } else if (H_ <= 5.0) {
    R_ = X;
    G_ = 0.0;
    B_ = C;
  } else if (H_ <= 6.0) {
    R_ = C;
    G_ = 0.0;
    B_ = X;
  } else {
    R_ = 0.0;
    G_ = 0.0;
    B_ = 0.0;
  }

  double m = (V - C);
  return std::make_tuple(R_ + m, G_ + m, B_ + m);
}

}  // namespace

void AdjustASB(DATA32* data, unsigned int w, unsigned int h, int alpha,
               float saturation_adjustment, float brightness_adjustment) {
  for (unsigned int i = 0; i < w * h; ++i, ++data) {
    unsigned char ca, cr, cg, cb;
    std::tie(ca, cr, cg, cb) = unpack_argb(*data);

    // transparent => nothing to do.
    if (ca == 0) {
      continue;
    }

    double h, s, v;
    std::tie(h, s, v) = RgbToHsv(cr / 255.0, cg / 255.0, cb / 255.0);

    // adjust
    ca = (ca * alpha) / 100.0;
    s = clamp(s + saturation_adjustment, 0.0, 1.0);
    v = clamp(v + brightness_adjustment, 0.0, 1.0);

    // update the pixel data
    double r, g, b;
    std::tie(r, g, b) = HsvToRgb(h, s, v);
    cr = std::nearbyint(r * 255.0);
    cg = std::nearbyint(g * 255.0);
    cb = std::nearbyint(b * 255.0);
    (*data) = pack_argb(ca, cr, cg, cb);
  }
}

void CreateHeuristicMask(DATA32* data, int w, int h) {
  // first we need to find the mask color, therefore we check all 4 edge pixel
  // and take the color which
  // appears most often (we only need to check three edges, the 4th is
  // implicitly clear)
  unsigned int top_left = data[0], top_right = data[w - 1],
               bottomLeft = data[w * h - w], bottomRight = data[w * h - 1];
  int max = (top_left == top_right) + (top_left == bottomLeft) +
            (top_left == bottomRight);
  int maskPos = 0;

  if (max < (top_right == top_left) + (top_right == bottomLeft) +
                (top_right == bottomRight)) {
    max = (top_right == top_left) + (top_right == bottomLeft) +
          (top_right == bottomRight);
    maskPos = w - 1;
  }

  if (max < (bottomLeft == top_right) + (bottomLeft == top_left) +
                (bottomLeft == bottomRight)) {
    maskPos = w * h - w;
  }

  // now mask out every pixel which has the same color as the edge pixels
  unsigned char* udata = (unsigned char*)data;
  unsigned char b = udata[4 * maskPos];
  unsigned char g = udata[4 * maskPos + 1];
  unsigned char r = udata[4 * maskPos + 1];

  for (int i = 0; i < h * w; ++i) {
    if (b - udata[0] == 0 && g - udata[1] == 0 && r - udata[2] == 0) {
      udata[3] = 0;
    }

    udata += 4;
  }
}

void RenderImage(Server* server, Drawable d, int x, int y, int w, int h) {
  // in real_transparency mode imlib_render_image_on_drawable does not the right
  // thing, because
  // the operation is IMLIB_OP_COPY, but we would need IMLIB_OP_OVER (which does
  // not exist)
  // Therefore we have to do it with the XRender extension (i.e. copy what imlib
  // is doing internally)
  // But first we need to render the image onto itself with PictOpIn to adjust
  // the colors to the alpha channel
  Pixmap pmap_tmp = XCreatePixmap(server->dsp, server->root_window(), w, h, 32);
  imlib_context_set_drawable(pmap_tmp);
  imlib_context_set_blend(0);
  imlib_render_image_on_drawable(0, 0);
  Picture pict_image = XRenderCreatePicture(
      server->dsp, pmap_tmp,
      XRenderFindStandardFormat(server->dsp, PictStandardARGB32), 0, 0);
  Picture pict_drawable = XRenderCreatePicture(
      server->dsp, d, XRenderFindVisualFormat(server->dsp, server->visual), 0,
      0);
  XRenderComposite(server->dsp, PictOpIn, pict_image, None, pict_image, 0, 0, 0,
                   0, 0, 0, w, h);
  XRenderComposite(server->dsp, PictOpOver, pict_image, None, pict_drawable, 0,
                   0, 0, 0, x, y, w, h);
  imlib_context_set_blend(1);
  XFreePixmap(server->dsp, pmap_tmp);
  XRenderFreePicture(server->dsp, pict_image);
  XRenderFreePicture(server->dsp, pict_drawable);
}
