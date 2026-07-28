#pragma once

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string_view>
#include <system_error>
#include <utility>

namespace log_core {

class RotatingFile {
public:
  RotatingFile(std::filesystem::path path, size_t max_size,
               size_t max_files)
      : path_(std::move(path)), max_size_(max_size), max_files_(max_files) {
    open_file();
  }

  RotatingFile(const RotatingFile &) = delete;
  RotatingFile &operator=(const RotatingFile &) = delete;

  ~RotatingFile() {
    if (file_.is_open())
      file_.close();
  }

  bool is_open() const noexcept { return file_.is_open(); }

  bool write(std::string_view data) {
    if (!file_.is_open())
      return false;

    // A zero limit disables size-based rotation. A single oversized record is
    // written to an empty file instead of causing a rotate loop.
    if (max_size_ != 0 && current_size_ != 0 &&
        data.size() > max_size_ - std::min(current_size_, max_size_)) {
      if (!rotate())
        return false;
    }

    file_.write(data.data(), static_cast<std::streamsize>(data.size()));
    if (!file_)
      return false;
    current_size_ += data.size();
    return true;
  }

  void flush() {
    if (file_.is_open())
      file_.flush();
  }

private:
  bool open_file() {
    std::error_code ec;
    if (const auto parent = path_.parent_path(); !parent.empty())
      std::filesystem::create_directories(parent, ec);

    file_.clear();
    file_.open(path_, std::ios::out | std::ios::app | std::ios::binary);
    if (!file_.is_open())
      return false;

    file_.seekp(0, std::ios::end);
    const auto position = file_.tellp();
    if (position < 0) {
      current_size_ = 0;
      return true;
    }
    current_size_ = static_cast<size_t>(position);
    return true;
  }

  bool rotate() {
    if (max_files_ == 0)
      return true;

    if (file_.is_open())
      file_.close();

    std::error_code ec;
    auto backup = [this](size_t index) {
      auto path = path_;
      path += "." + std::to_string(index);
      return path;
    };

    const auto oldest = backup(max_files_);
    std::filesystem::remove(oldest, ec);
    if (ec && ec != std::errc::no_such_file_or_directory) {
      open_file();
      return false;
    }

    for (size_t i = max_files_; i > 1; --i) {
      const auto source = backup(i - 1);
      const auto destination = backup(i);
      ec.clear();
      if (!std::filesystem::exists(source, ec)) {
        if (ec) {
          open_file();
          return false;
        }
        continue;
      }
      std::filesystem::rename(source, destination, ec);
      if (ec) {
        open_file();
        return false;
      }
    }

    ec.clear();
    if (std::filesystem::exists(path_, ec)) {
      if (ec)
        {
          open_file();
          return false;
        }
      std::filesystem::rename(path_, backup(1), ec);
      if (ec)
        {
          open_file();
          return false;
        }
    } else if (ec) {
      open_file();
      return false;
    }

    return open_file();
  }

  std::filesystem::path path_;
  std::ofstream file_;
  size_t max_size_ = 0;
  size_t max_files_ = 0;
  size_t current_size_ = 0;
};

} // namespace log_core
