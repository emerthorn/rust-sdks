/*
 * VeilMesh patch (D6, CALLS_ARCHITECTURE_2026-09-21.md): a frame transformer
 * that hands every encoded frame to Rust and sends back whatever Rust returns.
 * libwebrtc provides only the hook; the cipher (SFrame, RFC 9605) lives in the
 * core. Nothing here knows about keys.
 *
 * Licensed under the Apache License, Version 2.0, like the rest of this crate.
 */
#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>

#include "api/frame_transformer_interface.h"
#include "api/scoped_refptr.h"
#include "livekit/rtp_receiver.h"
#include "livekit/rtp_sender.h"
#include "rust/cxx.h"

namespace livekit_ffi {

struct FrameTransformHandler;

/// Sits between encoder and packetizer (sender) or depacketizer and decoder
/// (receiver). For each frame: Rust gets direction, kind, ssrc, timestamp,
/// payload; Rust returns the new payload (empty = drop the frame).
class RustFrameTransformer : public webrtc::FrameTransformerInterface {
 public:
  explicit RustFrameTransformer(rust::Box<FrameTransformHandler> handler);
  ~RustFrameTransformer() override;

  void Transform(
      std::unique_ptr<webrtc::TransformableFrameInterface> frame) override;
  void RegisterTransformedFrameCallback(
      webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback)
      override;
  void RegisterTransformedFrameSinkCallback(
      webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
      uint32_t ssrc) override;
  void UnregisterTransformedFrameCallback() override;
  void UnregisterTransformedFrameSinkCallback(uint32_t ssrc) override;

 private:
  rust::Box<FrameTransformHandler> handler_;
  std::mutex mutex_;
  webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback_;
  std::unordered_map<uint32_t,
                     webrtc::scoped_refptr<webrtc::TransformedFrameCallback>>
      sink_callbacks_;
};

void set_sender_frame_transform(std::shared_ptr<RtpSender> sender,
                                rust::Box<FrameTransformHandler> handler);
void set_receiver_frame_transform(std::shared_ptr<RtpReceiver> receiver,
                                  rust::Box<FrameTransformHandler> handler);
void clear_sender_frame_transform(std::shared_ptr<RtpSender> sender);
void clear_receiver_frame_transform(std::shared_ptr<RtpReceiver> receiver);

}  // namespace livekit_ffi
