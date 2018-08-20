#ifndef TINT3_BEHAVIOR_CONTROL_HH
#define TINT3_BEHAVIOR_CONTROL_HH

#ifdef __clang__
#define FORCE_STD_MOVE_START        \
  _Pragma("clang diagnostic push"); \
  _Pragma("clang diagnostic ignored \"-Wpessimizing-move\"");
#else  // __clang__
#define FORCE_STD_MOVE_START
#endif  // __clang__

#ifdef __clang__
#define FORCE_STD_MOVE_END _Pragma("clang diagnostic pop");
#else  // __clang__
#define FORCE_STD_MOVE_END
#endif  // __clang__

#define FORCE_STD_MOVE(code) \
  FORCE_STD_MOVE_START;      \
  code;                      \
  FORCE_STD_MOVE_END;

#endif  // TINT3_BEHAVIOR_CONTROL_HH