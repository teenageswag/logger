#pragma once

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

namespace log_core {

// One writer owns the active path and all its backups.
class RotatingFile {
public:
  RotatingFile(std::filesystem::path path, uint64_t max_size, size_t max_files)
      : path_(std::move(path)), max_size_(max_size), max_files_(max_files) {
    if (path_.empty())
      throw std::invalid_argument("empty log path");
    file_.exceptions(std::ios::badbit | std::ios::failbit);
    open_file();
  }
  RotatingFile(const RotatingFile &) = delete;
  RotatingFile &operator=(const RotatingFile &) = delete;

  bool is_open() const noexcept { return file_.is_open(); }

  bool write(std::string_view data) {
    if (max_size_ && current_size_ &&
        data.size() > max_size_ - (std::min)(current_size_, max_size_))
      rotate();
    while (!data.empty()) {
      const auto size =
          (std::min)(data.size(),
                     static_cast<size_t>((std::numeric_limits<std::streamsize>::max)()));
      file_.write(data.data(), static_cast<std::streamsize>(size));
      if (size > (std::numeric_limits<uint64_t>::max)() - current_size_)
        throw std::overflow_error("log file size overflow");
      current_size_ += size;
      data.remove_prefix(size);
    }
    return true;
  }
  void flush() { file_.flush(); }

private:
  void open_file(bool truncate = false) {
    if (const auto parent = path_.parent_path(); !parent.empty())
      std::filesystem::create_directories(parent);
    file_.clear();
    errno = 0;
    try {
      file_.open(path_,
                 std::ios::out | std::ios::binary | (truncate ? std::ios::trunc : std::ios::app));
    } catch (const std::ios_base::failure &) {
      throw std::system_error(errno ? std::error_code(errno, std::generic_category())
                                    : std::make_error_code(std::errc::io_error),
                              "open log file");
    }
    current_size_ = std::filesystem::file_size(path_);
  }
  std::filesystem::path backup(size_t index) const {
    auto result = path_;
    result += "." + std::to_string(index);
    return result;
  }
  void rotate() {
    file_.close(); // Detect flush/close failures before renaming.
    try {
      if (max_files_ == 0) {
        open_file(true);
        return;
      }
      std::filesystem::remove(backup(max_files_));
      for (size_t i = max_files_; i > 1; --i)
        if (std::filesystem::exists(backup(i - 1)))
          std::filesystem::rename(backup(i - 1), backup(i));
      std::filesystem::rename(path_, backup(1));
      open_file();
    } catch (...) {
      // Preserve the original error; later records can retry after recovery.
      if (!file_.is_open()) {
        try {
          open_file();
        } catch (...) {
        }
      }
      throw;
    }
  }

  std::filesystem::path path_;
  std::ofstream file_;
  uint64_t max_size_ = 0, current_size_ = 0;
  size_t max_files_ = 0;
};

} // namespace log_core
