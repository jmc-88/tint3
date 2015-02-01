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

#ifndef TINT3_DBUS_INTERFACE_H
#define TINT3_DBUS_INTERFACE_H

#include "dbus/connection.h"

#include <string>

namespace dbus {

class Connection;
class Interface {
  friend class Connection;

 public:
  bool Call(std::string const& method);

 private:
  Interface(Connection const* const bus, std::string const& destination,
            std::string const& path, std::string const& interface);

  Connection const* const bus_;
  std::string const destination_;
  std::string const path_;
  std::string const interface_;
};

}  // namespace dbus

#endif  // TINT3_DBUS_INTERFACE_H
