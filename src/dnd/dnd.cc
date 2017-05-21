#include "dnd/dnd.hh"
#include "util/common.hh"

namespace dnd {

Property::Property(const void* data, int format, unsigned long nitems,
                   Atom type)
    : data(data), format(format), nitems(nitems), type(type) {}

AutoProperty::AutoProperty(const void* data, int format, unsigned long nitems,
                           Atom type)
    : Property(data, format, nitems, type) {}

AutoProperty::~AutoProperty() { XFree(const_cast<void*>(data)); }

AutoProperty ReadProperty(Display* disp, Window w, Atom property) {
  Atom actual_type;
  int actual_format;
  unsigned long nitems;
  unsigned long bytes_after;
  unsigned char* ret = nullptr;

  int read_bytes = 1024;

  do {
    if (ret != nullptr) {
      XFree(ret);
    }

    XGetWindowProperty(disp, w, property, 0, read_bytes, False, AnyPropertyType,
                       &actual_type, &actual_format, &nitems, &bytes_after,
                       &ret);
    read_bytes *= 2;
  } while (bytes_after != 0);

  return AutoProperty{ret, actual_format, nitems, actual_type};
}

std::string BuildCommand(std::string const& dnd_launcher_exec,
                         Property const& prop) {
  const char* data = static_cast<const char*>(prop.data);
  unsigned int total = prop.nitems * prop.format / 8;

  util::string::Builder cmd;
  cmd << dnd_launcher_exec << " \"";
  for (unsigned int i = 0; i < total; i++) {
    char c = data[i];

    if (c == '\n') {
      if (i < total - 1) {
        cmd << "\" \"";
      }
    } else {
      if (c == '`' || c == '$' || c == '\\') {
        cmd << '\\';
      }

      cmd << c;
    }
  }
  cmd << '"';
  return cmd;
}

}  // namespace dnd
