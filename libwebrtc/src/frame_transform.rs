// VeilMesh patch (D6): per-frame transform hook. libwebrtc provides the hook
// (encoder→packetizer on the sender, depacketizer→decoder on the receiver);
// what happens to the bytes — SFrame — is the caller's business.
// Licensed under the Apache License, Version 2.0.

pub use webrtc_sys::frame_transform::FrameTransform;
