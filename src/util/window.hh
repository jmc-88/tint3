#ifndef TINT3_UTIL_WINDOW_HH
#define TINT3_UTIL_WINDOW_HH

#include <string>
#include <vector>

#include "util/pango.hh"

namespace util {
namespace window {

void SetActive(Window win);
void SetClose(Window win);
int IsIconified(Window win);
int IsUrgent(Window win);
int IsHidden(Window win);
int IsActive(Window win);
int IsSkipTaskbar(Window win);
void MaximizeRestore(Window win);
void ToggleShade(Window win);
int GetDesktop(Window win);
void SetDesktop(Window win, int desktop);
unsigned int GetMonitor(Window win);
Window GetActive();

}  // namespace window
}  // namespace util

void SetDesktop(int desktop);

int GetIconCount(unsigned long* data, int num);
unsigned long* GetBestIcon(unsigned long* data, int icon_count, int num,
                           int* iw, int* ih, int best_icon_size);

void GetTextSize(util::pango::FontDescriptionPtr const& font,
                 std::string const& text, int* width, int* height);

#endif  // TINT3_UTIL_WINDOW_HH
