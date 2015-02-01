/*
 * tint3 - A lightweight panel for X11.
 * Copyright (C) 2015  Daniele Cocca <daniele.cocca@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "dbus/interface.h"

#include "util/log.h"

#include <dbus/dbus.h>

namespace dbus {

Interface::Interface(Connection const* const bus,
                     std::string const& destination, std::string const& path,
                     std::string const& interface)
    : bus_(bus),
      destination_(destination),
      path_(path),
      interface_(interface) {}

bool Interface::Call(const std::string& method) {
  if (!bus_->IsConnected()) {
    return false;
  }

  DBusMessage* method_message = dbus_message_new_method_call(
      destination_.empty() ? nullptr : destination_.c_str(), path_.c_str(),
      interface_.empty() ? nullptr : interface_.c_str(), method.c_str());

  if (method_message == nullptr) {
    util::log::Error() << "No memory for dbus_message_new_method_call().\n";
    return false;
  }

  bool result =
      dbus_connection_send(bus_->connection_, method_message, nullptr);
  dbus_message_unref(method_message);
  return result;
}

}  // namespace dbus
