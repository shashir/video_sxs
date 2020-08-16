#include <climits>
#include <iostream>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/strings/ascii.h>
#include <absl/strings/str_format.h>
#include <opencv2/opencv.hpp>

ABSL_FLAG(std::string, input1, "", "First video.");
ABSL_FLAG(std::string, input2, "", "Second video.");
ABSL_FLAG(int, input1_start_frame, 0, "Frame to start first video from.");
ABSL_FLAG(int, input2_start_frame, 0, "Frame to start second video from.");
ABSL_FLAG(bool, adapt_first, false,
          "Whether to change size/aspect ratio of the first video. If "
          "false, then only the second video will have it's size/aspect ratio "
          "adapted.");
ABSL_FLAG(std::string, output, "", "Output file.");
ABSL_FLAG(std::string, fourcc_codec, "h264",
          "FourCC codec identifier. Default is H.264 for MPEG-4.");

// Wraps OpenCV video capture with video information.
struct VideoCaptureWithInfo {
  std::string path;
  cv::VideoCapture capture;
  cv::Size size;
  std::string codec;
  double fps;
  int frames;

  VideoCaptureWithInfo(const std::string& path): path(path), capture(path) {
    if (!capture.isOpened()) {
      throw std::invalid_argument("Unable to open input file: " + path);
    }
    size = cv::Size(
        capture.get(cv::CAP_PROP_FRAME_WIDTH),
        capture.get(cv::CAP_PROP_FRAME_HEIGHT));
    int fourcc = capture.get(cv::CAP_PROP_FOURCC);
    codec = absl::StrFormat(
        "%c%c%c%c",
        fourcc & 0xff,
        (fourcc >> 8) & 0xff,
        (fourcc >> 16) & 0xff,
        (fourcc >> 24) & 0xff);
    frames = capture.get(cv::CAP_PROP_FRAME_COUNT);
    fps = capture.get(cv::CAP_PROP_FPS);
  }
};

std::ostream& operator<<(std::ostream& out, const VideoCaptureWithInfo& obj) {
  return out << absl::StrFormat((
      "\t\tPath:\t%s\n"
      "\t\tWidth:\t%d\n"
      "\t\tHeight:\t%d\n"
      "\t\tCodec:\t%s\n"
      "\t\tFPS:\t%f\n"
      "\t\tFrames:\t%d\n"
      "\t\tDuration:\t%f seconds"),
      obj.path, obj.size.width, obj.size.height,
      obj.codec, obj.fps, obj.frames, obj.frames / obj.fps);
}

// Resize video to desired size.
cv::Mat resize(const cv::Size& size, cv::Mat* video) {
  cv::Mat out;
  cv::resize(*video, out, size);
  return out;
}

int codec_to_int(const std::string& codec) {
  if (strlen(codec.c_str()) != 4) {
    throw std::invalid_argument("Unknown output codec: " + codec);
  }
  std::string codec_lowercase = absl::AsciiStrToLower(codec);
  unsigned char buffer[4] = {
      static_cast<unsigned char>(codec_lowercase.c_str()[0]),
      static_cast<unsigned char>(codec_lowercase.c_str()[1]),
      static_cast<unsigned char>(codec_lowercase.c_str()[2]),
      static_cast<unsigned char>(codec_lowercase.c_str()[3])};
  return int(
      (buffer[3]) << 24 | (buffer[2]) << 16 | (buffer[1]) << 8 | (buffer[0]));
}

