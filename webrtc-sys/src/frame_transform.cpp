/*
 * VeilMesh patch (D6): see include/livekit/frame_transform.h.
 * Licensed under the Apache License, Version 2.0.
 */
#include "livekit/frame_transform.h"

#include "api/make_ref_counted.h"
#include "webrtc-sys/src/frame_transform.rs.h"

namespace livekit_ffi {

RustFrameTransformer::RustFrameTransformer(
    rust::Box<FrameTransformHandler> handler)
    : handler_(std::move(handler)) {}

RustFrameTransformer::~RustFrameTransformer() = default;

void RustFrameTransformer::Transform(
    std::unique_ptr<webrtc::TransformableFrameInterface> frame) {
  const bool outgoing =
      frame->GetDirection() ==
      webrtc::TransformableFrameInterface::Direction::kSender;
  const bool is_video = frame->GetMimeType().rfind("video/", 0) == 0;
  const bool key_frame =
      is_video &&
      static_cast<webrtc::TransformableVideoFrameInterface*>(frame.get())
          ->IsKeyFrame();

  auto data = frame->GetData();
  rust::Slice<const uint8_t> input(data.data(), data.size());
  rust::Vec<uint8_t> output = handler_->transform(
      outgoing, is_video, key_frame, frame->GetSsrc(), frame->GetTimestamp(),
      input);
  if (output.empty()) {
    // Rust dropped the frame (e.g. undecryptable): nothing goes further.
    return;
  }
  frame->SetData(std::span<const uint8_t>(output.data(), output.size()));

  webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = sink_callbacks_.find(frame->GetSsrc());
    callback = it != sink_callbacks_.end() ? it->second : callback_;
  }
  if (callback) {
    callback->OnTransformedFrame(std::move(frame));
  }
}

void RustFrameTransformer::RegisterTransformedFrameCallback(
    webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback) {
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = std::move(callback);
}

void RustFrameTransformer::RegisterTransformedFrameSinkCallback(
    webrtc::scoped_refptr<webrtc::TransformedFrameCallback> callback,
    uint32_t ssrc) {
  std::lock_guard<std::mutex> lock(mutex_);
  sink_callbacks_[ssrc] = std::move(callback);
}

void RustFrameTransformer::UnregisterTransformedFrameCallback() {
  std::lock_guard<std::mutex> lock(mutex_);
  callback_ = nullptr;
}

void RustFrameTransformer::UnregisterTransformedFrameSinkCallback(
    uint32_t ssrc) {
  std::lock_guard<std::mutex> lock(mutex_);
  sink_callbacks_.erase(ssrc);
}

void set_sender_frame_transform(std::shared_ptr<RtpSender> sender,
                                rust::Box<FrameTransformHandler> handler) {
  auto transformer =
      webrtc::make_ref_counted<RustFrameTransformer>(std::move(handler));
  sender->rtc_sender()->SetEncoderToPacketizerFrameTransformer(transformer);
}

void set_receiver_frame_transform(std::shared_ptr<RtpReceiver> receiver,
                                  rust::Box<FrameTransformHandler> handler) {
  auto transformer =
      webrtc::make_ref_counted<RustFrameTransformer>(std::move(handler));
  receiver->rtc_receiver()->SetDepacketizerToDecoderFrameTransformer(
      transformer);
}

void clear_sender_frame_transform(std::shared_ptr<RtpSender> sender) {
  sender->rtc_sender()->SetEncoderToPacketizerFrameTransformer(nullptr);
}

void clear_receiver_frame_transform(std::shared_ptr<RtpReceiver> receiver) {
  receiver->rtc_receiver()->SetDepacketizerToDecoderFrameTransformer(nullptr);
}

}  // namespace livekit_ffi
