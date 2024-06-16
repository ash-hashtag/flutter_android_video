/*
 * Copyright (C) 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* This is a JNI example where we use native methods to play video
 * using the native AMedia* APIs.
 * See the corresponding Java source file located at:
 *
 *   src/com/example/nativecodec/NativeMedia.java
 *
 * In this example we use assert() for "impossible" error conditions,
 * and explicit handling and recovery for more likely error conditions.
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <jni.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "looper.hpp"
#include "media/NdkMediaCodec.h"
#include "media/NdkMediaExtractor.h"

// for __android_log_print(ANDROID_LOG_INFO, "YourApp", "formatted message");
#include <android/log.h>

#define TAG "NativeCodec"
#define LOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, TAG, __VA_ARGS__)

// for native window JNI
#include <android/asset_manager.h>
#include <android/asset_manager_jni.h>
#include <android/native_window_jni.h>
#include <malloc.h>

typedef struct {
  int fd;
  ANativeWindow *window;
  AMediaExtractor *ex;
  AMediaCodec *codec;
  int64_t renderstart;
  bool sawInputEOS;
  bool sawOutputEOS;
  bool isPlaying;
  bool renderonce;
} workerdata;

// workerdata data = {-1, NULL, NULL, NULL, 0, false, false, false, false};

enum {
  kMsgCodecBuffer,
  kMsgPause,
  kMsgResume,
  kMsgPauseAck,
  kMsgDecodeDone,
  kMsgSeek,
};

class mylooper : public looper {
  virtual void handle(int what, void *obj);
};

// static mylooper *mlooper = NULL;

int64_t systemnanotime() {
  timespec now;
  clock_gettime(CLOCK_MONOTONIC, &now);
  return now.tv_sec * 1000000000LL + now.tv_nsec;
}

class FlNativeVideo {
public:
  workerdata data;
  workerdata audiodata;
  mylooper *mlooper;

  FlNativeVideo() {
    this->data = {-1, NULL, NULL, NULL, 0, false, false, false, false};
    this->audiodata = {-1, NULL, NULL, NULL, 0, false, false, false, false};

    this->mlooper = new mylooper();
  }

  ~FlNativeVideo() {
    this->mlooper->quit();
    delete this->mlooper;
  }
};

void doCodecWork(FlNativeVideo *video) {
  workerdata *d = &video->data;
  ssize_t bufidx = -1;
  if (!d->sawInputEOS) {
    bufidx = AMediaCodec_dequeueInputBuffer(d->codec, 2000);
    // LOGV("input buffer %zd", bufidx);
    if (bufidx >= 0) {
      size_t bufsize;
      auto buf = AMediaCodec_getInputBuffer(d->codec, bufidx, &bufsize);
      auto sampleSize = AMediaExtractor_readSampleData(d->ex, buf, bufsize);
      if (sampleSize < 0) {
        sampleSize = 0;
        d->sawInputEOS = true;
        LOGV("EOS");
      }
      auto presentationTimeUs = AMediaExtractor_getSampleTime(d->ex);

      AMediaCodec_queueInputBuffer(
          d->codec, bufidx, 0, sampleSize, presentationTimeUs,
          d->sawInputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
      AMediaExtractor_advance(d->ex);
    }
  }

  if (!d->sawOutputEOS) {
    AMediaCodecBufferInfo info;
    auto status = AMediaCodec_dequeueOutputBuffer(d->codec, &info, 0);
    if (status >= 0) {
      if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
        LOGV("output EOS");
        d->sawOutputEOS = true;
      }
      int64_t presentationNano = info.presentationTimeUs * 1000;
      if (d->renderstart < 0) {
        d->renderstart = systemnanotime() - presentationNano;
      }
      int64_t delay = (d->renderstart + presentationNano) - systemnanotime();
      if (delay > 0) {
        usleep(delay / 1000);
      }
      AMediaCodec_releaseOutputBuffer(d->codec, status, info.size != 0);
      if (d->renderonce) {
        d->renderonce = false;
        return;
      }
    } else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
      LOGV("output buffers changed");
    } else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
      auto format = AMediaCodec_getOutputFormat(d->codec);
      LOGV("format changed to: %s", AMediaFormat_toString(format));
      AMediaFormat_delete(format);
    } else if (status == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
      LOGV("no output buffer right now");
    } else {
      LOGV("unexpected info code: %zd", status);
    }
  }

  // Audio Processing
  // d = &video->audiodata;
  // bufidx = -1;
  // if (!d->sawInputEOS) {
  //   bufidx = AMediaCodec_dequeueInputBuffer(d->codec, 2000);
  //   // LOGV("input buffer %zd", bufidx);
  //   if (bufidx >= 0) {
  //     size_t bufsize;
  //     auto buf = AMediaCodec_getInputBuffer(d->codec, bufidx, &bufsize);
  //     auto sampleSize = AMediaExtractor_readSampleData(d->ex, buf, bufsize);
  //     if (sampleSize < 0) {
  //       sampleSize = 0;
  //       d->sawInputEOS = true;
  //       LOGV("EOS");
  //     }
  //     auto presentationTimeUs = AMediaExtractor_getSampleTime(d->ex);

  //     AMediaCodec_queueInputBuffer(
  //         d->codec, bufidx, 0, sampleSize, presentationTimeUs,
  //         d->sawInputEOS ? AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM : 0);
  //     AMediaExtractor_advance(d->ex);
  //   }
  // }

  // if (!d->sawOutputEOS) {
  //   AMediaCodecBufferInfo info;
  //   auto status = AMediaCodec_dequeueOutputBuffer(d->codec, &info, 0);
  //   if (status >= 0) {
  //     if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
  //       LOGV("output EOS");
  //       d->sawOutputEOS = true;
  //     }
  //     int64_t presentationNano = info.presentationTimeUs * 1000;
  //     if (d->renderstart < 0) {
  //       d->renderstart = systemnanotime() - presentationNano;
  //     }
  //     int64_t delay = (d->renderstart + presentationNano) - systemnanotime();
  //     if (delay > 0) {
  //       usleep(delay / 1000);
  //     }
  //     AMediaCodec_releaseOutputBuffer(d->codec, status, info.size != 0);
  //     if (d->renderonce) {
  //       d->renderonce = false;
  //       return;
  //     }
  //   } else if (status == AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED) {
  //     LOGV("output buffers changed");
  //   } else if (status == AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED) {
  //     auto format = AMediaCodec_getOutputFormat(d->codec);
  //     LOGV("format changed to: %s", AMediaFormat_toString(format));
  //     AMediaFormat_delete(format);
  //   } else if (status == AMEDIACODEC_INFO_TRY_AGAIN_LATER) {
  //     LOGV("no output buffer right now");
  //   } else {
  //     LOGV("unexpected info code: %zd", status);
  //   }
  // }

  if (!d->sawInputEOS || !d->sawOutputEOS) {
    video->mlooper->post(kMsgCodecBuffer, video);
  }
}

void mylooper::handle(int what, void *obj) {
  switch (what) {
  case kMsgCodecBuffer:
    doCodecWork((FlNativeVideo *)obj);
    break;

  case kMsgDecodeDone: {
    FlNativeVideo *video = (FlNativeVideo *)obj;
    // audio processing
    // workerdata* ad = &video->audiodata;
    // AMediaCodec_stop(ad->codec);
    // AMediaCodec_delete(ad->codec);
    // AMediaExtractor_delete(ad->ex);
    // ad->sawInputEOS = true;
    // ad->sawOutputEOS = true;

    workerdata *d = &video->data;
    AMediaCodec_stop(d->codec);
    AMediaCodec_delete(d->codec);
    AMediaExtractor_delete(d->ex);
    d->sawInputEOS = true;
    d->sawOutputEOS = true;

  } break;

  case kMsgSeek: {
    FlNativeVideo *video = (FlNativeVideo *)obj;
    workerdata *d = &video->data;
    AMediaExtractor_seekTo(d->ex, 0, AMEDIAEXTRACTOR_SEEK_NEXT_SYNC);
    AMediaCodec_flush(d->codec);
    d->renderstart = -1;
    d->sawInputEOS = false;
    d->sawOutputEOS = false;
    if (!d->isPlaying) {
      d->renderonce = true;
      post(kMsgCodecBuffer, video);
    }
    LOGV("seeked");
  } break;

  case kMsgPause: {
    FlNativeVideo *video = (FlNativeVideo *)obj;
    workerdata *d = &video->data;
    if (d->isPlaying) {
      // flush all outstanding codecbuffer messages with a no-op message
      d->isPlaying = false;

      // video->audiodata.isPlaying = false;

      post(kMsgPauseAck, NULL, true);
    }
  } break;

  case kMsgResume: {
    FlNativeVideo *video = (FlNativeVideo *)obj;
    workerdata *d = &video->data;
    if (!d->isPlaying) {
      d->renderstart = -1;
      d->isPlaying = true;
      // video->audiodata.isPlaying = false;
      // video->audiodata.renderstart = -1;
      post(kMsgCodecBuffer, video);
    }
  } break;
  }
}

extern "C" {

int ffi_createStreamingMediaPlayer(const char *filepath, FlNativeVideo *video) {
  LOGV("@@@ create");

  LOGV("opening %s", filepath);
  FILE *f = fopen(filepath, "r");
  if (f == NULL) {
    LOGE("failed to open file: %s (%s)", filepath, strerror(errno));
    return -1;
  }
  // int fd = open(filepath, O_RDONLY);
  int fd = fileno(f);

  long cur = ftell(f);
  fseek(f, 0, SEEK_END);
  long len = ftell(f);

  if (fd < 0) {
    LOGE("failed to open file: %s %d (%s)", filepath, fd, strerror(errno));
    return -1;
  }

  workerdata *data = &video->data;
  // workerdata *audiodata = &video->audiodata;

  data->fd = fd;
  // audiodata->fd = fd;

  workerdata *d = data;
  // workerdata *ad = audiodata;

  AMediaExtractor *ex = AMediaExtractor_new();
  media_status_t err = AMediaExtractor_setDataSourceFd(
      ex, d->fd, static_cast<off64_t>(cur), static_cast<off64_t>(len));
  // close(d->fd);
  fclose(f);
  if (err != AMEDIA_OK) {
    LOGV("setDataSource error: %d", err);
    return -2;
  }

  int numtracks = AMediaExtractor_getTrackCount(ex);

  AMediaCodec *codec = NULL;

  LOGV("input has %d tracks", numtracks);
  for (int i = 0; i < numtracks; i++) {
    AMediaFormat *format = AMediaExtractor_getTrackFormat(ex, i);
    const char *s = AMediaFormat_toString(format);
    LOGV("track %d format: %s", i, s);
    const char *mime;
    if (!AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
      LOGV("no mime type");
      return -3;
    } else if (!strncmp(mime, "video/", 6)) {
      // Omitting most error handling for clarity.
      // Production code should check for errors.
      AMediaExtractor_selectTrack(ex, i);
      codec = AMediaCodec_createDecoderByType(mime);
      AMediaCodec_configure(codec, format, d->window, NULL, 0);
      d->ex = ex;
      d->codec = codec;
      d->renderstart = -1;
      d->sawInputEOS = false;
      d->sawOutputEOS = false;
      d->isPlaying = false;
      d->renderonce = true;
      AMediaCodec_start(codec);
    }

    // Audio Processing Can't make it work
    // else if (!strncmp(mime, "audio/", 6)) {
    //   AMediaExtractor_selectTrack(ex, i);
    //   codec = AMediaCodec_createDecoderByType(mime);
    //   AMediaCodec_configure(codec, format, ad->window, NULL, 0);
    //   ad->ex = ex;
    //   ad->codec = codec;
    //   ad->renderstart = -1;
    //   ad->sawInputEOS = false;
    //   ad->sawOutputEOS = false;
    //   ad->isPlaying = false;
    //   ad->renderonce = true;
    //   AMediaCodec_start(codec);
    // }

    AMediaFormat_delete(format);
  }
  video->mlooper->post(kMsgCodecBuffer, video);

  return 1;
}
// set the playing state for the streaming media player
void ffi_NativeCodec_setPlayingStreamingMediaPlayer(bool isPlaying,
                                                    FlNativeVideo *video) {

  LOGV("@@@ playpause: %d", isPlaying);
  if (video->mlooper) {
    if (isPlaying) {
      video->mlooper->post(kMsgResume, video);
    } else {
      video->mlooper->post(kMsgPause, video);
    }
  }
}

// set the playing state for the streaming media player
void Java_com_example_flutter_1android_1video_1test_MainActivityKt_setPlayingStreamingMediaPlayer(
    JNIEnv *env, jclass clazz, jboolean isPlaying, jlong videoPointer) {
  LOGV("@@@ playpause: %d", isPlaying);
  FlNativeVideo *video = (FlNativeVideo *)videoPointer;
  mylooper *mlooper = video->mlooper;
  workerdata *data = &video->data;
  if (mlooper) {
    if (isPlaying) {
      mlooper->post(kMsgResume, data);
    } else {
      mlooper->post(kMsgPause, data);
    }
  }
}

// shut down the native media system
void Java_com_example_flutter_1android_1video_1test_MainActivityKt_shutdown(
    JNIEnv *env, jclass clazz, jlong videoPointer) {
  FlNativeVideo *video = (FlNativeVideo *)videoPointer;
  LOGV("@@@ shutdown");
  mylooper *mlooper = video->mlooper;
  workerdata *data = &video->data;
  if (mlooper) {
    mlooper->post(kMsgDecodeDone, video, true /* flush */);
  }
  if (data->window) {
    ANativeWindow_release(data->window);
    data->window = NULL;
  }
  delete video;
}

