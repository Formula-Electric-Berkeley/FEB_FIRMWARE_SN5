/**
 ******************************************************************************
 * @file           : feb_can_publisher.hpp
 * @brief          : Periodic TX for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_PUBLISHER_HPP
#define FEB_CAN_PUBLISHER_HPP

#include "feb_board_config.hpp"
#include "feb_can_lib.h"
#include "feb_can_scheduler.hpp"
#include "feb_can_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace feb::can
{
template <class M>
class Publisher
{
public:

  using Fill = bool (*)(typename M::Data &out);
  using FillCtx = bool (*)(typename M::Data &out, void *ctx);

  static_assert(M::kSender == Node::kNone || M::kSender == kThisNode,
                "this board is not the declared sender of this message");
  static_assert(M::kLength <= kMaxPayload, "message is longer than this board's transport");

  explicit Publisher(Fill fill, std::uint32_t period_ms = M::kCycleMs,
                     std::uint32_t offset_ms = kAutoOffset) noexcept
      : fill_(fill)
  {
    join(period_ms, offset_ms);
  }

  Publisher(FillCtx fill, void *ctx, std::uint32_t period_ms = M::kCycleMs,
            std::uint32_t offset_ms = kAutoOffset) noexcept
      : fill_ctx_(fill), ctx_(ctx)
  {
    join(period_ms, offset_ms);
  }

  ~Publisher() { Scheduler::remove(&entry_); }

  Publisher(const Publisher &) = delete;
  Publisher &operator=(const Publisher &) = delete;

  void stop() { Scheduler::remove(&entry_); }

  void start()
  {
    if (entry_.period_ms != 0)
    {
      Scheduler::add(&entry_);
    }
  }
  bool publish() { return emit(); }

  std::uint32_t offset_ms() const { return entry_.offset_ms; }
  std::uint32_t tx_count() const { return entry_.tx_count; }
  std::uint32_t skip_count() const { return entry_.skip_count; }
  std::uint32_t error_count() const { return entry_.error_count; }

private:
  void join(std::uint32_t period_ms, std::uint32_t offset_ms)
  {
    entry_.frame_id = M::kFrameId;
    entry_.period_ms = period_ms;
    entry_.requested_offset = offset_ms;
    entry_.emit = &trampoline;
    if (period_ms != 0)
    {
      Scheduler::add(&entry_);
    }
  }

  static bool trampoline(Entry *e)
  {
    static_assert(offsetof(Publisher, entry_) == 0 && std::is_standard_layout_v<Publisher>,
                  "entry_ must stay the first member: the callback casts between them");
    return reinterpret_cast<Publisher *>(e)->emit();
  }

  bool emit()
  {
    typename M::Data data{};
    const bool filled = fill_ ? fill_(data) : (fill_ctx_ && fill_ctx_(data, ctx_));
    if (!filled)
    {
      entry_.skip_count++;
      return false;
    }

    std::uint8_t buf[M::kLength]{};
    if (M::kPack(buf, &data, sizeof(buf)) != static_cast<int>(M::kLength))
    {
      entry_.error_count++;
      return false;
    }

    if (FEB_CAN_TX_Send(kVehicleBus, M::kFrameId,
                        M::kExtended ? FEB_CAN_ID_EXT : FEB_CAN_ID_STD, buf,
                        M::kLength) != FEB_CAN_OK)
    {
      entry_.error_count++;
      return false;
    }

    entry_.tx_count++;
    return true;
  }

  Entry entry_{};
  Fill fill_ = nullptr;
  FillCtx fill_ctx_ = nullptr;
  void *ctx_ = nullptr;
};

}  // namespace feb::can

#endif  /* FEB_CAN_PUBLISHER_HPP */
