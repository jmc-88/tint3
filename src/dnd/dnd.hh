#ifndef TINT3_DND_DND_HH
#define TINT3_DND_DND_HH

#include <X11/Xatom.h>
#include <X11/Xlib.h>

#include <memory>
#include <string>

namespace dnd {

// Holds data returned by XGetWindowProperty.
class Property {
 public:
  Property(const void* data, int format, unsigned long nitems, Atom type);
  Property(Property&&) = default;
  Property(Property const&) = delete;
  Property& operator=(Property) = delete;

  const void* data;
  const int format;
  const unsigned long nitems;
  const Atom type;
};

// Same as Property, but automatically frees the allocated memory with XFree.
class AutoProperty : public Property {
 public:
  AutoProperty(const void* data, int format, unsigned long nitems, Atom type);
  ~AutoProperty();

  AutoProperty(AutoProperty&&) = default;
  AutoProperty(AutoProperty const&) = delete;
  AutoProperty& operator=(AutoProperty) = delete;
};

AutoProperty ReadProperty(Display* disp, Window w, Atom property);
std::string BuildCommand(std::string const& dnd_launcher_exec,
                         Property const& prop);

Atom PickTargetFromList(Display* disp, Atom const* atom_list, int nitems);
Atom PickTargetFromAtoms(Display* disp, Atom t1, Atom t2, Atom t3);
Atom PickTargetFromTargets(Display* disp, Property const& p);

}  // namespace dnd

#endif  // TINT3_DND_DND_HH
