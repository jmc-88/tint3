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

#include "dbus/connection.h"

#include "util/log.h"

namespace dbus {

Connection* Connection::SystemBus() {
  DBusError err;
  dbus_error_init(&err);

  DBusConnection* conn = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

  if (dbus_error_is_set(&err)) {
    util::log::Error() << "Unable to connect to the system bus: \"" << err.name
                       << ": " << err.message << "\"\n";
    dbus_error_free(&err);
    return nullptr;
  }

  return new Connection(conn);
}

Connection::Connection(DBusConnection* connection) : connection_(connection) {}

DBusConnection* Connection::Get() const { return connection_; }

}  // namespace dbus
