#ifndef TINT3_BATTERY_FREEBSD_ACPIIO_HH
#define TINT3_BATTERY_FREEBSD_ACPIIO_HH

#include "battery/battery_interface.hh"

namespace freebsd_acpiio {

class Battery : public BatteryInterface {
 public:
  Battery();

  bool Found() const;
  bool Update();
  double charge_percentage() const;
  ChargeState charge_state() const;
  unsigned int seconds_to_charge() const;

 private:
  bool found_;
  ChargeState charge_state_;
  double charge_percentage_;
  unsigned int seconds_to_charge_;
};

}  // namespace freebsd_acpiio

#endif  // TINT3_BATTERY_FREEBSD_ACPIIO_HH
