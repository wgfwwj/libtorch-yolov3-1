#include <experimental/filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "TargetStorage.h"
#include "config.h"

using namespace std;
namespace fs = std::experimental::filesystem;

TargetStorage::TargetStorage(const array<int64_t, 2> &orig_dim, int video_FPS) {
    fs::create_directories(OUTPUT_DIR);
    writer.open((fs::path(OUTPUT_DIR) / VIDEO_NAME).string(),
                cv::VideoWriter::fourcc('a', 'v', 'c', '1'),
                video_FPS, cv::Size(orig_dim[1], orig_dim[0]));
    if (!writer.isOpened()) {
        cerr << "Cannot open cv::VideoWriter" << endl;
        exit(-3);
    }
}

void TargetStorage::update(const vector<Track> &trks, int frame, const cv::Mat &image) {
    for (auto[id, box]:trks) {
        // save normalized boxes
        box = normalize_rect(box, image.cols, image.rows);

        auto &t = targets[id];
        t.trajectories.emplace(frame, box);
        if ((frame - t.last_snap) > 5) {
            t.snapshots[frame] = image(unnormalize_rect(pad_rect(box, padding), image.cols, image.rows)).clone();
            t.last_snap = frame;
        }
    }

    record();

    writer.write(image);
}

void TargetStorage::record() {
    for (auto&[id, t]:targets) {
        auto dir_path = fs::path(OUTPUT_DIR) / TARGETS_DIR_NAME / to_string(id);
        fs::create_directories(dir_path);

        ofstream fp(dir_path / TRAJ_TXT_NAME, ios::app);
        fp << right << fixed << setprecision(3);
        for (auto &[frame, box]:t.trajectories) {
            fp << setw(6) << frame
               << setw(6) << box.x
               << setw(6) << box.y
               << setw(6) << box.width
               << setw(6) << box.height
               << setw(6) << endl;
        }
        t.trajectories.clear();

        dir_path /= SNAPSHOTS_DIR_NAME;
        fs::create_directories(dir_path);
        for (auto &[frame, ss]:t.snapshots) {
            cv::imwrite((dir_path / (to_string(frame) + ".jpg")).string(), ss);
        }
        t.snapshots.clear();
    }
}
