#ifndef TINT3_UTIL_WINDOW_HH
#define TINT3_UTIL_WINDOW_HH

#include <glib.h>
#include <pango/pangocairo.h>

#include <string>
#include <vector>

void SetActive(Window win);
void SetDesktop(int desktop);
void SetClose(Window win);
std::vector<std::string> ServerGetDesktopNames();
int WindowIsIconified(Window win);
int WindowIsUrgent(Window win);
int WindowIsHidden(Window win);
int WindowIsActive(Window win);
int WindowIsSkipTaskbar(Window win);
int GetIconCount(gulong* data, int num);
gulong* GetBestIcon(gulong* data, int icon_count, int num, int* iw, int* ih,
                    int best_icon_size);
void WindowMaximizeRestore(Window win);
void WindowToggleShade(Window win);
int WindowGetDesktop(Window win);
void WindowSetDesktop(Window win, int desktop);
int WindowGetMonitor(Window win);
Window WindowGetActive();

void GetTextSize(PangoFontDescription* font, int* height_ink, int* height,
                 int panel_height, std::string const& text);
void GetTextSize2(PangoFontDescription* font, int* height_ink, int* height,
                  int* width, int panel_height, int panel_with,
                  std::string const& text);

#endif  // TINT3_UTIL_WINDOW_HH