// set the surface
void Java_com_example_flutter_1android_1video_1test_MainActivityKt_setSurface(
    JNIEnv *env, jclass clazz, jobject surface, jlong flNativeVideoPointer) {

  FlNativeVideo *video = (FlNativeVideo *)flNativeVideoPointer;
  workerdata *data = &video->data;
  // obtain a native window from a Java surface
  if (data->window) {
    ANativeWindow_release(data->window);
    data->window = NULL;
  }
  data->window = ANativeWindow_fromSurface(env, surface);
  LOGV("@@@ setsurface %p", data->window);
}

jlong Java_com_example_flutter_1android_1video_1test_MainActivityKt_newFlNativeVideoFromfile(
    JNIEnv *env, jclass clazz, jstring filePath,
    jobject surface) {

  FlNativeVideo *video = new FlNativeVideo();

  Java_com_example_flutter_1android_1video_1test_MainActivityKt_setSurface(
      env, clazz, surface, (jlong)video);

  const char *utf8 = env->GetStringUTFChars(filePath, NULL);
  int result = ffi_createStreamingMediaPlayer(utf8, video);
  env->ReleaseStringUTFChars(filePath, utf8);
  if (result != 1) {
    workerdata *data = &video->data;
    ANativeWindow_release(data->window);
    data->window = NULL;
    delete video;
    return 0;
  }

  // ffi_NativeCodec_setPlayingStreamingMediaPlayer(true, video);
  return (jlong)video;
}
}
