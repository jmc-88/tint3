#include <cstring>

#include "dnd/dnd.hh"
#include "server.hh"
#include "util/common.hh"

namespace {

char const* GetAtomName(Display* disp, Atom a) {
  if (a == None) {
    return "None";
  }
  return XGetAtomName(disp, a);
}

}  // namespace

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

// This function takes a list of targets which can be converted to (atom_list,
// nitems) and a list of acceptable targets with prioritees (datatypes). It
// returns the highest entry in datatypes which is also in atom_list: i.e. it
// finds the best match.
Atom PickTargetFromList(Display* disp, Atom const* atom_list, int nitems) {
  Atom to_be_requested = None;

  for (int i = 0; i < nitems; ++i) {
    char const* atom_name = GetAtomName(disp, atom_list[i]);

    // See if this data type is allowed and of higher priority (closer to zero)
    // than the present one.
    if (std::strcmp(atom_name, "STRING") == 0) {
      to_be_requested = atom_list[i];
    }
  }

  return to_be_requested;
}

// Finds the best target given up to three atoms provided (any can be None).
Atom PickTargetFromAtoms(Display* disp, Atom t1, Atom t2, Atom t3) {
  Atom atoms[3] = {None};
  int n = 0;

  if (t1 != None) {
    atoms[n++] = t1;
  }

  if (t2 != None) {
    atoms[n++] = t2;
  }

  if (t3 != None) {
    atoms[n++] = t3;
  }

  return PickTargetFromList(disp, atoms, n);
}

// Finds the best target given a local copy of a property.
Atom PickTargetFromTargets(Display* disp, dnd::Property const& p) {
  if ((p.type != XA_ATOM && p.type != server.atoms_["TARGETS"]) ||
      p.format != 32) {
    // This would be really broken. Targets have to be an atom list
    // and applications should support this. Nevertheless, some
    // seem broken (MATLAB 7, for instance), so ask for STRING
    // next instead as the lowest common denominator
    return XA_STRING;
  }

  return PickTargetFromList(disp, static_cast<Atom const*>(p.data), p.nitems);
}

}  // namespace dnd
