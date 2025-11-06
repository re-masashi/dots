#include <algorithm>
#include <pipewire/pipewire.h>
#include <spa/param/audio/format-utils.h>
#include <spa/param/props.h>
#include <aubio/aubio.h>
#include <memory>
#include <iostream>
#include <fstream>
#include <csignal>
#include <atomic>
#include <vector>
#include <cstring>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <thread>
#include <cmath>

class EnhancedBeatDetector {
private:
    static constexpr uint32_t SAMPLE_RATE = 44100;
    static constexpr uint32_t CHANNELS = 1;
    static constexpr float SILENCE_THRESHOLD = 0.01f;  // -40dB
    static constexpr float BPM_MIN = 60.0f;
    static constexpr float BPM_MAX = 200.0f;
    static constexpr float CONFIDENCE_THRESHOLD = 0.5f;
    static constexpr float BPM_VARIANCE_LIMIT = 5.0f;
    
    // PipeWire objects
    pw_main_loop* main_loop_;
    pw_context* context_;
    pw_core* core_;
    pw_stream* stream_;
    
    // Aubio objects
    std::unique_ptr<aubio_tempo_t, decltype(&del_aubio_tempo)> tempo_;
    std::unique_ptr<fvec_t, decltype(&del_fvec)> input_buffer_;
    std::unique_ptr<fvec_t, decltype(&del_fvec)> output_buffer_;
    
    // Additional aubio objects for enhanced features
    std::unique_ptr<aubio_onset_t, decltype(&del_aubio_onset)> onset_;
    std::unique_ptr<aubio_pitch_t, decltype(&del_aubio_pitch)> pitch_;
    std::unique_ptr<fvec_t, decltype(&del_fvec)> pitch_buffer_;
    
    const uint32_t buf_size_;
    const uint32_t fft_size_;
    
    static std::atomic<bool> should_quit_;
    static EnhancedBeatDetector* instance_;
    
    // Enhanced features
    std::ofstream log_file_;
    bool enable_logging_;
    bool enable_performance_stats_;
    bool enable_pitch_detection_;
    bool enable_visual_feedback_;
    
    // Performance tracking
    std::vector<double> process_times_;
    uint64_t total_beats_;
    uint64_t total_onsets_;
    std::chrono::steady_clock::time_point start_time_;
    
    // Beat analysis
    std::vector<float> recent_bpms_;
    std::vector<float> bpm_stability_;
    static constexpr size_t BPM_HISTORY_SIZE = 20;
    static constexpr size_t STABILITY_WINDOW = 5;
    float smoothed_bpm_;
    std::chrono::steady_clock::time_point last_beat_time_;
    
    // Circular buffer for variable PipeWire buffer sizes
    std::vector<float> sample_accumulator_;
    uint32_t accumulated_samples_;
    
    // Performance tracking
    uint64_t frame_count_;
    
    // Visual feedback
    std::string generate_beat_visual(float bpm, float confidence, bool is_beat) {
        if (!enable_visual_feedback_) return "";
        
        std::stringstream ss;
        if (is_beat) {
            int intensity = static_cast<int>(std::min(bpm / 20.0f, 10.0f));
            ss << "\r 🎵 ";
            for (int i = 0; i < intensity; ++i) ss << "█";
            for (int i = intensity; i < 10; ++i) ss << "░";
            ss << " BPM: " << std::fixed << std::setprecision(1) << bpm;
            ss << " | Conf: " << std::setprecision(2) << confidence;
            ss << " | Avg: " << get_average_bpm();
        }
        return ss.str();
    }

public:
    explicit EnhancedBeatDetector(uint32_t buf_size = 128, 
                                 bool enable_logging = true,
                                 bool enable_performance_stats = true,
                                 bool enable_pitch_detection = false,
                                 bool enable_visual_feedback = true) 
        : main_loop_(nullptr)
        , context_(nullptr)
        , core_(nullptr)
        , stream_(nullptr)
        , tempo_(nullptr, &del_aubio_tempo)
        , input_buffer_(nullptr, &del_fvec)
        , output_buffer_(nullptr, &del_fvec)
        , onset_(nullptr, &del_aubio_onset)
        , pitch_(nullptr, &del_aubio_pitch)
        , pitch_buffer_(nullptr, &del_fvec)
        , buf_size_(buf_size)
        , fft_size_(buf_size * 8)
        , enable_logging_(enable_logging)
        , enable_performance_stats_(enable_performance_stats)
        , enable_pitch_detection_(enable_pitch_detection)
        , enable_visual_feedback_(enable_visual_feedback)
        , total_beats_(0)
        , total_onsets_(0)
        , smoothed_bpm_(0.0f)
        , accumulated_samples_(0)
        , frame_count_(0)
    {
        instance_ = this;
        recent_bpms_.reserve(BPM_HISTORY_SIZE);
        bpm_stability_.reserve(STABILITY_WINDOW);
        sample_accumulator_.resize(buf_size_);
        if (enable_performance_stats_) {
            process_times_.reserve(1000);
        }
        initialize();
    }
    
