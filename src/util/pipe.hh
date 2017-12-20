#ifndef TINT3_UTIL_PIPE_HH
#define TINT3_UTIL_PIPE_HH

namespace test {

class MockSelfPipe;

}  // namespace test

namespace util {

class Pipe {
 public:
  enum class Options {
    kNone,
    kNonBlocking,
  };

  Pipe(Options opts = Options::kNone);
  ~Pipe();

  bool IsAlive() const;
  int ReadEnd() const;
  int WriteEnd() const;

 private:
  friend class test::MockSelfPipe;

  bool alive_;
  int pipe_fd_[2];

  bool SetFlag(int fd, int flag) const;
};

class SelfPipe : public Pipe {
 public:
  SelfPipe();

  void WriteOneByte();
  void ReadPendingBytes();
};

}  // namespace util

#endif  // TINT3_UTIL_PIPE_HH
