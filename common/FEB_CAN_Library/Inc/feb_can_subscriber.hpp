/**
 ******************************************************************************
 * @file           : feb_can_subscriber.hpp
 * @brief          : Latest-value RX for FEB CAN Library
 * @author         : Formula Electric @ Berkeley
 ******************************************************************************
 */

#ifndef FEB_CAN_SUBSCRIBER_HPP
#define FEB_CAN_SUBSCRIBER_HPP

#include "feb_board_config.hpp"
#include "feb_can_lib.h"
#include "feb_can_rx_registry.hpp"
#include "feb_can_traits.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace feb::can
{
template <class M>
class Subscriber
{
public:

  static constexpr std::uint32_t kStaleMs = M::kCycleMs * 3u;

  static_assert(M::kLength <= kMaxPayload, "message is longer than this board's transport");

  Subscriber() noexcept
  {
    node_.frame_id = M::kFrameId;
    node_.extended = M::kExtended;
    node_.attach = &attach;
    RxRegistry::link(&node_);
  }

  ~Subscriber() { RxRegistry::unlink(&node_); }

  Subscriber(const Subscriber &) = delete;
  Subscriber &operator=(const Subscriber &) = delete;

  using OnFrame = void (*)(const typename M::Data &);
  void on_receive(OnFrame fn) { on_receive_ = fn; }

  const typename M::Data &value() const { return data_; }
  const typename M::Data &v() const { return data_; }

  typename M::Data snapshot() const
  {
    typename M::Data out;
    std::uint32_t before;
    std::uint32_t after;
    do
    {
      before = seq_;
      out = data_;
      after = seq_;
    } while ((before & 1u) != 0u || before != after);
    return out;
  }

  bool registered() const { return node_.handle >= 0; }
  bool present() const { return present_; }
  std::uint32_t rx_count() const { return rx_count_; }
  std::uint32_t last_rx_ms() const { return last_rx_ms_; }

  std::uint32_t age_ms() const
  {
    return present_ ? (FEB_CAN_Now() - last_rx_ms_) : 0xFFFFFFFFu;
  }

  bool fresh() const
  {
    static_assert(M::kCycleMs > 0, "message has no cycle time to derive staleness from");
    return present_ && (FEB_CAN_Now() - last_rx_ms_) < kStaleMs;
  }

private:
  static void attach(RxNode *n)
  {
    static_assert(offsetof(Subscriber, node_) == 0 && std::is_standard_layout_v<Subscriber>,
                  "node_ must stay the first member: the callback casts between them");
    Subscriber *self = reinterpret_cast<Subscriber *>(n);
    const FEB_CAN_RX_Params_t params = {
        .instance = kVehicleBus,
        .can_id = M::kFrameId,
        .id_type = M::kExtended ? FEB_CAN_ID_EXT : FEB_CAN_ID_STD,
        .filter_type = FEB_CAN_FILTER_EXACT,
        .mask = M::kExtended ? 0x1FFFFFFFu : 0x7FFu,
        .fifo = FEB_CAN_FIFO_0,
        .callback = &on_frame,
        .user_data = self,
    };
    n->handle = FEB_CAN_RX_Register(&params);
  }

  static void on_frame(FEB_CAN_Instance_t, uint32_t, FEB_CAN_ID_Type_t, const uint8_t *data,
                       uint8_t length, void *user_data)
  {
    Subscriber *self = static_cast<Subscriber *>(user_data);

    typename M::Data decoded;
    if (M::kUnpack(&decoded, data, length) < 0)
    {
      self->error_count_++;
      return;
    }

    self->seq_ = self->seq_ + 1;
    self->data_ = decoded;
    self->seq_ = self->seq_ + 1;

    self->last_rx_ms_ = FEB_CAN_Now();
    self->rx_count_++;
    self->present_ = true;

    if (self->on_receive_ != nullptr)
    {
      self->on_receive_(self->data_);
    }
  }

  RxNode node_{};
  volatile std::uint32_t seq_ = 0;
  OnFrame on_receive_ = nullptr;
  typename M::Data data_{};
  std::uint32_t last_rx_ms_ = 0;
  std::uint32_t rx_count_ = 0;
  std::uint32_t error_count_ = 0;
  bool present_ = false;
};

template <class M>
inline Subscriber<M> rx;

}  // namespace feb::can

#endif  /* FEB_CAN_SUBSCRIBER_HPP */