    ~EnhancedBeatDetector() {
        print_final_stats();
        cleanup();
        instance_ = nullptr;
    }
    
    // Delete copy constructor and assignment operator
    EnhancedBeatDetector(const EnhancedBeatDetector&) = delete;
    EnhancedBeatDetector& operator=(const EnhancedBeatDetector&) = delete;
    
    bool initialize() {
        start_time_ = std::chrono::steady_clock::now();
        
        // Initialize logging
        if (enable_logging_) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            std::stringstream filename;
            filename << "beat_log_" << std::put_time(std::localtime(&time_t), "%Y%m%d_%H%M%S") << ".txt";
            log_file_.open(filename.str());
            if (log_file_.is_open()) {
                log_file_ << "# Beat Detection Log - " << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S") << "\n";
                log_file_ << "# Timestamp,BPM,Confidence,Pitch(Hz),Amplitude,Variance\n";
                std::cout << " Logging to: " << filename.str() << std::endl;
            }
        }
        
        // Initialize PipeWire
        pw_init(nullptr, nullptr);
        
        main_loop_ = pw_main_loop_new(nullptr);
        if (!main_loop_) {
            std::cerr << " Failed to create main loop" << std::endl;
            return false;
        }
        
        context_ = pw_context_new(pw_main_loop_get_loop(main_loop_), nullptr, 0);
        if (!context_) {
            std::cerr << " Failed to create context" << std::endl;
            return false;
        }
        
        core_ = pw_context_connect(context_, nullptr, 0);
        if (!core_) {
            std::cerr << " Failed to connect to PipeWire" << std::endl;
            return false;
        }
        
        // Initialize Aubio tempo detector with HFC method (better accuracy)
        tempo_.reset(new_aubio_tempo("hfc", fft_size_, buf_size_, SAMPLE_RATE));
        if (!tempo_) {
            std::cerr << " Failed to create aubio tempo detector" << std::endl;
            return false;
        }
        aubio_tempo_set_threshold(tempo_.get(), 0.2f);       // More sensitive
        aubio_tempo_set_delay_ms(tempo_.get(), 30.0);        // Faster response
        aubio_tempo_set_silence(tempo_.get(), -45.0);        // Catch quieter signals
        aubio_tempo_set_tatum_signature(tempo_.get(), 4);
        
        input_buffer_.reset(new_fvec(buf_size_));
        output_buffer_.reset(new_fvec(1));
        
        if (!input_buffer_ || !output_buffer_) {
            std::cerr << " Failed to create aubio buffers" << std::endl;
            return false;
        }
        
        // Initialize onset detection with HFC method
        onset_.reset(new_aubio_onset("hfc", fft_size_, buf_size_, SAMPLE_RATE));
        if (!onset_) {
            std::cerr << " Failed to create aubio onset detector" << std::endl;
            return false;
        }
        aubio_onset_set_threshold(onset_.get(), 0.2f);       // Onset sensitivity
        aubio_onset_set_minioi_ms(onset_.get(), 25.0);       // Min 25ms between beats
        aubio_onset_set_silence(onset_.get(), -45.0);        // Only process above -45dB
        
        // Initialize pitch detection if enabled
        if (enable_pitch_detection_) {
            pitch_.reset(new_aubio_pitch("default", fft_size_, buf_size_, SAMPLE_RATE));
            pitch_buffer_.reset(new_fvec(1));
            if (!pitch_ || !pitch_buffer_) {
                std::cerr << " Failed to create aubio pitch detector" << std::endl;
                return false;
            }
            aubio_pitch_set_unit(pitch_.get(), "Hz");
        }
        
