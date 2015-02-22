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

Connection Connection::SystemBus() {
  DBusError err;
  dbus_error_init(&err);

  DBusConnection* connection = dbus_bus_get(DBUS_BUS_SYSTEM, &err);

  if (dbus_error_is_set(&err)) {
    util::log::Error() << "Unable to connect to the system bus: " << err.message
                       << '\n';
    dbus_error_free(&err);
    connection = nullptr;
  }

  return Connection(connection);
}

namespace {

int DBusWatchGetFileDescriptor(DBusWatch* watch) {
  int fd = dbus_watch_get_socket(watch);

  if (fd == -1) {
    fd = dbus_watch_get_unix_fd(watch);
  }

  return fd;
}

}  // namespace

Connection::Connection(DBusConnection* connection) : connection_(connection), should_dispatch_(false) {
  if (connection_ == nullptr) {
    return;
  }

  if (!SetWatchFunctions()) {
    util::log::Error() << "dbus_connection_set_watch_functions() failed.\n";
    // As for now we only have shared (system bus) connections, there is no
    // need to close them.  If we'll ever add support for private connections,
    // then we'll also need to use dbus_connection_close() on them, instead of
    // just dereferencing them.
    dbus_connection_unref(connection_);
    connection_ = nullptr;
    return;
  }

  SetDispatchStatusFunction();
}

Connection::~Connection() {
  if (connection_ != nullptr) {
    dbus_connection_unref(connection_);
  }
}

bool Connection::SetWatchFunctions() {
  auto add_function = [&](DBusWatch* watch, void*) -> dbus_bool_t {
    int fd = DBusWatchGetFileDescriptor(watch);
    unsigned int flags = dbus_watch_get_flags(watch);

    util::log::Debug() << "DBusWatch: add fd " << fd << ", flags " << flags << '\n';

    // TODO: implement
    return TRUE;
  };

  auto remove_function = [&](DBusWatch* watch, void*) -> void {
    // TODO: implement
  };

  auto toggled_function = [&](DBusWatch* watch, void*) -> void {
    // TODO: implement
  };

  dbus_bool_t ret = dbus_connection_set_watch_functions(
    connection_, add_function, remove_function, toggled_function, nullptr,
    nullptr);

  return (ret == TRUE);
}

void Connection::SetDispatchStatusFunction() {
  auto dispatch_status_function = [](DBusConnection*,
                                     DBusDispatchStatus new_status,
                                     void* data) -> void {
    auto connection = reinterpret_cast<Connection*>(data);
    connection->should_dispatch_ = (new_status == DBUS_DISPATCH_DATA_REMAINS);
  };

  dbus_connection_set_dispatch_status_function(
    connection_,
    dispatch_status_function,
    this,
    nullptr);
}

Connection::operator bool() const { return connection_ != nullptr; }

bool Connection::IsConnected() const { return connection_ != nullptr; }

bool Connection::HasIncomingMessages() const { return should_dispatch_; }

dbus::Interface Connection::Interface(std::string const& destination,
                                      std::string const& path,
                                      std::string const& interface) const {
  return dbus::Interface(this, destination, path, interface);
}

}  // namespace dbus
