#ifndef LINUX_SYSFS_H
#define LINUX_SYSFS_H

#include <string>
#include <vector>

#include "battery/battery_interface.h"

namespace linux_sysfs {

std::vector<std::string> GetBatteryDirectories();

class Battery : public BatteryInterface {
 public:
  Battery(std::string const& base_path);

  bool Found() const;
  bool Update();
  double charge_percentage() const;
  ChargeState charge_state() const;
  unsigned int seconds_to_charge() const;

 private:
  std::string path_current_now_;
  std::string path_energy_now_;
  std::string path_energy_full_;
  std::string path_status_;
  bool found_;
  ChargeState charge_state_;
  double charge_percentage_;
  unsigned int seconds_to_charge_;
};

}  // namespace linux_sysfs

#endif  // LINUX_SYSFS_H
