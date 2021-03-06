#ifndef DETECTOR_H
#define DETECTOR_H

#include <memory>
#include <array>
#include <opencv2/opencv.hpp>

class Detector {
public:
    explicit Detector(const std::array<int64_t, 2> &_inp_dim);

    ~Detector();

    std::vector<cv::Rect2f> detect(cv::Mat image);

private:
    class Darknet;

    std::unique_ptr<Darknet> net;

    std::array<int64_t, 2> inp_dim;
    static const float NMS_threshold;
    static const float confidence_threshold;
};


#endif //DETECTOR_H
