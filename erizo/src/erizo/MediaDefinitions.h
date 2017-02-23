/*
 * mediadefinitions.h
 */
#ifndef ERIZO_SRC_ERIZO_MEDIADEFINITIONS_H_
#define ERIZO_SRC_ERIZO_MEDIADEFINITIONS_H_

#include <boost/thread/mutex.hpp>
#include <vector>
#include <algorithm>

#include "lib/Clock.h"
#include "lib/ClockUtils.h"

namespace erizo {

class NiceConnection;

enum packetType {
    VIDEO_PACKET,
    AUDIO_PACKET,
    OTHER_PACKET
};

struct dataPacket {
  dataPacket() = default;

  dataPacket(int comp_, const char *data_, int length_, packetType type_, uint64_t received_time_ms_) :
    comp{comp_}, length{length_}, type{type_}, received_time_ms{received_time_ms_}, is_keyframe{false} {
      memcpy(data, data_, length_);
  }

  dataPacket(int comp_, const char *data_, int length_, packetType type_) :
    comp{comp_}, length{length_}, type{type_}, received_time_ms{ClockUtils::timePointToMs(clock::now())},
    is_keyframe{false} {
      memcpy(data, data_, length_);
  }

  dataPacket(int comp_, const unsigned char *data_, int length_) :
    comp{comp_}, length{length_}, type{VIDEO_PACKET}, received_time_ms{ClockUtils::timePointToMs(clock::now())},
    is_keyframe{false} {
      memcpy(data, data_, length_);
  }

  bool belongsToSpatialLayer(int spatial_layer_) {
    std::vector<int>::iterator item = std::find(compatible_spatial_layers.begin(),
                                              compatible_spatial_layers.end(),
                                              spatial_layer_);

    return item != compatible_spatial_layers.end();
  }

  bool belongsToTemporalLayer(int temporal_layer_) {
    std::vector<int>::iterator item = std::find(compatible_temporal_layers.begin(),
                                              compatible_temporal_layers.end(),
                                              temporal_layer_);

    return item != compatible_temporal_layers.end();
  }

  int comp;
  char data[1500];
  int length;
  packetType type;
  uint64_t received_time_ms;
  std::vector<int> compatible_spatial_layers;
  std::vector<int> compatible_temporal_layers;
  bool is_keyframe;  // Note: It can be just a keyframe first packet in VP8
};

class Monitor {
 protected:
    boost::mutex monitor_mutex_;
};

class FeedbackSink {
 public:
    virtual ~FeedbackSink() {}
    int deliverFeedback(std::shared_ptr<dataPacket> data_packet) {
        return this->deliverFeedback_(data_packet);
    }
 private:
    virtual int deliverFeedback_(std::shared_ptr<dataPacket> data_packet) = 0;
};


class FeedbackSource {
 protected:
    FeedbackSink* fb_sink_;
 public:
    FeedbackSource(): fb_sink_{nullptr} {}
    virtual ~FeedbackSource() {}
    void setFeedbackSink(FeedbackSink* sink) {
        fb_sink_ = sink;
    }
};

/*
 * A MediaSink
 */
class MediaSink: public virtual Monitor {
 protected:
    // SSRCs received by the SINK
    uint32_t audio_sink_ssrc_;
    uint32_t video_sink_ssrc_;
    // Is it able to provide Feedback
    FeedbackSource* sink_fb_source_;

 public:
    int deliverAudioData(std::shared_ptr<dataPacket> data_packet) {
        return this->deliverAudioData_(data_packet);
    }
    int deliverVideoData(std::shared_ptr<dataPacket> data_packet) {
        return this->deliverVideoData_(data_packet);
    }
    uint32_t getVideoSinkSSRC() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return video_sink_ssrc_;
    }
    void setVideoSinkSSRC(uint32_t ssrc) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        video_sink_ssrc_ = ssrc;
    }
    uint32_t getAudioSinkSSRC() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return audio_sink_ssrc_;
    }
    void setAudioSinkSSRC(uint32_t ssrc) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        audio_sink_ssrc_ = ssrc;
    }
    bool isVideoSinkSSRC(uint32_t ssrc) {
      return ssrc == video_sink_ssrc_;
    }
    bool isAudioSinkSSRC(uint32_t ssrc) {
      return ssrc == audio_sink_ssrc_;
    }
    FeedbackSource* getFeedbackSource() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return sink_fb_source_;
    }
    MediaSink() : audio_sink_ssrc_{0}, video_sink_ssrc_{0}, sink_fb_source_{nullptr} {}
    virtual ~MediaSink() {}

    virtual void close() = 0;

 private:
    virtual int deliverAudioData_(std::shared_ptr<dataPacket> data_packet) = 0;
    virtual int deliverVideoData_(std::shared_ptr<dataPacket> data_packet) = 0;
};

/**
 * A MediaSource is any class that produces audio or video data.
 */
class MediaSource: public virtual Monitor {
 protected:
    // SSRCs coming from the source
    uint32_t audio_source_ssrc_;
    std::vector<uint32_t> video_source_ssrc_list_;
    MediaSink* video_sink_;
    MediaSink* audio_sink_;
    // can it accept feedback
    FeedbackSink* source_fb_sink_;

 public:
    void setAudioSink(MediaSink* audio_sink) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        this->audio_sink_ = audio_sink;
    }
    void setVideoSink(MediaSink* video_sink) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        this->video_sink_ = video_sink;
    }

    FeedbackSink* getFeedbackSink() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return source_fb_sink_;
    }
    virtual int sendPLI() = 0;
    uint32_t getVideoSourceSSRC() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return video_source_ssrc_list_[0];
    }
    void setVideoSourceSSRC(uint32_t ssrc) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        video_source_ssrc_list_[0] = ssrc;
    }
    std::vector<uint32_t> getVideoSourceSSRCList() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return video_source_ssrc_list_;  //  return by copy to avoid concurrent access
    }
    void setVideoSourceSSRCList(const std::vector<uint32_t>& new_ssrc_list) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        video_source_ssrc_list_ = new_ssrc_list;
    }
    uint32_t getAudioSourceSSRC() {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        return audio_source_ssrc_;
    }
    void setAudioSourceSSRC(uint32_t ssrc) {
        boost::mutex::scoped_lock lock(monitor_mutex_);
        audio_source_ssrc_ = ssrc;
    }

    bool isVideoSourceSSRC(uint32_t ssrc) {
      auto found_ssrc = std::find_if(video_source_ssrc_list_.begin(), video_source_ssrc_list_.end(),
          [ssrc](uint32_t known_ssrc) {
          return known_ssrc == ssrc;
          });
      return (found_ssrc != video_source_ssrc_list_.end());
    }

    bool isAudioSourceSSRC(uint32_t ssrc) {
      return audio_source_ssrc_ == ssrc;
    }

    MediaSource() : audio_source_ssrc_{0}, video_source_ssrc_list_{std::vector<uint32_t>(1, 0)},
      video_sink_{nullptr}, audio_sink_{nullptr}, source_fb_sink_{nullptr} {}
    virtual ~MediaSource() {}

    virtual void close() = 0;
};

}  // namespace erizo

#endif  // ERIZO_SRC_ERIZO_MEDIADEFINITIONS_H_
