#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include <cstring>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "util/log.hh"
#include "util/pipe.hh"

constexpr unsigned int kBufferSize = 1024;
constexpr char kPipeName[] = "/tint3_pipe_test_shm";

class PipeTestFixture {
 public:
  PipeTestFixture() : pipe_shm_fd_(-1), pipe_mmap_addr_(MAP_FAILED) {
    shm_unlink(kPipeName);
    pipe_shm_fd_ =
        shm_open(kPipeName, O_RDWR | O_CREAT | O_EXCL, S_IWUSR | S_IRUSR);
    if (pipe_shm_fd_ == -1) {
      FAIL("shm_open(" << kPipeName << "): " << strerror(errno));
    }

    if (ftruncate(pipe_shm_fd_, kBufferSize) == -1) {
      shm_unlink(kPipeName);
      FAIL("ftruncate(" << pipe_shm_fd_ << "): " << strerror(errno));
    }

    pipe_mmap_addr_ = mmap(buffer_, kBufferSize, PROT_READ | PROT_WRITE,
                           MAP_SHARED, pipe_shm_fd_, 0);
    if (pipe_mmap_addr_ == MAP_FAILED) {
      shm_unlink(kPipeName);
      FAIL("mmap(" << static_cast<void*>(buffer_) << "): " << strerror(errno));
    }

    std::memset(pipe_mmap_addr_, 0, kBufferSize);
  }

  ~PipeTestFixture() {
    if (pipe_mmap_addr_ != MAP_FAILED) {
      if (munmap(pipe_mmap_addr_, kBufferSize) != 0) {
        WARN("munmap(" << static_cast<void*>(pipe_mmap_addr_)
                       << "): " << strerror(errno));
      }
    }

    if (pipe_shm_fd_ != -1) {
      if (close(pipe_shm_fd_) != 0) {
        WARN("close(" << pipe_shm_fd_ << "): " << strerror(errno));
      }

      if (shm_unlink(kPipeName) != 0) {
        WARN("shm_unlink(" << kPipeName << "): " << strerror(errno));
      }
    }
  }

  int GetFileDescriptor() const { return pipe_shm_fd_; }

  void* GetMMapAddress() const { return pipe_mmap_addr_; }

  unsigned int CountNonZeroBytes() const {
    const char* mmap_addr = static_cast<char*>(GetMMapAddress());
    unsigned int written_bytes = 0;
    for (; *mmap_addr != '\0'; ++mmap_addr) {
      ++written_bytes;
    }
    return written_bytes;
  }

 protected:
  int pipe_shm_fd_;
  void* pipe_mmap_addr_;
  char buffer_[kBufferSize];
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