// Check string flag exists and return value.
std::string check_string_flag_exists(const absl::Flag<std::string>& flag) {
  const std::string value = absl::GetFlag(flag);
  if (value.empty()) {
    throw std::invalid_argument(
        absl::StrFormat("Flag --%s must be non-empty.", flag.Name()));
  }
  return value;
}

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  VideoCaptureWithInfo capture1(check_string_flag_exists(FLAGS_input1));
  std::cout << "Input video 1:\n" << capture1 << std::endl;
  VideoCaptureWithInfo capture2(check_string_flag_exists(FLAGS_input2));
  std::cout << "Input video 2:\n" << capture2 << std::endl;
  const std::string output_path = check_string_flag_exists(FLAGS_output);
  const bool adapt_first = absl::GetFlag(FLAGS_adapt_first);

  const bool resize_required = capture1.size != capture2.size;
  // Note that height and width are transposed when dealing with matrix
  // dimensions.
  const cv::Size frame_matrix_dims(
      (adapt_first) ? capture2.size.width : capture1.size.width,
      (adapt_first) ? capture2.size.height : capture1.size.height);

  const int video1_start = absl::GetFlag(FLAGS_input1_start_frame);
  if (video1_start < 0 || video1_start >= capture1.frames) {
    throw std::invalid_argument(absl::StrFormat(
        "Video 1 start frame %d ought to be in range [0, %d)",
        video1_start, capture1.frames));
  }
  std::cout << "Starting video 1 from frame: " << video1_start << std::endl;
  capture1.capture.set(cv::CAP_PROP_POS_FRAMES, video1_start);

  const int video2_start = absl::GetFlag(FLAGS_input2_start_frame);
  if (video2_start < 0 || video2_start >= capture2.frames) {
    throw std::invalid_argument(absl::StrFormat(
        "Video 2 start frame %d ought to be in range [0, %d)",
        video2_start, capture2.frames));
  }
  std::cout << "Starting video 2 from frame: " << video2_start << std::endl;
  capture2.capture.set(cv::CAP_PROP_POS_FRAMES, video2_start);

  cv::VideoWriter output_writer(
      output_path,
      /*fo*/codec_to_int(absl::GetFlag(FLAGS_fourcc_codec)),
      /*fps=*/(adapt_first) ? capture2.fps : capture1.fps,
      frame_matrix_dims);
  if (!output_writer.isOpened()) {
    throw std::invalid_argument("Unable to open output file: " + output_path);
  }

  const int min_frames = std::min(
      capture1.frames - video1_start, capture2.frames - video2_start);
  int channels = 0;

  bool preview_video = true;
  std::string window_name = "video";
  cv::startWindowThread();
  cv::namedWindow(window_name, cv::WINDOW_NORMAL);
  std::cout << "Press [Escape] to close the video preview." << std::endl;

  for (int i = 0; i < min_frames; i++) {
    cv::Mat frame1, frame2;
    capture1.capture.read(frame1);
    capture2.capture.read(frame2);

    if (resize_required) {
      if (adapt_first) {
        frame1 = resize(frame_matrix_dims, &frame1);
      } else {
        frame2 = resize(frame_matrix_dims, &frame2);
      }
    }

    if (channels == 0) {
      channels = frame1.channels();
      std::cout << "Number of channels detected: " << channels << std::endl;
    }
    if (channels != frame2.channels()) {
      throw std::invalid_argument(absl::StrFormat(
          "Mismatched number of channels in videos 1 and 2: %d, %d",
          channels, frame2.channels()));
    }

    for (int row = 0; row < frame_matrix_dims.height; ++row) {
      const auto* frame1_row_ptr = frame1.ptr<unsigned char>(row);
      auto* frame2_row_ptr = frame2.ptr<unsigned char>(row);
      int col = 0;
      for (; col < frame_matrix_dims.width * channels / 2; ++col) {
        frame2_row_ptr[col] = frame1_row_ptr[col];
      }
      // Draw a vertical divider line.
      for (int mid = col; mid < col + channels; ++mid) {
        frame2_row_ptr[mid] = UCHAR_MAX;
      }
    }

    std::cout << "\rVideo Frame: " << i + 1 << "/" << min_frames;
    output_writer.write(frame2);
    if (preview_video) {
      cv::imshow(window_name, frame2);
      if (cv::waitKey(1) == 27) {
        std::cout << "\nClosing preview window." << std::endl;
        cv::destroyAllWindows();
        cv::waitKey(1);
        preview_video = false;
      }
    }
  }
  std::cout << "\nWrote output to: " << output_path << std::endl;
  output_writer.release();
  capture1.capture.release();
  capture2.capture.release();
  if (preview_video) {
    cv::destroyAllWindows();
  }
  return 0;
}