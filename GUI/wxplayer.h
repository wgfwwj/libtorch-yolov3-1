#ifndef WXPLAYER_H
#define WXPLAYER_H

#include <wx/wxprec.h>

#ifndef WX_PRECOMP

#include <wx/wx.h>

#endif

#include <opencv2/opencv.hpp>


class wxPlayer : public wxWindow {
public:
    explicit wxPlayer(wxWindow *parent, wxWindowID id,
                      const wxString &file,
                      const std::function<cv::Mat(cv::Mat, int)> &post);

    int GetFrame() { return capture.get(cv::CAP_PROP_POS_FRAMES) - 1; }

    int GetFPS() { return capture.get(cv::CAP_PROP_FPS); }

    bool isPlaying() { return timer->IsRunning(); }

    void Seek(int frame);

    void Refresh() {
        Seek(GetFrame());
    }

private:
    void LoadNext();

    wxStaticBitmap *bitmap = nullptr;
    wxSlider *progress = nullptr;
    wxTimer *timer = nullptr;

    cv::VideoCapture capture;

    std::function<cv::Mat(cv::Mat, int)> post;

    enum {
        ID_Timer = 1,
        ID_Start_Pause,
        ID_Reload,
        ID_Progress
    };
};

#endif //WXPLAYER_H
