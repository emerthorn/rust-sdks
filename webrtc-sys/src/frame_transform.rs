// VeilMesh patch (D6): frame transform hook — every encoded frame passes
// through Rust. See include/livekit/frame_transform.h.
// Licensed under the Apache License, Version 2.0.

use std::sync::Arc;

#[cxx::bridge(namespace = "livekit_ffi")]
pub mod ffi {
    unsafe extern "C++" {
        include!("livekit/frame_transform.h");
        include!("livekit/rtp_sender.h");
        include!("livekit/rtp_receiver.h");

        type RtpSender = crate::rtp_sender::ffi::RtpSender;
        type RtpReceiver = crate::rtp_receiver::ffi::RtpReceiver;

        pub fn set_sender_frame_transform(
            sender: SharedPtr<RtpSender>,
            handler: Box<FrameTransformHandler>,
        );
        pub fn set_receiver_frame_transform(
            receiver: SharedPtr<RtpReceiver>,
            handler: Box<FrameTransformHandler>,
        );
        pub fn clear_sender_frame_transform(sender: SharedPtr<RtpSender>);
        pub fn clear_receiver_frame_transform(receiver: SharedPtr<RtpReceiver>);
    }

    extern "Rust" {
        type FrameTransformHandler;

        /// `outgoing` — encoder→packetizer (encrypt), otherwise
        /// depacketizer→decoder (decrypt). Empty result drops the frame.
        fn transform(
            self: &FrameTransformHandler,
            outgoing: bool,
            is_video: bool,
            key_frame: bool,
            ssrc: u32,
            rtp_timestamp: u32,
            data: &[u8],
        ) -> Vec<u8>;
    }
}

/// What Rust does with a frame. Called on libwebrtc's encoder/decoder
/// threads: must be cheap and must not block on the signaling thread.
pub trait FrameTransform: Send + Sync {
    fn transform(
        &self,
        outgoing: bool,
        is_video: bool,
        key_frame: bool,
        ssrc: u32,
        rtp_timestamp: u32,
        data: &[u8],
    ) -> Vec<u8>;
}

pub struct FrameTransformHandler {
    inner: Arc<dyn FrameTransform>,
}

impl FrameTransformHandler {
    pub fn new(inner: Arc<dyn FrameTransform>) -> Self {
        Self { inner }
    }

    fn transform(
        &self,
        outgoing: bool,
        is_video: bool,
        key_frame: bool,
        ssrc: u32,
        rtp_timestamp: u32,
        data: &[u8],
    ) -> Vec<u8> {
        self.inner
            .transform(outgoing, is_video, key_frame, ssrc, rtp_timestamp, data)
    }
}
