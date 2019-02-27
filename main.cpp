#include <torch/torch.h>
#include <opencv2/opencv.hpp>

#include "Darknet.h"

using namespace std;
using namespace std::chrono;

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        std::cerr << "usage: yolo-app <image path>\n";
        return -1;
    }

    torch::DeviceType device_type;

    if (torch::cuda::is_available()) {
        device_type = torch::kCUDA;
    } else {
        device_type = torch::kCPU;
    }
    torch::Device device(device_type);


    Darknet net("models/yolov3.cfg", &device);

    map<string, string> *info = net.get_net_info();
    int input_image_size = std::atoi(info->operator[]("height").c_str());

    std::cout << "loading weight ..." << endl;
    net.load_weights("weights/yolov3.weights");
    std::cout << "weight loaded ..." << endl;

    net.to(device);

    torch::NoGradGuard no_grad;
    net.eval();

    std::cout << "start to inference ..." << endl;

    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
//        std::cerr << "usage: yolo-app <image path>\n";
        return -2;
    }

    cv::namedWindow("out");
    cv::Mat origin_image;
    while (cap.read(origin_image)) {
        cv::Mat resized_image;

        cv::cvtColor(origin_image, resized_image, cv::COLOR_RGB2BGR);
        cv::resize(resized_image, resized_image, cv::Size(input_image_size, input_image_size));

        cv::Mat img_float;
        resized_image.convertTo(img_float, CV_32F, 1.0 / 255);

        auto img_tensor = torch::CPU(torch::kFloat32).tensorFromBlob(img_float.data,
                                                                     {1, input_image_size, input_image_size, 3});
        img_tensor = img_tensor.permute({0, 3, 1, 2});
        auto img_var = torch::autograd::make_variable(img_tensor, false).to(device);

        auto start = std::chrono::high_resolution_clock::now();

        auto output = net.forward(img_var);

        auto result = write_results(output, 0.1, 0.4);

        auto end = std::chrono::high_resolution_clock::now();

        auto duration = duration_cast<milliseconds>(end - start);

        // It should be known that it takes longer time at first time
        std::cout << "inference taken : " << duration.count() << " ms" << endl;

        auto bbox = std::get<0>(result);
        auto classes = std::get<1>(result);
        auto scores = std::get<2>(result);

        if (bbox.size(0) == 0) {
            std::cout << "no object found" << endl;
        } else {
            auto obj_num = bbox.size(0);

            std::cout << obj_num << " objects found" << endl;

            float w_scale = float(origin_image.cols) / input_image_size;
            float h_scale = float(origin_image.rows) / input_image_size;

            bbox.select(1, 0).mul_(w_scale);
            bbox.select(1, 1).mul_(h_scale);
            bbox.select(1, 2).mul_(w_scale);
            bbox.select(1, 3).mul_(h_scale);

            auto result_data = bbox.accessor<float, 2>();

            for (int i = 0; i < bbox.size(0); i++) {
                cv::rectangle(origin_image, cv::Point(result_data[i][0], result_data[i][1]),
                              cv::Point(result_data[i][2], result_data[i][3]), cv::Scalar(0, 0, 255), 1, 1, 0);
            }
        }
        cv::imshow("out", origin_image);

        if (cv::waitKey(1) != -1) break;
    }

    std::cout << "Done" << endl;

    return 0;
}