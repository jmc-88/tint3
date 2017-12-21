#include "catch.hpp"

#include <cstring>
#include <string>
#include <utility>

#include <fcntl.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "unix_features.hh"
#include "util/log.hh"
#include "util/pipe.hh"

TEST_CASE("move constructor") {
  util::Pipe p1;

  int read_fd = p1.ReadEnd();
  int write_fd = p1.WriteEnd();
  REQUIRE(p1.IsAlive());
  REQUIRE(read_fd != -1);
  REQUIRE(write_fd != -1);

#ifdef __clang__
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wpessimizing-move"
#endif  // __clang__
  util::Pipe p2{std::move(p1)};
#ifdef __clang__
#pragma clang diagnostic pop
#endif  // __clang__

  REQUIRE(p2.IsAlive());
  REQUIRE(p2.ReadEnd() == read_fd);
  REQUIRE(p2.WriteEnd() == write_fd);
  REQUIRE_FALSE(p1.IsAlive());
  REQUIRE(p1.ReadEnd() == -1);
  REQUIRE(p1.WriteEnd() == -1);
}

TEST_CASE("non blocking") {
  auto assert_nonblocking = [](int fd) {
    int flags = fcntl(fd, F_GETFL);
    if (flags == -1) {
      FAIL("fcntl(" << fd << ", F_GETFL): " << strerror(errno));
    }
    if ((flags & O_NONBLOCK) == 0) {
      FAIL("file descriptor " << fd << " is blocking");
    }
  };

  util::Pipe p{util::Pipe::Options::kNonBlocking};
  INFO("ReadEnd: file descriptor " << p.ReadEnd());
  assert_nonblocking(p.ReadEnd());
  INFO("WriteEnd: file descriptor " << p.WriteEnd());
  assert_nonblocking(p.WriteEnd());
}

constexpr unsigned int kBufferSize = 1024;
constexpr char kPipeName[] = "/tint3_pipe_test_shm";
constexpr char kTempTemplate[] = "/tmp/tint3_pipe_test.XXXXXX";

class SharedMemory {
 public:
  SharedMemory(const char* shm_name, unsigned int shm_size)
      : shm_name_(shm_name), shm_size_(shm_size), shm_fd_(-1), mem_(nullptr) {
#ifdef TINT3_HAVE_SHM_OPEN
    shm_unlink(shm_name_.c_str());
    shm_fd_ = shm_open(shm_name_.c_str(), O_RDWR | O_CREAT | O_EXCL,
                       S_IWUSR | S_IRUSR);
    if (shm_fd_ == -1) {
      FAIL("shm_open(" << shm_name_ << "): " << strerror(errno));
    }
#else   // TINT3_HAVE_SHM_OPEN
    char temp_name[sizeof(kTempTemplate) + 1] = {'\0'};
    std::strcpy(temp_name, kTempTemplate);
    shm_fd_ = mkstemp(temp_name);
    if (shm_fd_ == -1) {
      FAIL("mkstemp(): " << strerror(errno));
    }
    shm_name_.assign(temp_name);
#endif  // TINT3_HAVE_SHM_OPEN

    if (ftruncate(shm_fd_, shm_size) == -1) {
      close(shm_fd_);
#ifdef TINT3_HAVE_SHM_OPEN
      shm_unlink(shm_name_.c_str());
#endif  // TINT3_HAVE_SHM_OPEN
      FAIL("ftruncate(" << shm_fd_ << "): " << strerror(errno));
    }

    mem_ = mmap(nullptr, shm_size_, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_,
                0);
    if (mem_ == MAP_FAILED) {
      close(shm_fd_);
#ifdef TINT3_HAVE_SHM_OPEN
      shm_unlink(shm_name_.c_str());
#endif  // TINT3_HAVE_SHM_OPEN
      FAIL("mmap(): " << strerror(errno));
    }

    std::memset(mem_, 0, shm_size_);
  }

  ~SharedMemory() {
    if (mem_ != MAP_FAILED) {
      if (munmap(mem_, shm_size_) != 0) {
        WARN("munmap(" << mem_ << "): " << strerror(errno));
      }
    }

    if (shm_fd_ != -1) {
      if (close(shm_fd_) != 0) {
        WARN("close(" << shm_fd_ << "): " << strerror(errno));
      }

#ifdef TINT3_HAVE_SHM_OPEN
      if (shm_unlink(shm_name_.c_str()) != 0) {
        WARN("shm_unlink(" << shm_name_ << "): " << strerror(errno));
      }
#else   // TINT3_HAVE_SHM_OPEN
      if (unlink(shm_name_.c_str()) != 0) {
        WARN("unlink(" << shm_name_ << "): " << strerror(errno));
      }
#endif  // TINT3_HAVE_SHM_OPEN
    }
  }

  int fd() const { return shm_fd_; }

  void* addr() const { return mem_; }

 private:
  std::string shm_name_;
  unsigned int shm_size_;
  int shm_fd_;
  void* mem_;
};

class PipeTestFixture {
 public:
  PipeTestFixture() : shm_(kPipeName, kBufferSize) {}

  int GetFileDescriptor() const { return shm_.fd(); }

  unsigned int CountNonZeroBytes() const {
    const char* ptr = static_cast<char*>(shm_.addr());
    unsigned int written_bytes = 0;
    for (; *ptr != '\0'; ++ptr) {
      ++written_bytes;
    }
    return written_bytes;
  }

 protected:
  const SharedMemory shm_;
};

namespace test {

class MockSelfPipe : public util::SelfPipe {
 public:
  explicit MockSelfPipe(int shm_fd) {
    pipe_fd_[0] = dup(shm_fd);
    if (pipe_fd_[0] == -1) {
      FAIL("dup(" << shm_fd << "): " << strerror(errno));
    }

    pipe_fd_[1] = dup(shm_fd);
    if (pipe_fd_[1] == -1) {
      close(pipe_fd_[0]);
      FAIL("dup(" << shm_fd << "): " << strerror(errno));
    }

    alive_ = true;
  }

  ~MockSelfPipe() {
    // util::~Pipe() closes the file descriptors.
  }
};

}  // namespace test

TEST_CASE_METHOD(PipeTestFixture, "WriteOneByte", "Writing works.") {
  test::MockSelfPipe mock_self_pipe{GetFileDescriptor()};

  mock_self_pipe.WriteOneByte();
  REQUIRE(CountNonZeroBytes() == 1);

  mock_self_pipe.WriteOneByte();
  mock_self_pipe.WriteOneByte();
  REQUIRE(CountNonZeroBytes() == 3);
}

TEST_CASE_METHOD(PipeTestFixture, "ReadPendingBytes", "Reading works.") {
  test::MockSelfPipe mock_self_pipe{GetFileDescriptor()};
  // This will read as long as there's something to read -- in our case, it's
  // until the end of file, which is determined by the above ftruncate()
  // (i.e., kBufferSize bytes).
  mock_self_pipe.ReadPendingBytes();
  off_t position = lseek(GetFileDescriptor(), 0, SEEK_CUR);
  REQUIRE(static_cast<unsigned int>(position) == kBufferSize);
}
