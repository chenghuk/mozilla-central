/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef MediaEncoder_h_
#define MediaEncoder_h_

#include "mozilla/DebugOnly.h"
#include "TrackEncoder.h"
#include "ContainerWriter.h"
#include "MediaStreamGraph.h"

namespace mozilla {

/**
 * MediaEncoder is the framework of encoding module, it controls and manages
 * procedures between ContainerWriter and TrackEncoder. ContainerWriter packs
 * the encoded track data with a specific container (e.g. ogg, mp4).
 * AudioTrackEncoder and VideoTrackEncoder are subclasses of TrackEncoder, and
 * are responsible for encoding raw data coming from MediaStreamGraph.
 *
 * Also, MediaEncoder is a type of MediaStreamListener, it starts to receive raw
 * segments after itself is added to the source stream. In the mean time,
 * encoded track data is pulled by its owner periodically on a worker thread. A
 * reentrant monitor is used to protect the push and pull of resource.
 *
 * MediaEncoder is designed to be a passive component, neither it owns nor in
 * charge of managing threads. However, a monitor is used in function
 * TrackEncoder::GetEncodedTrack() for the purpose of thread safety (e.g.
 * between callbacks of MediaStreamListener and others), a call to this function
 * might block. Therefore, MediaEncoder should not run on threads that forbid
 * blocking, such as main thread or I/O thread.
 *
 * For example, an usage from MediaRecorder of this component would be:
 * 1) Create an encoder with a valid MIME type.
 *    => encoder = MediaEncoder::CreateEncoder(aMIMEType);
 *    It then generate a ContainerWriter according to the MIME type, and an
 *    AudioTrackEncoder (or a VideoTrackEncoder too) associated with the media
 *    type.
 *
 * 2) Dispatch the task GetEncodedData() to a worker thread.
 *
 * 3) To start encoding, add this component to its source stream.
 *    => sourceStream->AddListener(encoder);
 *
 * 4) To stop encoding, remove this component from its source stream.
 *    => sourceStream->RemoveListener(encoder);
 */
class MediaEncoder : public MediaStreamListener
{
public :
  enum {
    ENCODE_METADDATA,
    ENCODE_TRACK,
    ENCODE_DONE,
  };

  MediaEncoder(ContainerWriter* aWriter,
               AudioTrackEncoder* aAudioEncoder,
               VideoTrackEncoder* aVideoEncoder,
               const nsAString& aMIMEType)
    : mWriter(aWriter)
    , mAudioEncoder(aAudioEncoder)
    , mVideoEncoder(aVideoEncoder)
    , mMIMEType(aMIMEType)
    , mState(MediaEncoder::ENCODE_METADDATA)
    , mShutdown(false)
  {}

  ~MediaEncoder() {};

  /**
   * Notified by the control loop of MediaStreamGraph; aQueueMedia is the raw
   * track data in form of MediaSegment.
   */
  virtual void NotifyQueuedTrackChanges(MediaStreamGraph* aGraph, TrackID aID,
                                        TrackRate aTrackRate,
                                        TrackTicks aTrackOffset,
                                        uint32_t aTrackEvents,
                                        const MediaSegment& aQueuedMedia);

  /**
   * Notified the stream is being removed.
   */
  virtual void NotifyRemoved(MediaStreamGraph* aGraph);

  /**
   * Creates an encoder with a given MIME type. Returns null if we are unable
   * to create the encoder. For now, default aMIMEType to "audio/ogg" and use
   * Ogg+Opus if it is empty.
   */
  static already_AddRefed<MediaEncoder> CreateEncoder(const nsAString& aMIMEType);

  /**
   * Encodes the raw track data and returns the final container data. Assuming
   * it is called on a single worker thread. The buffer of container data is
   * allocated in ContainerWriter::GetContainerData(), and is appended to
   * aOutputBufs. aMIMEType is the valid mime-type of this returned container
   * data.
   */
  void GetEncodedData(nsTArray<nsTArray<uint8_t> >* aOutputBufs,
                      nsAString& aMIMEType);

  /**
   * Return true if MediaEncoder has been shutdown. Reasons are encoding
   * complete, encounter an error, or being canceled by its caller.
   */
  bool IsShutdown()
  {
    return mShutdown;
  }

  /**
   * Cancel the encoding, and wakes up the lock of reentrant monitor in encoder.
   */
  void Cancel()
  {
    if (mAudioEncoder) {
      mAudioEncoder->NotifyCancel();
    }
  }

private:
  nsAutoPtr<ContainerWriter> mWriter;
  nsAutoPtr<AudioTrackEncoder> mAudioEncoder;
  nsAutoPtr<VideoTrackEncoder> mVideoEncoder;
  nsString mMIMEType;
  int mState;
  bool mShutdown;
};

}
#endif
