#include <opencv2/core/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/core/mat.hpp>

#include <stdio.h>
#include <memory>

#include <cmath>

#include <fmt/format.h>

const char *wndname = "wnd";
const char *wndname2 = "wnd2";
const char *wndname3 = "wnd3";

using byte = unsigned char;

void gamma(float *data, size_t size, float gamma)
{
  for (auto i = 0; i < size; ++i)
    data[i] = std::pow(data[i], 1.0f / gamma);
}

void filter_bad(float *data, size_t size)
{
  for (auto i = 0; i < size; ++i)
    if (std::isnan(data[i]) || std::isinf(data[i]) || data[i] <= 0.0f)
      data[i] = 255.0f;
}

constexpr size_t cubeface_count = 6;
std::array<cv::Mat, cubeface_count> cubefaces;

bool loadfaces()
{
  cubefaces[0] = cv::imread("media/cube_photo0.jpg", cv::IMREAD_COLOR);
  cubefaces[1] = cv::imread("media/cube_photo1.jpg", cv::IMREAD_COLOR);
  cubefaces[2] = cv::imread("media/cube_photo2.jpg", cv::IMREAD_COLOR);
  cubefaces[3] = cv::imread("media/cube_photo3.jpg", cv::IMREAD_COLOR);
  cubefaces[4] = cv::imread("media/cube_photo4.jpg", cv::IMREAD_COLOR);
  cubefaces[5] = cv::imread("media/cube_photo5.jpg", cv::IMREAD_COLOR);

  for (auto i = 0; i < cubeface_count; ++i)
    if(!cubefaces[i].data)
      return false;

  return true;
}

void removeAlpha(float *data, size_t size)
{
  assert(size % 4 == 0);

  for (auto current = 0, insert = 0; current < size; current+=4, insert+=3) {
    data[insert+0] = data[current+0];
    data[insert+1] = data[current+1];
    data[insert+2] = data[current+2];
  }
}

void conv8UC4To32FC4(byte *data, size_t size)
{
  auto rgbend = data + size;
  auto rgbp = rgbend - (size/sizeof(float));

  for (auto rgbfp = reinterpret_cast<float*>(data);
       rgbp != rgbend;
       ++rgbp, ++rgbfp) {
    *rgbfp = static_cast<float>(*rgbp)/255.0f;
  }

}

int main()
{
  cv::namedWindow(wndname, cv::WINDOW_AUTOSIZE);
  cv::namedWindow(wndname2, cv::WINDOW_AUTOSIZE);
  cv::namedWindow(wndname3, cv::WINDOW_AUTOSIZE);

  cv::Mat tmatch;

  size_t depth_width = 512, depth_height = 424;
  size_t fsize_depth = depth_width * depth_height;
  size_t total_size_depth = fsize_depth * sizeof(float);

  size_t rgb_width = 1920, rgb_height = 1080;
  size_t fsize_rgb = rgb_width * rgb_height;
  size_t total_size_rgb = fsize_rgb * sizeof(float) * 4;

  auto imgraw_depth_ = std::make_unique<byte[]>(total_size_depth);
  auto imgraw_depth  = std::make_unique<byte[]>(total_size_depth);

  auto imgraw_rgb_ = std::make_unique<byte[]>(total_size_rgb);
  auto imgraw_rgb  = std::make_unique<byte[]>(total_size_rgb);

  auto imgf_depth = reinterpret_cast<float*>(imgraw_depth.get());
  auto imgf_rgb = reinterpret_cast<float*>(imgraw_rgb.get());

  auto f = fopen("./depth.raw", "rb");
  if (fread(imgraw_depth_.get(), 4, depth_width*depth_height, f) != depth_width*depth_height)
    return -1;
  fclose(f);

  auto rgbend = imgraw_rgb_.get() + total_size_rgb;
  auto rgbp = rgbend - (total_size_rgb/sizeof(float));

  f = fopen("./rgb.raw", "rb");
  if (fread(rgbp, 4, rgb_width*rgb_height, f) != rgb_width*rgb_height)
    return -2;
  fclose(f);

  conv8UC4To32FC4(imgraw_rgb_.get(), total_size_rgb);
  removeAlpha(reinterpret_cast<float*>(imgraw_rgb_.get()), rgb_width * rgb_height * sizeof(float));

  if (!loadfaces())
    return -3;

  int c = 0;

  float g = 0.5f;
  while (c != 'q') {

    if (c == '+')
      g += 0.015625f;
    else
      g -= 0.015625f;

    std::copy(imgraw_depth_.get(), imgraw_depth_.get()+total_size_depth, imgraw_depth.get());
    std::copy(imgraw_rgb_.get(), imgraw_rgb_.get()+total_size_rgb, imgraw_rgb.get());

    filter_bad(imgf_depth, fsize_depth);
    gamma(imgf_depth, fsize_depth, g);

    auto image_depth = cv::Mat(depth_height, depth_width, CV_32FC1, imgf_depth);
    auto image_rgb = cv::Mat(rgb_height, rgb_width, CV_32FC3, imgf_rgb);
    auto image_th = cv::Mat(depth_height, depth_width, CV_32FC1);

    for (auto &face : cubefaces) {
    //auto &face = cubefaces[0]; {
      constexpr auto match_method = cv::TM_CCOEFF;

      auto result_cols = image_rgb.cols - face.cols + 1;
      auto result_rows = image_rgb.rows - face.rows + 1;

      tmatch.create(result_rows, result_cols, CV_32FC3);

      cv::Mat tmp;
      face.convertTo(tmp, CV_32FC3);
      matchTemplate(image_rgb, tmp, tmatch, match_method);
      normalize(tmatch, tmatch, 0, 1, cv::NORM_MINMAX, -1, cv::Mat());

      double minVal, maxVal;
      cv::Point minLoc, maxLoc, matchLoc;

      minMaxLoc(tmatch, &minVal, &maxVal, &minLoc, &maxLoc, cv::Mat());

      if constexpr (match_method == cv::TM_SQDIFF || match_method == cv::TM_CCORR)
        matchLoc = minLoc;
      else
        matchLoc = maxLoc;

      cv::rectangle(image_rgb, matchLoc, cv::Point(matchLoc.x + face.cols, matchLoc.y + face.rows), cv::Scalar::all(0), 2, 8, 0);

      cv::imshow(wndname3, image_rgb);
      cv::waitKey(20);
    }

    cv::threshold(image_depth, image_th, 0.25f, 255.0f, cv::THRESH_BINARY);

    cv::imshow(wndname, image_depth);
    cv::imshow(wndname2, image_th);
    cv::imshow(wndname3, image_rgb);

    cv::waitKey(20);
    cv::waitKey(20);
    if (auto ct = getchar(); ct != '\n')
      c = ct;
  }
}