        return setup_stream();
    }
    
    void run() {
        if (!main_loop_) return;
        
        print_startup_info();
        pw_main_loop_run(main_loop_);
    }
    
    void stop() {
        should_quit_ = true;
        if (main_loop_) {
            pw_main_loop_quit(main_loop_);
        }
    }
    
    static void signal_handler(int sig) {
        if (instance_) {
            std::cout << "\n Received signal " << sig << ", stopping gracefully..." << std::endl;
            instance_->stop();
        }
    }

private:
    void print_startup_info() {
        std::cout << "\n󰝚  Beat Detector Started!" << std::endl;
        std::cout << "   Buffer size: " << buf_size_ << " samples" << std::endl;
        std::cout << "   FFT size: " << fft_size_ << " samples" << std::endl;
        std::cout << "   Sample rate: " << SAMPLE_RATE << " Hz" << std::endl;
        std::cout << "   Detection method: HFC (Harmonic Flux Coefficient)" << std::endl;
        std::cout << "   Features enabled:" << std::endl;
        std::cout << "    Logging: " << (enable_logging_ ? "✓" : "✗") << std::endl;
        std::cout << "    Performance stats: " << (enable_performance_stats_ ? "✓" : "✗") << std::endl;
        std::cout << "    Pitch detection: " << (enable_pitch_detection_ ? "✓" : "✗") << std::endl;
        std::cout << "    Confidence gating: ✓" << std::endl;
        std::cout << "    BPM stability tracking: ✓" << std::endl;
        std::cout << "\n Listening for beats... Press Ctrl+C to stop.\n" << std::endl;
    }
    
    void print_final_stats() {
        if (!enable_performance_stats_) return;
        
        auto end_time = std::chrono::steady_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time_);
        
        std::cout << "\n  Final Statistics:" << std::endl;
        std::cout << "   󱎫  Total runtime: " << duration.count() << " seconds" << std::endl;
        std::cout << "    Total beats detected: " << total_beats_ << std::endl;
        std::cout << "    Total frames processed: " << frame_count_ << std::endl;
        
        if (frame_count_ > 0) {
            float beats_per_second = static_cast<float>(total_beats_) / duration.count();
            std::cout << "    Detection rate: " << std::fixed << std::setprecision(2) 
                      << beats_per_second << " beats/sec" << std::endl;
        }
        
        if (!process_times_.empty()) {
            double avg_time = 0;
            for (double t : process_times_) avg_time += t;
            avg_time /= process_times_.size();
            
            auto max_time = *std::max_element(process_times_.begin(), process_times_.end());
            auto min_time = *std::min_element(process_times_.begin(), process_times_.end());
            
            std::cout << "   ⚡ Average processing time: " << std::fixed << std::setprecision(3) 
                      << avg_time << " ms" << std::endl;
            std::cout << "   📈 Max processing time: " << max_time << " ms" << std::endl;
            std::cout << "   📉 Min processing time: " << min_time << " ms" << std::endl;
        }
        
        if (!recent_bpms_.empty()) {
            std::cout << "   󰝚 Final average BPM: " << std::fixed << std::setprecision(1) 
                      << get_average_bpm() << std::endl;
        }
    }
    
    float get_average_bpm() const {
        if (recent_bpms_.empty()) return 0.0f;
        float sum = 0;
        for (float bpm : recent_bpms_) sum += bpm;
        return sum / recent_bpms_.size();
    }
    
    float get_bpm_variance() const {
        if (bpm_stability_.empty()) return 999.0f;
        float mean = 0.0f;
        for (float bpm : bpm_stability_) mean += bpm;
        mean /= bpm_stability_.size();
        
        float variance = 0.0f;
        for (float bpm : bpm_stability_) {
            variance += (bpm - mean) * (bpm - mean);
        }
        return std::sqrt(variance / bpm_stability_.size());
    }
    
    bool setup_stream() {
        static const pw_stream_events stream_events = {
            .version = PW_VERSION_STREAM_EVENTS,
            .destroy = nullptr,
            .state_changed = on_state_changed,
            .control_info = nullptr,
            .io_changed = nullptr,
            .param_changed = nullptr,
            .add_buffer = nullptr,
            .remove_buffer = nullptr,
            .process = on_process,
            .drained = nullptr,
            .command = nullptr,
            .trigger_done = nullptr,
        };
        
        stream_ = pw_stream_new_simple(
            pw_main_loop_get_loop(main_loop_),
            "enhanced-beat-detector",
            pw_properties_new(
                PW_KEY_MEDIA_TYPE, "Audio",
                PW_KEY_MEDIA_CATEGORY, "Capture",
                PW_KEY_MEDIA_ROLE, "DSP",
                PW_KEY_STREAM_CAPTURE_SINK, "true",
                nullptr
            ),
            &stream_events,
            this
        );
        
        if (!stream_) {
            std::cerr << " Failed to create stream" << std::endl;
            return false;
        }
        
        uint8_t buffer[1024];
        spa_pod_builder pod_builder = SPA_POD_BUILDER_INIT(buffer, sizeof(buffer));
        
        struct spa_audio_info_raw audio_info = {};
        audio_info.format = SPA_AUDIO_FORMAT_F32_LE;
        audio_info.channels = CHANNELS;
        audio_info.rate = SAMPLE_RATE;
        audio_info.flags = 0;
        
        const spa_pod* params[1];
        params[0] = spa_format_audio_raw_build(&pod_builder, SPA_PARAM_EnumFormat, &audio_info);
        
        if (pw_stream_connect(stream_,
                             PW_DIRECTION_INPUT,
                             PW_ID_ANY,
                             static_cast<pw_stream_flags>(
                                 PW_STREAM_FLAG_AUTOCONNECT |
                                 PW_STREAM_FLAG_MAP_BUFFERS |
                                 PW_STREAM_FLAG_RT_PROCESS),
                             params, 1) < 0) {
            std::cerr << " Failed to connect stream" << std::endl;
            return false;
        }
        
        return true;
    }
    
    static void on_state_changed(void* userdata, enum pw_stream_state,
                                enum pw_stream_state state, const char* error) {
        auto* detector = static_cast<EnhancedBeatDetector*>(userdata);
        
        const char* state_emoji = "󰑓 ";
        switch (state) {
            case PW_STREAM_STATE_CONNECTING: state_emoji = "󰄙 "; break;
            case PW_STREAM_STATE_PAUSED: state_emoji = "⏸ "; break;
            case PW_STREAM_STATE_STREAMING: state_emoji = "󰝚 "; break;
            case PW_STREAM_STATE_ERROR: state_emoji = "⚠ "; break;
            case PW_STREAM_STATE_UNCONNECTED: state_emoji = "✗ "; break;
        }
        
        std::cout << state_emoji << " Stream state: " << pw_stream_state_as_string(state) << std::endl;
        
        if (state == PW_STREAM_STATE_ERROR) {
            std::cerr << " Stream error: " << (error ? error : "unknown") << std::endl;
            detector->stop();
        }
    }
    
    static void on_process(void* userdata) {
        auto* detector = static_cast<EnhancedBeatDetector*>(userdata);
        detector->process_audio();
    }
    
    void process_audio() {
        if (should_quit_) return;
        
        auto process_start = std::chrono::high_resolution_clock::now();
        
        pw_buffer* buffer = pw_stream_dequeue_buffer(stream_);
        if (!buffer) return;
        
        spa_buffer* spa_buf = buffer->buffer;
        if (!spa_buf->datas[0].data) {
            pw_stream_queue_buffer(stream_, buffer);
            return;
        }
        
        const float* audio_data = static_cast<const float*>(spa_buf->datas[0].data);
        const uint32_t n_samples = spa_buf->datas[0].chunk->size / sizeof(float);
        
        // Accumulate samples into circular buffer
        for (uint32_t i = 0; i < n_samples; ++i) {
            sample_accumulator_[accumulated_samples_++] = audio_data[i];
            
            if (accumulated_samples_ >= buf_size_) {
                std::memcpy(input_buffer_->data, sample_accumulator_.data(), buf_size_ * sizeof(float));
                
                // Check signal amplitude to gate silence
                float max_amplitude = 0.0f;
                float rms_energy = 0.0f;
                for (uint32_t j = 0; j < buf_size_; ++j) {
                    max_amplitude = std::max(max_amplitude, std::abs(input_buffer_->data[j]));
                    rms_energy += input_buffer_->data[j] * input_buffer_->data[j];
                }
                rms_energy = std::sqrt(rms_energy / buf_size_);
                
                frame_count_++;
                
                // Only process if above silence threshold
                if (max_amplitude < SILENCE_THRESHOLD) {
                    if (frame_count_ % 200 == 0) {
                        std::cout << " [SILENCE] Frame #" << frame_count_ 
                                  << " (amp: " << std::fixed << std::setprecision(4) << max_amplitude << ")" << std::endl;
                    }
                    accumulated_samples_ = 0;
                    continue;
                }
                
                // Adaptive threshold based on signal energy
                float adaptive_threshold = 0.15f + (0.15f * rms_energy);
                aubio_onset_set_threshold(onset_.get(), std::min(adaptive_threshold, 0.3f));
                
                // Process audio with aubio
                aubio_tempo_do(tempo_.get(), input_buffer_.get(), output_buffer_.get());
                float current_bpm = aubio_tempo_get_bpm(tempo_.get());
                float tempo_confidence = aubio_tempo_get_confidence(tempo_.get());
                
                // Use ONSET as primary beat source (more reliable)
                aubio_onset_do(onset_.get(), input_buffer_.get(), output_buffer_.get());
                bool is_onset = output_buffer_->data[0] != 0.0f;
                
                float pitch_hz = 0.0f;
                if (enable_pitch_detection_) {
                    aubio_pitch_do(pitch_.get(), input_buffer_.get(), pitch_buffer_.get());
                    pitch_hz = pitch_buffer_->data[0];
                }
                
                // BPM smoothing with validity checking
                if (current_bpm > BPM_MIN && current_bpm < BPM_MAX) {
                    smoothed_bpm_ = 0.7f * smoothed_bpm_ + 0.3f * current_bpm;
                } else if (smoothed_bpm_ == 0.0f) {
                    smoothed_bpm_ = current_bpm;
                }
                
                // Trust beat only if both onset detected AND tempo confident
                bool is_beat = is_onset && tempo_confidence > CONFIDENCE_THRESHOLD;
                
                // Debug output every 200 frames
                if (frame_count_ % 200 == 0) {
                    std::cout << " [DEBUG] Frame #" << frame_count_ 
                              << " | Amp: " << std::fixed << std::setprecision(4) << max_amplitude
                              << " | BPM: " << std::setprecision(1) << smoothed_bpm_
                              << " | Conf: " << std::setprecision(2) << tempo_confidence
                              << " | Beat: " << (is_beat ? "YES" : "NO") << std::endl;
                }
                
                // Beat detection
                if (is_beat) {
                    total_beats_++;
                    last_beat_time_ = std::chrono::steady_clock::now();
                    
                    recent_bpms_.push_back(smoothed_bpm_);
                    if (recent_bpms_.size() > BPM_HISTORY_SIZE) {
                        recent_bpms_.erase(recent_bpms_.begin());
                    }
                    
                    // Track BPM stability
                    bpm_stability_.push_back(smoothed_bpm_);
                    if (bpm_stability_.size() > STABILITY_WINDOW) {
                        bpm_stability_.erase(bpm_stability_.begin());
                    }
                    
                    float variance = get_bpm_variance();
                    bool is_stable = variance < BPM_VARIANCE_LIMIT;
                    
                    if (enable_visual_feedback_) {
                        std::cout << generate_beat_visual(smoothed_bpm_, tempo_confidence, true) << std::flush;
                    } else {
                        std::cout << " 🎵 BEAT! BPM: " << std::fixed << std::setprecision(1) 
                                  << smoothed_bpm_ << " | Conf: " << std::setprecision(2) 
                                  << tempo_confidence;
                        if (is_stable) {
                            std::cout << " | STABLE";
                        }
                        std::cout << std::endl;
                    }
                    
                    // Logging
                    if (enable_logging_ && log_file_.is_open()) {
                        auto now = std::chrono::system_clock::now();
                        auto time_t = std::chrono::system_clock::to_time_t(now);
                        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            now.time_since_epoch()) % 1000;
                        
                        log_file_ << std::put_time(std::localtime(&time_t), "%H:%M:%S") 
                                 << "." << std::setfill('0') << std::setw(3) << ms.count() << ","
                                 << std::fixed << std::setprecision(1) << smoothed_bpm_ << ","
                                 << std::setprecision(2) << tempo_confidence << ","
                                 << pitch_hz << ","
                                 << std::setprecision(4) << max_amplitude << ","
                                 << get_bpm_variance() << "\n";
                        log_file_.flush();
                    }
                }
                
                total_onsets_++;
                accumulated_samples_ = 0;
            }
        }
        
        pw_stream_queue_buffer(stream_, buffer);
        
        // Performance tracking
        if (enable_performance_stats_) {
            auto process_end = std::chrono::high_resolution_clock::now();
            auto process_time = std::chrono::duration<double, std::milli>(process_end - process_start).count();
            
            if (process_times_.size() < 1000) {
                process_times_.push_back(process_time);
            }
        }
    }
    
    void cleanup() {
        if (log_file_.is_open()) {
            log_file_.close();
        }
        
        if (stream_) {
            pw_stream_destroy(stream_);
            stream_ = nullptr;
        }
        
        if (core_) {
            pw_core_disconnect(core_);
            core_ = nullptr;
        }
        
        if (context_) {
            pw_context_destroy(context_);
            context_ = nullptr;
        }
        
        if (main_loop_) {
            pw_main_loop_destroy(main_loop_);
            main_loop_ = nullptr;
        }
        
        tempo_.reset();
        input_buffer_.reset();
        output_buffer_.reset();
        onset_.reset();
        pitch_.reset();
        pitch_buffer_.reset();
        
        pw_deinit();
        
        std::cout << "\n Cleanup complete - All resources freed!" << std::endl;
    }
};

