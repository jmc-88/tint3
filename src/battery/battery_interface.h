#ifndef BATTERY_INTERFACE_H
#define BATTERY_INTERFACE_H
enum class ChargeState { kUnknown, kCharging, kDischarging, kFull };

struct BatteryTimestamp {
  int16_t hours;
  int8_t minutes;
  int8_t seconds;
};

struct BatteryState {
  int percentage;
  BatteryTimestamp time;
  ChargeState state;
};

class BatteryInterface {
 public:
  virtual bool Found() const = 0;
  virtual bool Update() = 0;
  virtual double charge_percentage() const = 0;
  virtual ChargeState charge_state() const = 0;
  virtual unsigned int seconds_to_charge() const = 0;
};

#endif  // BATTERY_INTERFACE_H
