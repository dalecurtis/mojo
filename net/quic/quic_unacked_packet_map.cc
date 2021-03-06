// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/quic/quic_unacked_packet_map.h"

#include "base/logging.h"
#include "base/stl_util.h"
#include "net/quic/quic_connection_stats.h"
#include "net/quic/quic_utils_chromium.h"

using std::max;

namespace net {

QuicUnackedPacketMap::QuicUnackedPacketMap()
    : largest_sent_packet_(0),
      largest_observed_(0),
      least_unacked_(1),
      bytes_in_flight_(0),
      pending_crypto_packet_count_(0) {
}

QuicUnackedPacketMap::~QuicUnackedPacketMap() {
  QuicPacketSequenceNumber index = least_unacked_;
  for (UnackedPacketMap::iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++index) {
    delete it->retransmittable_frames;
    // Only delete all_transmissions once, for the newest packet.
    if (it->all_transmissions != nullptr &&
        index == *it->all_transmissions->rbegin()) {
      delete it->all_transmissions;
    }
  }
}

void QuicUnackedPacketMap::AddSentPacket(
    const SerializedPacket& packet,
    QuicPacketSequenceNumber old_sequence_number,
    TransmissionType transmission_type,
    QuicTime sent_time,
    QuicByteCount bytes_sent,
    bool set_in_flight) {
  QuicPacketSequenceNumber sequence_number = packet.sequence_number;
  DCHECK_LT(largest_sent_packet_, sequence_number);
  DCHECK_GE(sequence_number, least_unacked_ + unacked_packets_.size());
  while (least_unacked_ + unacked_packets_.size() < sequence_number) {
    unacked_packets_.push_back(TransmissionInfo());
    unacked_packets_.back().is_unackable = true;
  }

  TransmissionInfo info;
  if (old_sequence_number == 0) {
    if (packet.retransmittable_frames != nullptr &&
        packet.retransmittable_frames->HasCryptoHandshake() == IS_HANDSHAKE) {
      ++pending_crypto_packet_count_;
    }
    info = TransmissionInfo(packet.retransmittable_frames,
                            packet.sequence_number_length);
  } else {
    info = OnRetransmittedPacket(
        old_sequence_number, sequence_number, transmission_type);
  }
  info.sent_time = sent_time;

  largest_sent_packet_ = max(sequence_number, largest_sent_packet_);
  if (set_in_flight) {
    bytes_in_flight_ += bytes_sent;
    info.bytes_sent = bytes_sent;
    info.in_flight = true;
  }
  unacked_packets_.push_back(info);
}

void QuicUnackedPacketMap::RemoveObsoletePackets() {
  while (!unacked_packets_.empty()) {
    if (!IsPacketRemovable(least_unacked_, unacked_packets_.front())) {
      break;
    }
    unacked_packets_.pop_front();
    ++least_unacked_;
  }
}

TransmissionInfo QuicUnackedPacketMap::OnRetransmittedPacket(
    QuicPacketSequenceNumber old_sequence_number,
    QuicPacketSequenceNumber new_sequence_number,
    TransmissionType transmission_type) {
  DCHECK_GE(old_sequence_number, least_unacked_);
  DCHECK_LT(old_sequence_number, least_unacked_ + unacked_packets_.size());
  DCHECK_GE(new_sequence_number, least_unacked_ + unacked_packets_.size());
  DCHECK_NE(NOT_RETRANSMISSION, transmission_type);

  // TODO(ianswett): Discard and lose the packet lazily instead of immediately.
  TransmissionInfo* transmission_info =
      &unacked_packets_.at(old_sequence_number - least_unacked_);
  RetransmittableFrames* frames = transmission_info->retransmittable_frames;
  transmission_info->retransmittable_frames = nullptr;
  LOG_IF(DFATAL, frames == nullptr)
      << "Attempt to retransmit packet with no "
      << "retransmittable frames: " << old_sequence_number;

  // Only keep one transmission older than largest observed, because only the
  // most recent is expected to possibly be a spurious retransmission.
  while (transmission_info->all_transmissions != nullptr &&
         transmission_info->all_transmissions->size() > 1 &&
         *(++transmission_info->all_transmissions->begin()) <
             largest_observed_) {
    QuicPacketSequenceNumber old_transmission =
        *transmission_info->all_transmissions->begin();
    TransmissionInfo* old_info =
        &unacked_packets_[old_transmission - least_unacked_];
    // Don't remove old packets if they're still in flight.
    if (old_info->in_flight) {
      break;
    }
    old_info->all_transmissions->pop_front();
    // This will cause the packet be removed in RemoveObsoletePackets.
    old_info->all_transmissions = nullptr;
  }
  // Don't link old transmissions to new ones when version or
  // encryption changes.
  if (transmission_type == ALL_INITIAL_RETRANSMISSION ||
      transmission_type == ALL_UNACKED_RETRANSMISSION) {
    RemoveAckability(transmission_info);
  } else {
    if (transmission_info->all_transmissions == nullptr) {
      transmission_info->all_transmissions = new SequenceNumberList();
      transmission_info->all_transmissions->push_back(old_sequence_number);
    }
    transmission_info->all_transmissions->push_back(new_sequence_number);
  }
  TransmissionInfo info =
      TransmissionInfo(frames,
                       transmission_info->sequence_number_length,
                       transmission_type,
                       transmission_info->all_transmissions);
  // Proactively remove obsolete packets so the least unacked can be raised.
  RemoveObsoletePackets();
  return info;
}

void QuicUnackedPacketMap::ClearAllPreviousRetransmissions() {
  while (!unacked_packets_.empty() && least_unacked_ < largest_observed_) {
    // If this packet is in flight, or has retransmittable data, then there is
    // no point in clearing out any further packets, because they would not
    // affect the high water mark.
    TransmissionInfo* info = &unacked_packets_.front();
    if (info->in_flight || info->retransmittable_frames != nullptr) {
      break;
    }

    if (info->all_transmissions != nullptr) {
      if (info->all_transmissions->size() < 2) {
        LOG(DFATAL) << "all_transmissions must be nullptr or have multiple "
                    << "elements.  size:" << info->all_transmissions->size();
        delete info->all_transmissions;
      } else {
        info->all_transmissions->pop_front();
        if (info->all_transmissions->size() == 1) {
          // Set the newer transmission's 'all_transmissions' entry to nullptr.
          QuicPacketSequenceNumber new_transmission =
              info->all_transmissions->front();
          TransmissionInfo* new_info =
              &unacked_packets_.at(new_transmission - least_unacked_);
          delete new_info->all_transmissions;
          new_info->all_transmissions = nullptr;
        }
      }
    }
    unacked_packets_.pop_front();
    ++least_unacked_;
  }
}

bool QuicUnackedPacketMap::HasRetransmittableFrames(
    QuicPacketSequenceNumber sequence_number) const {
  DCHECK_GE(sequence_number, least_unacked_);
  DCHECK_LT(sequence_number, least_unacked_ + unacked_packets_.size());
  return unacked_packets_[sequence_number - least_unacked_]
             .retransmittable_frames != nullptr;
}

void QuicUnackedPacketMap::NackPacket(QuicPacketSequenceNumber sequence_number,
                                      size_t min_nacks) {
  DCHECK_GE(sequence_number, least_unacked_);
  DCHECK_LT(sequence_number, least_unacked_ + unacked_packets_.size());
  unacked_packets_[sequence_number - least_unacked_].nack_count =
      max(min_nacks,
          unacked_packets_[sequence_number - least_unacked_].nack_count);
}

void QuicUnackedPacketMap::RemoveRetransmittability(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK_GE(sequence_number, least_unacked_);
  DCHECK_LT(sequence_number, least_unacked_ + unacked_packets_.size());
  TransmissionInfo* info = &unacked_packets_[sequence_number - least_unacked_];
  SequenceNumberList* all_transmissions = info->all_transmissions;
  if (all_transmissions == nullptr) {
    MaybeRemoveRetransmittableFrames(info);
    return;
  }
  // TODO(ianswett): Consider adding a check to ensure there are retransmittable
  // frames associated with this packet.
  for (SequenceNumberList::const_iterator it = all_transmissions->begin();
       it != all_transmissions->end(); ++it) {
    TransmissionInfo* transmission_info =
        &unacked_packets_[*it - least_unacked_];
    MaybeRemoveRetransmittableFrames(transmission_info);
    transmission_info->all_transmissions = nullptr;
  }
  delete all_transmissions;
}

void QuicUnackedPacketMap::RemoveAckability(TransmissionInfo* info) {
  DCHECK(info->retransmittable_frames == nullptr);
  info->is_unackable = true;
  SequenceNumberList* all_transmissions = info->all_transmissions;
  if (all_transmissions == nullptr) {
    return;
  }
  for (SequenceNumberList::const_iterator it = all_transmissions->begin();
       it != all_transmissions->end(); ++it) {
    TransmissionInfo* transmission_info =
        &unacked_packets_[*it - least_unacked_];
    transmission_info->all_transmissions = nullptr;
    transmission_info->is_unackable = true;
  }
  delete all_transmissions;
}

void QuicUnackedPacketMap::MaybeRemoveRetransmittableFrames(
    TransmissionInfo* transmission_info) {
  if (transmission_info->retransmittable_frames != nullptr) {
    if (transmission_info->retransmittable_frames->HasCryptoHandshake()
            == IS_HANDSHAKE) {
      --pending_crypto_packet_count_;
    }
    delete transmission_info->retransmittable_frames;
    transmission_info->retransmittable_frames = nullptr;
  }
}

void QuicUnackedPacketMap::IncreaseLargestObserved(
    QuicPacketSequenceNumber largest_observed) {
  DCHECK_LE(largest_observed_, largest_observed);
  largest_observed_ = largest_observed;
}

bool QuicUnackedPacketMap::IsPacketUseless(
    QuicPacketSequenceNumber sequence_number,
    const TransmissionInfo& info) const {
  return (info.is_unackable || sequence_number <= largest_observed_) &&
         !info.in_flight && info.retransmittable_frames == nullptr &&
         info.all_transmissions == nullptr;
}

bool QuicUnackedPacketMap::IsPacketRemovable(
    QuicPacketSequenceNumber sequence_number,
    const TransmissionInfo& info) const {
  return (info.is_unackable || sequence_number <= largest_observed_ ||
          unacked_packets_.size() > kMaxTcpCongestionWindow) &&
         !info.in_flight && info.retransmittable_frames == nullptr &&
         info.all_transmissions == nullptr;
}

bool QuicUnackedPacketMap::IsUnacked(
    QuicPacketSequenceNumber sequence_number) const {
  if (sequence_number < least_unacked_ ||
      sequence_number >= least_unacked_ + unacked_packets_.size()) {
    return false;
  }
  return !IsPacketUseless(sequence_number,
                          unacked_packets_[sequence_number - least_unacked_]);
}

void QuicUnackedPacketMap::RemoveFromInFlight(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK_GE(sequence_number, least_unacked_);
  DCHECK_LT(sequence_number, least_unacked_ + unacked_packets_.size());
  TransmissionInfo* info = &unacked_packets_[sequence_number - least_unacked_];
  if (info->in_flight) {
    LOG_IF(DFATAL, bytes_in_flight_ < info->bytes_sent);
    bytes_in_flight_ -= info->bytes_sent;
    info->in_flight = false;
  }
}

bool QuicUnackedPacketMap::HasUnackedPackets() const {
  return !unacked_packets_.empty();
}

bool QuicUnackedPacketMap::HasInFlightPackets() const {
  return bytes_in_flight_ > 0;
}

const TransmissionInfo& QuicUnackedPacketMap::GetTransmissionInfo(
    QuicPacketSequenceNumber sequence_number) const {
  return unacked_packets_[sequence_number - least_unacked_];
}

QuicTime QuicUnackedPacketMap::GetLastPacketSentTime() const {
  UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
  while (it != unacked_packets_.rend()) {
    if (it->in_flight) {
      LOG_IF(DFATAL, it->sent_time == QuicTime::Zero())
          << "Sent time can never be zero for a packet in flight.";
      return it->sent_time;
    }
    ++it;
  }
  LOG(DFATAL) << "GetLastPacketSentTime requires in flight packets.";
  return QuicTime::Zero();
}

QuicTime QuicUnackedPacketMap::GetFirstInFlightPacketSentTime() const {
  UnackedPacketMap::const_iterator it = unacked_packets_.begin();
  while (it != unacked_packets_.end() && !it->in_flight) {
    ++it;
  }
  if (it == unacked_packets_.end()) {
    LOG(DFATAL) << "GetFirstInFlightPacketSentTime requires in flight packets.";
    return QuicTime::Zero();
  }
  return it->sent_time;
}

size_t QuicUnackedPacketMap::GetNumUnackedPacketsDebugOnly() const {
  size_t unacked_packet_count = 0;
  QuicPacketSequenceNumber sequence_number = least_unacked_;
  for (UnackedPacketMap::const_iterator it = unacked_packets_.begin();
       it != unacked_packets_.end(); ++it, ++sequence_number) {
    if (!IsPacketUseless(sequence_number, *it)) {
      ++unacked_packet_count;
    }
  }
  return unacked_packet_count;
}

bool QuicUnackedPacketMap::HasMultipleInFlightPackets() const {
  size_t num_in_flight = 0;
  for (UnackedPacketMap::const_reverse_iterator it = unacked_packets_.rbegin();
       it != unacked_packets_.rend(); ++it) {
    if (it->in_flight) {
      ++num_in_flight;
    }
    if (num_in_flight > 1) {
      return true;
    }
  }
  return false;
}

bool QuicUnackedPacketMap::HasPendingCryptoPackets() const {
  return pending_crypto_packet_count_ > 0;
}

bool QuicUnackedPacketMap::HasUnackedRetransmittableFrames() const {
  for (UnackedPacketMap::const_reverse_iterator it =
           unacked_packets_.rbegin(); it != unacked_packets_.rend(); ++it) {
    if (it->in_flight && it->retransmittable_frames) {
      return true;
    }
  }
  return false;
}

QuicPacketSequenceNumber
QuicUnackedPacketMap::GetLeastUnacked() const {
  return least_unacked_;
}

void QuicUnackedPacketMap::RestoreInFlight(
    QuicPacketSequenceNumber sequence_number) {
  DCHECK_GE(sequence_number, least_unacked_);
  DCHECK_LT(sequence_number, least_unacked_ + unacked_packets_.size());
  TransmissionInfo* info = &unacked_packets_[sequence_number - least_unacked_];
  DCHECK(!info->in_flight);
  DCHECK_NE(0u, info->bytes_sent);
  DCHECK(info->sent_time.IsInitialized());

  bytes_in_flight_ += info->bytes_sent;
  info->in_flight = true;
}

}  // namespace net
