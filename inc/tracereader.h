/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TRACEREADER_H
#define TRACEREADER_H

#include <cstring>
#include <deque>
#include <memory>
#include <numeric>
#include <string>
#include <type_traits>

#include "instruction.h"
#include "util/detect.h"
#include <iostream>

namespace champsim
{
class tracereader
{
  static uint64_t instr_unique_id; // NOLINT(cppcoreguidelines-avoid-non-const-global-variables)
  struct reader_concept {
    virtual ~reader_concept() = default;
    virtual ooo_model_instr operator()() = 0;
    [[nodiscard]] virtual bool eof() const = 0;
  };

  template <typename T>
  struct reader_model final : public reader_concept {
    T intern_;
    reader_model(T&& val) : intern_(std::move(val)) {}

    template <typename U>
    using has_eof = decltype(std::declval<U>().eof());

    ooo_model_instr operator()() override { return intern_(); }
    [[nodiscard]] bool eof() const override
    {
      if constexpr (champsim::is_detected_v<has_eof, T>) {
        return intern_.eof();
      }
      return false; // If an eof() member function is not provided, assume the trace never ends.
    }
  };

  std::unique_ptr<reader_concept> pimpl_;

public:
  template <typename T, std::enable_if_t<!std::is_same_v<tracereader, T>, bool> = true>
  tracereader(T&& val) : pimpl_(std::make_unique<reader_model<T>>(std::forward<T>(val)))
  {
  }

  auto operator()()
  {
    auto retval = (*pimpl_)();
    retval.instr_id = instr_unique_id++;
    return retval;
  }

  [[nodiscard]] auto eof() const { return pimpl_->eof(); }
};

template <typename T, typename F>
class bulk_tracereader
{
  static_assert(std::is_trivial_v<T>);
  static_assert(std::is_standard_layout_v<T>);

  uint8_t cpu;
  bool eof_ = false;
  F trace_file;

  constexpr static std::size_t buffer_size = 128;
  constexpr static std::size_t refresh_thresh = 1;
  std::deque<ooo_model_instr> instr_buffer;

  bool read_exact(char* buffer, std::size_t size)
  {
    trace_file.read(buffer, static_cast<std::streamsize>(size));
    return static_cast<std::size_t>(trace_file.gcount()) == size;
  }

public:
  ooo_model_instr operator()();

  bulk_tracereader(uint8_t cpu_idx, std::string tf) : cpu(cpu_idx), trace_file(tf) {}
  bulk_tracereader(uint8_t cpu_idx, F&& file) : cpu(cpu_idx), trace_file(std::move(file)) {}

  [[nodiscard]] bool eof() const { return eof_ && instr_buffer.empty(); }
};

ooo_model_instr apply_branch_target(ooo_model_instr branch, const ooo_model_instr& target);

template <typename It>
void set_branch_targets(It begin, It end)
{
  std::reverse_iterator rbegin{end};
  std::reverse_iterator rend{begin};
  std::adjacent_difference(rbegin, rend, rbegin, apply_branch_target);
}

template <typename T, typename F>
ooo_model_instr bulk_tracereader<T, F>::operator()()
{
  if (std::size(instr_buffer) <= refresh_thresh && !eof_) {
    while (instr_buffer.size() < buffer_size) {
      T raw_instr{};

      if (!read_exact(reinterpret_cast<char*>(&raw_instr), sizeof(T))) {
	if (trace_file.gcount() != 0) {
	  throw std::runtime_error("Truncated instruction record in trace");
	}
        eof_ = true;
        break;
      }

      ooo_model_instr instr{this->cpu, raw_instr};

      if constexpr (std::is_same_v<T, input_instr>) {
        if (raw_instr.read_count > NUM_INSTR_SOURCES ||
	    raw_instr.write_count > NUM_INSTR_DESTINATIONS) {
	  throw std::runtime_error("Invalid memory-operation count in trace");
	}
        instr.source_memory_size.assign(raw_instr.source_memory_size,
		                      raw_instr.source_memory_size + raw_instr.read_count);

        instr.destination_memory_size.assign(raw_instr.destination_memory_size,
		                raw_instr.destination_memory_size + raw_instr.write_count);

        std::size_t total_read_bytes = 0;
	for (size_t i = 0; i < instr.source_memory_size.size(); i++)
          total_read_bytes += instr.source_memory_size[i];

	instr.source_memory_data.resize(total_read_bytes);
	if (total_read_bytes > 0 &&
	    !read_exact(reinterpret_cast<char*>(instr.source_memory_data.data()), total_read_bytes)) {
          throw std::runtime_error("Truncated read-data payload in trace");
        }
	std::size_t total_write_bytes = 0;
	for (size_t i = 0; i < instr.destination_memory_size.size(); i++)
	  total_write_bytes += instr.destination_memory_size[i];

	instr.destination_memory_data.resize(total_write_bytes);
	if (total_write_bytes > 0 &&
	    !read_exact(reinterpret_cast<char*>(instr.destination_memory_data.data()), total_write_bytes)) {
          throw std::runtime_error("Truncated write-data payload in trace");
        }
      }
      instr_buffer.push_back(std::move(instr));
    }

    // Set branch targets
    if (!instr_buffer.empty())
      set_branch_targets(std::begin(instr_buffer), std::end(instr_buffer));
  }

  if (instr_buffer.empty())
    throw std::runtime_error("Attempted to read instruction past end of trace");

  auto retval = instr_buffer.front();
  instr_buffer.pop_front();

  return retval;
}

std::string get_fptr_cmd(std::string_view fname);
} // namespace champsim

champsim::tracereader get_tracereader(const std::string& fname, uint8_t cpu, bool is_cloudsuite, bool repeat);

#endif