// Static member definitions
std::atomic<bool> EnhancedBeatDetector::should_quit_{false};
EnhancedBeatDetector* EnhancedBeatDetector::instance_{nullptr};

void print_usage() {
    std::cout << " Beat Detector Usage:" << std::endl;
    std::cout << "  ./beat_detector [buffer_size] [options]" << std::endl;
    std::cout << "\nOptions:" << std::endl;
    std::cout << "  --no-log          Disable logging to file" << std::endl;
    std::cout << "  --no-stats        Disable performance statistics" << std::endl;
    std::cout << "  --pitch           Enable pitch detection" << std::endl;
    std::cout << "  --no-visual       Disable visual feedback" << std::endl;
    std::cout << "  --help            Show this help" << std::endl;
    std::cout << "\nExamples:" << std::endl;
    std::cout << "  ./beat_detector 128               # Small buffer for low latency" << std::endl;
    std::cout << "  ./beat_detector 256 --pitch       # Medium buffer with pitch detection" << std::endl;
    std::cout << "  ./beat_detector 512 --no-visual   # Large buffer, no visual feedback" << std::endl;
}

int main(int argc, char* argv[]) {
    uint32_t buffer_size = 128;
    bool enable_logging = true;
    bool enable_performance_stats = true;
    bool enable_pitch_detection = false;
    bool enable_visual_feedback = true;
    
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            print_usage();
            return 0;
        } else if (arg == "--no-log") {
            enable_logging = false;
        } else if (arg == "--no-stats") {
            enable_performance_stats = false;
        } else if (arg == "--pitch") {
            enable_pitch_detection = true;
        } else if (arg == "--no-visual") {
            enable_visual_feedback = false;
        } else if (arg[0] != '-') {
            try {
                buffer_size = std::stoul(arg);
                if (buffer_size < 64 || buffer_size > 8192) {
                    std::cerr << " Buffer size must be between 64 and 8192" << std::endl;
                    return 1;
                }
            } catch (...) {
                std::cerr << " Invalid buffer size: " << arg << std::endl;
                return 1;
            }
        } else {
            std::cerr << " Unknown option: " << arg << std::endl;
            print_usage();
            return 1;
        }
    }
    
    std::signal(SIGINT, EnhancedBeatDetector::signal_handler);
    std::signal(SIGTERM, EnhancedBeatDetector::signal_handler);
    
    try {
        EnhancedBeatDetector detector(buffer_size, enable_logging, enable_performance_stats,
                                     enable_pitch_detection, enable_visual_feedback);
        detector.run();
    } catch (const std::exception& e) {
        std::cerr << " Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

/*
 * Compilation command:
 * g++ -std=c++17 -O3 -Wall -Wextra -I/usr/include/pipewire-0.3 -I/usr/include/spa-0.2 -I/usr/include/aubio \
 *     -o beat_detector beat_detector.cpp -lpipewire-0.3 -laubio -pthread
 */
