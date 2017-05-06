#include <algorithm>
#include <cstdint>  // acpiio.h expects uintN_t types to be defined

#include <dev/acpica/acpiio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "battery/freebsd_acpiio.hh"
#include "util/log.hh"

namespace {

constexpr char kAcpiDevice[] = "/dev/acpi";

int GetBatteries() {
  int fd = open(kAcpiDevice, O_RDONLY);
  if (fd != -1) {
    int units;
    int rc = ioctl(fd, ACPIIO_BATT_GET_UNITS, &units);
    close(fd);
    if (rc != -1) {
      return units;
    }
  }
  return -1;
}

ChargeState GetChargeState(int state) {
  if (state & ACPI_BATT_STAT_CHARGING) {
    return ChargeState::kCharging;
  }
  if (state & (ACPI_BATT_STAT_DISCHARG | ACPI_BATT_STAT_CRITICAL)) {
    return ChargeState::kDischarging;
  }
  return ChargeState::kUnknown;
}

}  // namespace

namespace freebsd_acpiio {

Battery::Battery()
    : found_(false),
      charge_state_(ChargeState::kUnknown),
      charge_percentage_(0),
      seconds_to_charge_(0) {
  int units = GetBatteries();
  if (units != -1) {
    found_ = true;
    util::log::Debug() << "freebsd_acpiio: Found " << units << " batteries.\n";
  }
}

bool Battery::Found() const { return found_; }

bool Battery::Update() {
  int fd = open(kAcpiDevice, O_RDONLY);
  if (fd == -1) {
    return false;
  }

  union acpi_battery_ioctl_arg bi;
  bi.unit = ACPI_BATTERY_ALL_UNITS;
  if (ioctl(fd, ACPIIO_BATT_GET_BATTINFO, &bi) == -1) {
    close(fd);
    return false;
  }

  charge_percentage_ = bi.battinfo.cap;
  charge_state_ = GetChargeState(bi.battinfo.state);
  // FIXME: we cap this at 0 since charging states return a negative value for
  // min. We can obviously do better, but this is a starting point.
  seconds_to_charge_ = std::max(0, bi.battinfo.min * 60);

  close(fd);
  return true;
}

double Battery::charge_percentage() const { return charge_percentage_; }

ChargeState Battery::charge_state() const { return charge_state_; }

unsigned int Battery::seconds_to_charge() const { return seconds_to_charge_; }

}  // namespace freebsd_acpiio
