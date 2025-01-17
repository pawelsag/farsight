#include <opencv2/aruco.hpp>
#include <opencv2/aruco/charuco.hpp>
#include <opencv2/calib3d/calib3d.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/core/mat.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/opencv.hpp>
#include <opencv2/photo.hpp>

#include "kinect_manager.hpp"

#include <stdio.h>

#include <optional>
#include <string>

#include <boost/program_options.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>

const cv::Size chessboard_size(6,9);
const cv::Size2f chessboard_real_size(0.08f, 0.08f);
const cv::Ptr<cv::aruco::Dictionary> dict =
  cv::aruco::getPredefinedDictionary(cv::aruco::DICT_5X5_250);
const cv::Ptr<cv::aruco::CharucoBoard> chboard =
  cv::aruco::CharucoBoard::create(6, 9, 0.035f, 0.021f, dict);
const cv::Size img_size(1080, 1920);

constexpr auto settings_filename = "charuco_settings.yaml";

static void
save_settings(cv::Mat &camera_matrix,
              cv::Mat &dist_coeffs,
              std::string_view filename)
{
  cv::FileStorage fs(filename.data(), cv::FileStorage::WRITE);

  if (!fs.isOpened())
  {
    fmt::print("Warning failed to save settings.\n");
    return;
  }

  fs << "camera_matrix" << camera_matrix;
  fs << "dist_coeffs" << dist_coeffs;
}

static void
load_settings(cv::Mat &camera_matrix, cv::Mat &dist_coeffs)
{
  cv::FileStorage fs(settings_filename, cv::FileStorage::READ);

  if (!fs.isOpened())
  {
    fmt::print("Warning failed to load settings.\n");
    return;
  }

  fs["camera_matrix"] >> camera_matrix;
  fs["dist_coeffs"] >> dist_coeffs;
}


void
depthProcess(libfreenect2::Frame *frame)
{
  auto total_size = frame->height * frame->width;
  auto fp = reinterpret_cast<float *>(frame->data);

  for (int i = 0; i < total_size; i++)
  {
    fp[i] /= 65535.0f;
  }
}

void
conv32FC1To8CU1(unsigned char *data, size_t size)
{
  auto fp = reinterpret_cast<float *>(data);

  for (auto i = 0; i < size; ++i, ++fp, ++data)
    *data = static_cast<unsigned char>(*fp * 255.0f);
}

enum class FrameGrabberType
{
  RGB,
  IR,
  FILE,
  UNKNOWN,
};

struct FrameGrabber
{
  FrameGrabber(FrameGrabberType type,
               kinect &kdev,
               std::vector<std::string> &&filenames = {})
    : type(type)
    , kdev(kdev)
    , filenames(std::move(filenames))
    , next_file(std::cbegin(this->filenames))
    , last_file(std::cend(this->filenames))
  {}

  std::optional<cv::Mat>
  grab(kinect &kdev)
  {
    cv::Mat image;

    switch (this->type)
    {
      case FrameGrabberType::RGB: {

        if (!kdev.waitForFrames(10))
          return std::nullopt;

        libfreenect2::Frame *rgb = kdev.frames[libfreenect2::Frame::Color];
        image = cv::Mat(rgb->height, rgb->width, CV_8UC4, rgb->data).clone();
        kdev.releaseFrames();
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);

        cv::flip(image, image, 1);
        break;
      }

      case FrameGrabberType::IR: {

        if (!kdev.waitForFrames(10))
          return std::nullopt;

        libfreenect2::Frame *ir = kdev.frames[libfreenect2::Frame::Ir];

        depthProcess(ir);
        conv32FC1To8CU1(ir->data, ir->height * ir->width);

        image = cv::Mat(ir->height, ir->width, CV_8UC1, ir->data).clone();
        kdev.releaseFrames();
        cv::cvtColor(image, image, cv::COLOR_BGRA2BGR);

        cv::flip(image, image, 1);
        return image;
        break;
      }

      case FrameGrabberType::FILE: {
        if (next_file != last_file)
        {
          image = cv::imread(*next_file);
          ++next_file;
        }
        else
        {
          return std::nullopt;
        }

        break;
      }

      case FrameGrabberType::UNKNOWN: {
        assert(false);
        return std::nullopt;
      }
    }

