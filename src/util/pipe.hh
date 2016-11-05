#ifndef TINT3_UTIL_PIPE_HH
#define TINT3_UTIL_PIPE_HH

namespace test {

class MockSelfPipe;

}  // namespace test

namespace util {

class Pipe {
public:
  Pipe();
  ~Pipe();

  bool IsAlive() const;
  int ReadEnd() const;
  int WriteEnd() const;

private:
  friend class test::MockSelfPipe;

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
  friend class test::MockSelfPipe;

  bool alive_;
};

}  // namespace util

#endif  // TINT3_UTIL_PIPE_HH
