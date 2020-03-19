#include "image_proc.hpp"
#include <fmt/format.h>

detector::detector()
{

  cv::SimpleBlobDetector::Params params;
  params.filterByArea = true;
  params.minArea = 5;
  params.maxArea = std::numeric_limits<float>::infinity();

  det = cv::SimpleBlobDetector::create(params);
  configScreen =cv::Mat::zeros(cv::Size(depth_width*2 +10, depth_height*2+10), CV_8UC1);

}
detector::bbox detector::detect(byte* frame_base, byte *frame_object, size_t size, cv::Mat &image_depth)
{
  int lowerb = 0, higherb = 255;
  int lowerb2 = 20, higherb2 = 240;
  bool clr = false;

  diff(frame_object, frame_base, size);
  auto image_depth_ =
    cv::Mat(depth_height, depth_width, CV_8UC1, frame_object);
  cv::Mat image_depth_th, image_depth_filtered;
  cv::threshold(image_depth_, image_depth_th, lowerb, higherb, cv::THRESH_BINARY_INV);
  cv::inRange(image_depth_, lowerb2, higherb2, image_depth_filtered);
  
  cv::Mat labels, stats, centroids;
  cv::connectedComponentsWithStats(image_depth_filtered, labels, stats, centroids);
  
  clr = !clr;
  bbox best_bbox;

  for (int i = 0; i < stats.rows; ++i)
  {
    int x = stats.at<int>({0,i});
    int y = stats.at<int>({1,i});
    int w = stats.at<int>({2,i});
    int h = stats.at<int>({3,i});
    int area = stats.at<int>({4,i});
  
    if (area >= depth_width * depth_height / 2)
      continue;
  
    if (area > best_bbox.area) {
      best_bbox.area = area;
      best_bbox.x = x;
      best_bbox.y = y;
      best_bbox.w = w;
      best_bbox.h = h;
    }
  }
  
  fmt::print("Area: {}\n", best_bbox.area);
  cv::Scalar color(clr ? 255 : 0, 0, 0);
  cv::Rect rect(best_bbox.x,best_bbox.y,best_bbox.w,best_bbox.h);
  cv::rectangle(image_depth, rect, color, 3);
  return best_bbox;
}

void detector::configure(int kinectID, cv::Mat &img, bbox &sizes)
{
    img.copyTo(sceneConfiguration[kinectID].img_object);
    sceneConfiguration[kinectID].area = sizes;
    sceneConfiguration[kinectID].imObjectSet= true;
}

void detector::setBaseImg(int kinectID, cv::Mat &img)
{
    img.copyTo(sceneConfiguration[kinectID].img_base);
    sceneConfiguration[kinectID].imBaseSet= true;
}

void detector::displayCurrectConfig()
{
    const auto & c1 = sceneConfiguration[0];
    const auto & c2 = sceneConfiguration[1];
    cv::Mat temp; 

    if(c1.imBaseSet == true)
    {
        matRoi =cv::Rect(0, 0, depth_width, depth_height);
        resize(c1.img_base, temp, cv::Size(depth_width, depth_height));
        temp.copyTo(configScreen(matRoi));
    }
    
    if(c1.imObjectSet== true)
    {
        matRoi =cv::Rect(depth_width, 0, depth_width,depth_height);
        resize(c1.img_object, temp, cv::Size(depth_width,depth_height));
        temp.copyTo(configScreen(matRoi));
    }

    if(c2.imBaseSet == true)
    {
        matRoi =cv::Rect(0, depth_height, depth_width, depth_height);
        resize(c2.img_base, temp, cv::Size(depth_width, depth_height));
        temp.copyTo(configScreen(matRoi));
    }
    
    if(c2.imObjectSet== true)
    {
        matRoi =cv::Rect(depth_width, depth_height, depth_width,depth_height);
        resize(c2.img_object, temp, cv::Size(depth_width,depth_height));
        temp.copyTo(configScreen(matRoi));
    }
    cv::imshow("Dupa", configScreen); 
}

void detector::presentResults()
{

}
