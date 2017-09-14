#include "battery/linux_sysfs.hh"
#include "util/common.hh"
#include "util/fs.hh"

namespace linux_sysfs {

std::vector<std::string> GetBatteryDirectories() {
  static constexpr const char kPowerSupply[] = "/sys/class/power_supply";

  std::vector<std::string> directories;

  for (auto& entry : util::fs::DirectoryContents(kPowerSupply)) {
    if (entry.substr(0, 2) == "AC") {
      continue;
    }

    auto sys_path = util::fs::BuildPath({kPowerSupply, entry});

    if (util::fs::FileExists({sys_path, "present"})) {
      directories.push_back(sys_path);
    }
  }

  return directories;
}

Battery::Battery(const std::string& base_path)
    : found_(false),
      charge_state_(ChargeState::kUnknown),
      charge_percentage_(0.0),
      seconds_to_charge_(0) {
  if (util::fs::FileExists({base_path, "energy_now"})) {
    path_energy_now_ = util::fs::BuildPath({base_path, "energy_now"});
    path_energy_full_ = util::fs::BuildPath({base_path, "energy_full"});
  } else if (util::fs::FileExists({base_path, "charge_now"})) {
    path_energy_now_ = util::fs::BuildPath({base_path, "charge_now"});
    path_energy_full_ = util::fs::BuildPath({base_path, "charge_full"});
  }

  path_current_now_ = util::fs::BuildPath({base_path, "power_now"});

  if (!util::fs::FileExists(path_current_now_)) {
    path_current_now_ = util::fs::BuildPath({base_path, "current_now"});
  }

  if (!path_energy_now_.empty() && !path_energy_full_.empty()) {
    path_status_ = util::fs::BuildPath({base_path, "status"});
  }

  found_ = (!path_current_now_.empty() && !path_energy_now_.empty() &&
            !path_energy_full_.empty() && !path_status_.empty());
}

bool Battery::Found() const { return found_; }

bool Battery::Update() {
  static constexpr const char kStatusCharging[] = "Charging\n";
  static constexpr const char kStatusDischarging[] = "Discharging\n";
  static constexpr const char kStatusFull[] = "Full\n";

  if (!found_) {
    return false;
  }

  charge_state_ = ChargeState::kUnknown;

  util::fs::ReadFile(path_status_, [&](std::string const& contents) {
    if (contents == kStatusCharging) {
      charge_state_ = ChargeState::kCharging;
    } else if (contents == kStatusDischarging) {
      charge_state_ = ChargeState::kDischarging;
    } else if (contents == kStatusFull) {
      charge_state_ = ChargeState::kFull;
    }
    return true;
  });

  long int energy_now = 0;
  util::fs::ReadFile(path_energy_now_, [&](std::string const& contents) {
    return util::string::ToNumber(contents, &energy_now);
  });

  long int energy_full = 0;
  util::fs::ReadFile(path_energy_full_, [&](std::string const& contents) {
    return util::string::ToNumber(contents, &energy_full);
  });

  long int current_now = 0;
  util::fs::ReadFile(path_current_now_, [&](std::string const& contents) {
    return util::string::ToNumber(contents, &current_now);
  });

  if (energy_full > 0) {
    charge_percentage_ = std::min(100.0, (energy_now * 100.0) / energy_full);
  }

  seconds_to_charge_ = 0;

  if (current_now > 0) {
    switch (charge_state_) {
      case ChargeState::kCharging:
        seconds_to_charge_ = 3600 * (energy_full - energy_now) / current_now;
        break;

      case ChargeState::kDischarging:
        seconds_to_charge_ = 3600 * energy_now / current_now;
        break;

      default:
        break;
    }
  }

  return true;
}

double Battery::charge_percentage() const { return charge_percentage_; }

ChargeState Battery::charge_state() const { return charge_state_; }

unsigned int Battery::seconds_to_charge() const { return seconds_to_charge_; }

}  // namespace linux_sysfs
