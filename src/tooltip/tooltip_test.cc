#include "catch.hpp"

#include <X11/Xlib.h>

#include <chrono>

#include "panel.hh"
#include "server.hh"
#include "tooltip/tooltip.hh"
#include "util/area.hh"
#include "util/environment.hh"
#include "util/timer_test_utils.hh"

namespace {

const int kTooltipTimeoutMs = 500;

// Will set up a Panel for a 1024x768 screen area.
const int kScreenWidth = 1024;
const int kScreenHeight = 768;

}  // namespace

// Area is an abstract class because it has a pure virtual destructor.
// Subclass it here so that we can instantiate it for testing.
class ConcreteArea : public Area {
 public:
  ConcreteArea() : Area() {
    // Set up a fake 500x50 horizontal panel that is horizontally centered and
    // vertically aligned to the bottom of a 1024x768 screen.
    PanelConfig panel_config;
    panel_config.horizontal = true;
    panel_config.width = 500;
    panel_config.height = 50;
    panel_config.horizontal_position = PanelHorizontalPosition::kCenter;
    panel_config.vertical_position = PanelVerticalPosition::kBottom;
    panel_ = new Panel();
    panel_->UseConfig(panel_config, 1);
    panel_->root_x_ = (kScreenWidth - panel_->width_) / 2;
    panel_->root_y_ = (kScreenHeight - panel_->height_);
    // Set the size of this Area.
    width_ = 50;
    height_ = 50;
  }

  ~ConcreteArea() { delete panel_; }
};

class TooltipTestFixture {
 public:
  TooltipTestFixture()
      : fake_clock_(new FakeClock{0}),
        timer_(new Timer{[&] { return fake_clock_->Now(); }}),
        tooltip_(nullptr) {
    tooltip_config = TooltipConfig{};
    tooltip_config.font_desc = pango_font_description_from_string("sans 10");
    tooltip_config.hide_timeout_msec = kTooltipTimeoutMs;
    tooltip_config.show_timeout_msec = kTooltipTimeoutMs;

    server.dsp = XOpenDisplay(nullptr);
    if (!server.dsp) {
      FAIL("Couldn't connect to the X server on DISPLAY="
           << environment::Get("DISPLAY"));
    }
    server.InitX11();
    GetMonitors();

    tooltip_ = new Tooltip{&server, timer_};
  }

  ~TooltipTestFixture() {
    delete tooltip_;
    delete timer_;
    delete fake_clock_;
    server.Cleanup();
  }

  FakeClock* fake_clock() const { return fake_clock_; }

  Timer* timer() const { return timer_; }

  Tooltip* tooltip() const { return tooltip_; }

  XWindowAttributes GetTooltipWindowAttributes() const {
    XWindowAttributes xwa;
    if (!XGetWindowAttributes(server.dsp, tooltip_->window(), &xwa)) {
      FAIL("XGetWindowAttributes() failed");
    }
    return xwa;
  }

 private:
  FakeClock* fake_clock_;
  Timer* timer_;
  Tooltip* tooltip_;
};

TEST_CASE_METHOD(TooltipTestFixture, "IsBound") {
  REQUIRE_FALSE(tooltip()->IsBound());

  ConcreteArea area;
  tooltip()->Update(&area, nullptr, "test");
  REQUIRE(tooltip()->IsBound());
}

TEST_CASE_METHOD(TooltipTestFixture, "IsBoundTo") {
  ConcreteArea area;
  REQUIRE_FALSE(tooltip()->IsBoundTo(&area));

  tooltip()->Update(&area, nullptr, "test");
  REQUIRE(tooltip()->IsBoundTo(&area));
}

namespace {

constexpr char kLoremIpsum[] =
    "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod "
    "tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim "
    "veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea "
    "commodo consequat. Duis aute irure dolor in reprehenderit in voluptate "
    "velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
    "occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
    "mollit anim id est laborum.";

}  // namespace

TEST_CASE_METHOD(TooltipTestFixture, "Update") {
  // Correct binding is already tested by the IsBound and IsBoundTo tests.
  ConcreteArea area;
  tooltip()->Update(&area, nullptr, kLoremIpsum);

  XWindowAttributes xwa = GetTooltipWindowAttributes();
  // Should not escape the screen area to the left.
  REQUIRE(xwa.x == 0);
  // Should be anchored to the top edge of the panel.
  REQUIRE(xwa.y + xwa.height == area.panel_->root_y_);
  // Despite the ridiculously long text, this shouldn't exceed the screen size.
  REQUIRE(xwa.width <= kScreenWidth);
}

TEST_CASE_METHOD(TooltipTestFixture, "Show") {
  // Trigger a Show event.
  ConcreteArea area;
  tooltip()->Show(&area, nullptr, "test");

  // Clock step, in milliseconds.
  // It's such that kClockStep * 3 > kTooltipTimeoutMs.
  const int kClockStep = (kTooltipTimeoutMs * 0.4);

  // kTooltipTimeoutMs not elapsed yet: window not mapped.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kClockStep));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsUnmapped);

  // kTooltipTimeoutMs not elapsed yet: window not mapped.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kClockStep));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsUnmapped);

  // kTooltipTimeoutMs elapsed: window is mapped.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kClockStep));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);
}

TEST_CASE_METHOD(TooltipTestFixture, "Hide") {
  // Make window shown first.
  ConcreteArea area;
  tooltip()->Show(&area, nullptr, "test");
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kTooltipTimeoutMs));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);

  // Trigger a Hide event.
  tooltip()->Hide();

  // Clock step, in milliseconds.
  // It's such that kClockStep * 3 > kTooltipTimeoutMs.
  const int kClockStep = (kTooltipTimeoutMs * 0.4);

  // kTooltipTimeoutMs not elapsed yet: window is mapped.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kClockStep));
  timer()->ProcessExpiredIntervals();

  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);

  // kTooltipTimeoutMs not elapsed yet: window is mapped.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kClockStep));
  timer()->ProcessExpiredIntervals();

  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);

  // kTooltipTimeoutMs elapsed: window is not mapped.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kClockStep));
  timer()->ProcessExpiredIntervals();

  REQUIRE(GetTooltipWindowAttributes().map_state == IsUnmapped);
}

TEST_CASE_METHOD(TooltipTestFixture, "CancelTimeout") {
  // Make window shown first.
  ConcreteArea area;
  tooltip()->Show(&area, nullptr, "test");
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kTooltipTimeoutMs));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);

  // Trigger a Hide event.
  // Advance the time, but not enough for the hide timeout to happen.
  tooltip()->Hide();
  fake_clock()->AdvanceBy(
      std::chrono::milliseconds(static_cast<int>(kTooltipTimeoutMs * .67)));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);

  // Trigger a Show event.
  // Advance the time, enough for the hide timeout to have happened if it wasn't
  // canceled by Show().
  tooltip()->Show(&area, nullptr, "test");
  fake_clock()->AdvanceBy(
      std::chrono::milliseconds(static_cast<int>(kTooltipTimeoutMs * .34)));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);

  // Tooltip stays visible afterwards.
  fake_clock()->AdvanceBy(std::chrono::milliseconds(kTooltipTimeoutMs));
  timer()->ProcessExpiredIntervals();
  REQUIRE(GetTooltipWindowAttributes().map_state == IsViewable);
}
