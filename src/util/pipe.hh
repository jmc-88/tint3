#ifndef TINT3_UTIL_PIPE_HH
#define TINT3_UTIL_PIPE_HH

namespace util {

class Pipe {
public:
  Pipe();
  ~Pipe();

  bool IsAlive() const;
  int ReadEnd() const;
  int WriteEnd() const;

private:
  bool alive_;
  int pipe_fd_[2];
};

class SelfPipe : public Pipe {
public:
  SelfPipe();

  bool IsAlive() const;
  void WriteOneByte();
  void ReadPendingBytes();

private:
  bool alive_;
};

}  // namespace util

#endif  // TINT3_UTIL_PIPE_HH