    if (this->type != FrameGrabberType::FILE) {
      cv::imshow("image", image);
      if (cv::waitKey(10) == 'q')
        return std::nullopt;
    }

    if (image.empty())
      return std::nullopt;

    return image;
  }

  const FrameGrabberType type = FrameGrabberType::UNKNOWN;
  const std::vector<std::string> filenames;
  std::vector<std::string>::const_iterator next_file, last_file;
  kinect &kdev;
};

static auto
chessboard_calib(kinect &kdev, FrameGrabber fg, std::string_view outfile)
{
  // Creating vector to store vectors of 3D points for each checkerboard
  // image
  std::vector<std::vector<cv::Point3f>> objpoints;

  // Creating vector to store vectors of 2D points for each checkerboard
  // image
  std::vector<std::vector<cv::Point2f>> imgpoints;

  // Defining the world coordinates for 3D points
  std::vector<cv::Point3f> objp;
  for (int i{ 0 }; i < chessboard_size.width; i++)
  {
    for (int j{ 0 }; j < chessboard_size.height; j++)
      objp.push_back(cv::Point3f(j * 0.08, i * 0.08, 0));
  }

  // vector to store the pixel coordinates of detected checker board
  // corners
  std::vector<cv::Point2f> corner_pts;
  bool success;

  cv::Mat camera_matrix, dist_coeffs, rvecs, tvecs;

  while (1)
  {
    std::optional opt_image = fg.grab(kdev);

    if (!opt_image)
      break;

    cv::Mat image = opt_image.value();

    // Finding checker board corners
    // If desired number of corners are found in the image then success =
    // true
    success = cv::findChessboardCorners(
      image,
      chessboard_size,
      corner_pts,
      cv::CALIB_CB_ADAPTIVE_THRESH | cv::CALIB_CB_NORMALIZE_IMAGE);

    /*
     * If desired number of corner are detected,
     * we refine the pixel coordinates and display
     * them on the images of checker board
     */
    if (success)
    {
      cv::TermCriteria criteria(
        cv::TermCriteria::COUNT + cv::TermCriteria::EPS, 30, 0.001);

      // refining pixel coordinates for given 2d points.
      //cv::cornerSubPix(
      //  image, corner_pts, cv::Size(11, 11), cv::Size(-1, -1), criteria);

      // Displaying the detected corner points on the checker board
      cv::drawChessboardCorners(image,
                                chessboard_size,
                                corner_pts,
                                success);

      objpoints.push_back(objp);
      imgpoints.push_back(corner_pts);
    }
  }

  if (objpoints.size() == 0)
    return;

  cv::calibrateCamera(objpoints,
                      imgpoints,
                      img_size,
                      camera_matrix,
                      dist_coeffs,
                      rvecs,
                      tvecs);

  save_settings(camera_matrix, dist_coeffs, outfile);
}

static auto
charuco_find_precalib(kinect &kdev, FrameGrabber fg)
{
  cv::Ptr<cv::aruco::DetectorParameters> params =
    cv::aruco::DetectorParameters::create();
  std::vector<std::vector<cv::Point2f>> all_charuco_corners;
  std::vector<std::vector<int>> all_charuco_ids;

  while (1)
  {
    std::optional opt_image = fg.grab(kdev);

    if (!opt_image)
      break;

    cv::Mat image = opt_image.value(), image_copy;

    image.copyTo(image_copy);
    std::vector<int> marker_ids;
    std::vector<std::vector<cv::Point2f>> marker_corners;
    cv::aruco::detectMarkers(
      image, chboard->dictionary, marker_corners, marker_ids, params);

    if (marker_ids.size() > 0)
    {
      cv::aruco::drawDetectedMarkers(
        image_copy, marker_corners, marker_ids);
      std::vector<cv::Point2f> charuco_corners;
      std::vector<int> charuco_ids;
      cv::aruco::interpolateCornersCharuco(marker_corners,
                                           marker_ids,
                                           image,
                                           chboard,
                                           charuco_corners,
                                           charuco_ids);

      if (charuco_ids.size() > 0)
      {
        cv::aruco::drawDetectedCornersCharuco(
          image_copy, charuco_corners, charuco_ids, cv::Scalar(255, 0, 0));
        all_charuco_ids.emplace_back(std::move(charuco_ids));
        all_charuco_corners.emplace_back(std::move(charuco_corners));
      }
    }

    kdev.releaseFrames();
  }

  return std::make_tuple(std::move(all_charuco_corners),
                         std::move(all_charuco_ids));
}

static int
calibrate_and_save(kinect &kdev, FrameGrabber &grabber, std::string_view outfile, bool is_simple)
{
  if (is_simple) {
    chessboard_calib(kdev, grabber, outfile);
    return 0;
  }

  auto [charuco_corners, charuco_ids] =
    charuco_find_precalib(kdev, grabber);

  cv::Mat camera_matrix, dist_coeffs;
  std::vector<cv::Mat> rvecs, tvecs;

  cv::aruco::calibrateCameraCharuco(charuco_corners,
                                    charuco_ids,
                                    chboard,
                                    img_size,
                                    camera_matrix,
                                    dist_coeffs,
                                    rvecs,
                                    tvecs,
                                    0);

  save_settings(camera_matrix, dist_coeffs, outfile);

  return 0;
}

static int
manual_calib(std::string color_type_str, std::string_view outfile, bool is_simple)
{
  FrameGrabberType image_type;

  std::for_each(std::begin(color_type_str),
                std::end(color_type_str),
                [](char &c) { c = std::toupper(c); });

  if (color_type_str == "" || color_type_str == "RGB")
  {
    image_type = FrameGrabberType::RGB;
  }
  else if (color_type_str == "IR")
  {
    image_type = FrameGrabberType::IR;
  }
  else
  {
    puts("Unknown image type. Try RGB or IR\n");
    return 1;
  }

  kinect kdev(0);
  FrameGrabber grabber(image_type, kdev);

  return calibrate_and_save(kdev, grabber, outfile, is_simple);
}

static int
auto_calib(std::vector<std::string> &&filenames, std::string_view outfile, bool is_simple)
{
  kinect kdev = kinect::nodev();
  FrameGrabber grabber(FrameGrabberType::FILE, kdev, std::move(filenames));

  return calibrate_and_save(kdev, grabber, outfile, is_simple);
}

namespace popt = boost::program_options;

static void
usage(const char *program_name)
{
  fmt::print("Charuco calibration util. Generate clibration params from "
             "images or video.\n"
             "If no file names passed, live video is choosen\n\n"
             "Usage: {} [file1[,file2[,file3...]]]\n",
             program_name);
}

int
main(int argc, char **argv)
{
  popt::options_description desc;

  // clang-format off
  desc.add_options()
    ("help", "Shows this message")

    ("format", popt::value<std::string>()->default_value("RGB"),
    "Image format: \"RGB\" or \"IR\". (used only live video used)")

    ("chessboard", "Enables simple, chessboard calibration.\n")

    ("outfile", popt::value<std::string>()->default_value(settings_filename),
    "Output filename.");
  // clang-format on

  popt::variables_map vm;
  popt::parsed_options parsed = popt::command_line_parser(argc, argv)
                                  .options(desc)
                                  .allow_unregistered()
                                  .run();
  popt::store(parsed, vm);
  popt::notify(vm);

  if (vm.count("help"))
  {
    usage(argv[0]);
    fmt::print("{}\n", desc);
    return 0;
  }

  auto format = vm["format"].as<std::string>();
  auto outfile = vm["outfile"].as<std::string>();

  std::vector<std::string> filenames =
    popt::collect_unrecognized(parsed.options, popt::include_positional);

  bool is_simple = vm.count("chessboard");

  if (filenames.size() == 0)
    return manual_calib(format, outfile, is_simple);
  else
    return auto_calib(std::move(filenames), outfile, is_simple);
}
