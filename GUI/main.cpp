#include <algorithm>
#include <chrono>
#include <opencv2/opencv.hpp>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_opengl3.h"

#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)

#include <GL/gl3w.h>    // Initialize with gl3wInit()

#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
#include <GL/glew.h>    // Initialize with glewInit()
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
#include <glad/glad.h>  // Initialize with gladLoadGL()
#else
#include IMGUI_IMPL_OPENGL_LOADER_CUSTOM
#endif

#include <GLFW/glfw3.h>

#include "util.h"
#include "Target.h"

using namespace std;

static void glfw_error_callback(int error, const char *description) {
    cerr << "Glfw Error" << error << ": " << description << endl;
}

static GLFWwindow *setup_UI() {
    // Setup window
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return nullptr;

    // Decide GL+GLSL versions
#if __APPLE__
    // GL 3.2 + GLSL 150
    const char* glsl_version = "#version 150";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    //glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    //glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // 3.0+ only
#endif

    // Create window with graphics context
    GLFWwindow *window = glfwCreateWindow(1280, 720, "Dear ImGui GLFW+OpenGL3 example", NULL, NULL);
    if (!window)
        return nullptr;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    // Initialize OpenGL loader
#if defined(IMGUI_IMPL_OPENGL_LOADER_GL3W)
    bool err = gl3wInit() != 0;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLEW)
    bool err = glewInit() != GLEW_OK;
#elif defined(IMGUI_IMPL_OPENGL_LOADER_GLAD)
    bool err = gladLoadGL() == 0;
#else
    bool err = false; // If you use IMGUI_IMPL_OPENGL_LOADER_CUSTOM, your loader is likely to requires some form of initialization.
#endif
    if (err) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return nullptr;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();

    // Setup Platform/Renderer bindings
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    return window;
}

static void draw_res_window(const cv::Mat &image, TargetRepo &repo,
                            uint32_t display_frame, int hovered, GLuint tex, bool *p_open = __null) {
    auto size = image_window("Result", tex, p_open);
    cv::Mat ret_image;
    cv::resize(image, ret_image, {size[0], size[1]});
    for (size_t i = 0; i < repo.size(); ++i) {
        auto &t = repo[i];
        if (t.trajectories.count(display_frame)) {
            auto color = i == hovered ? cv::Scalar(0, 0, 255) : cv::Scalar(0, 0, 0);
            draw_trajectories(ret_image, t.trajectories, size[0], size[1], color);
            draw_bbox(ret_image, unnormalize_rect(t.trajectories.at(display_frame), size[0], size[1]),
                      to_string(i), color);
        }
    }
    mat_to_texture(ret_image, tex);
}

static auto draw_target_window(TargetRepo &repo, int FPS, bool *p_open = __null) {
    int hovered = -1;
    int rewind = -1;

    vector<TargetRepo::size_type> to_del;
    vector<std::array<size_t, 2>> to_merge;

    static int hovered_prev = -1;
    static chrono::steady_clock::time_point hovered_start;

    ImGui::Begin("Targets", p_open);
    auto &style = ImGui::GetStyle();
    auto window_visible_x2 = ImGui::GetWindowPos().x + ImGui::GetWindowContentRegionMax().x;
    for (size_t i = 0; i < repo.size(); ++i) {
        auto &t = repo[i];
        ImGui::PushID(i);
        {
            ImGui::BeginGroup();
            {
                auto it = t.snapshots.begin();
                if (hovered_prev == i) {
                    auto interval = 5 * 1000 / FPS;
                    for (auto duration = chrono::duration_cast<chrono::milliseconds>(
                            chrono::steady_clock::now() - hovered_start).count();
                         duration > interval; duration -= interval) {
                        // repeatly play snapshots
                        if (++it == t.snapshots.end()) {
                            it = t.snapshots.begin();
                        }
                    }
                }
                ImGui::Image(reinterpret_cast<ImTextureID>(it->second.tex), {50, 50});
            }
            ImGui::SameLine();
            {
                ImGui::BeginGroup();
                ImGui::Text("Id: %d", i);
                ImGui::Text("Duration: %3d, %3d", t.trajectories.begin()->first, t.trajectories.rbegin()->first);
                if (ImGui::Button("Delete")) {
                    to_del.push_back(i);
                }
                ImGui::SameLine();
                if (ImGui::Button("Rewind")) {
                    rewind = t.trajectories.begin()->first;
                }
                ImGui::EndGroup();
            }
            ImGui::EndGroup();
        }
        if (ImGui::IsItemHovered()) {
            hovered = i;
        }

        if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
            ImGui::SetDragDropPayload("TARGET_DRAG", &i, sizeof(i));
            ImGui::Image(reinterpret_cast<ImTextureID>(t.snapshots.begin()->second.tex), {50, 50});
            ImGui::EndDragDropSource();
        }
        if (ImGui::BeginDragDropTarget()) {
            if (const auto payload = ImGui::AcceptDragDropPayload("TARGET_DRAG")) {
                auto drop_i = *(const size_t *) payload->Data;
                to_merge.push_back({i, drop_i});
            }
            ImGui::EndDragDropTarget();
        }
        ImGui::PopID();

        auto last_x2 = ImGui::GetItemRectMax().x;
        auto next_x2 = last_x2 + style.ItemSpacing.x
                       + ImGui::GetItemRectSize().x; // Expected position if next button was on same line
        if (i != repo.size() - 1 && next_x2 < window_visible_x2)
            ImGui::SameLine();
    }
    ImGui::End();

    for (auto i:to_del) {
        repo.erase(i);
    }
    for (auto[to, from]:to_merge) {
        repo.merge(to, from);
    }

    if (hovered != hovered_prev) {
        hovered_prev = hovered;
        hovered_start = chrono::steady_clock::now();
    }

    return make_pair(hovered, rewind);
}

int main(int argc, const char *argv[]) {
    if (argc != 2) {
        cerr << "usage: yolo-app <image path>" << endl;
        return -1;
    }

    cv::VideoCapture cap(argv[1]);
    if (!cap.isOpened()) {
        cerr << "Cannot open the video" << endl;
        return -2;
    }

    auto video_FPS = static_cast<int>(cap.get(cv::CAP_PROP_FPS));

    array<int64_t, 2> orig_dim{cap.get(cv::CAP_PROP_FRAME_HEIGHT), cap.get(cv::CAP_PROP_FRAME_WIDTH)};

    TargetRepo repo;

    auto image = cv::Mat(orig_dim[0], orig_dim[1], CV_8UC3, {0, 0, 0});

    auto window = setup_UI();
    if (!window) {
        cerr << "GUI failed" << endl;
        return -1;
    }

    array<GLuint, 3> texture;
    glGenTextures(texture.size(), texture.data());

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        static auto show_demo_window = false;
        static auto show_res_window = true;
        static auto show_target_window = true;
        static auto playing = false;

        static uint32_t frame_min = 0, frame_max = static_cast<uint32_t>(cap.get(cv::CAP_PROP_FRAME_COUNT));
        static uint32_t processed_frame = 0;

        ImGui::Begin("Control", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);
        ImGui::Text("GUI Framerate: %d", static_cast<int>(ImGui::GetIO().Framerate));
        ImGui::Text("Video Framerate: %d", video_FPS);
        ImGui::Text("Processing:");
        ImGui::SameLine();
        ImGui::ProgressBar(1.0f * processed_frame / frame_max, ImVec2(0.0f, 0.0f),
                           (to_string(processed_frame) + "/" + to_string(frame_max)).c_str());
        ImGui::Separator();
        ImGui::Checkbox("Show demo window", &show_demo_window);
        ImGui::Checkbox("Show result window", &show_res_window);
        ImGui::Checkbox("Show target window", &show_target_window);
        ImGui::Separator();
        ImGui::Checkbox("Playing", &playing);
        auto next = false;
        if (auto display_frame = static_cast<uint32_t>(cap.get(cv::CAP_PROP_POS_FRAMES));
                ImGui::SliderScalar("Display", ImGuiDataType_U32, &display_frame, &frame_min, &processed_frame,
                                    ("%u/" + to_string(processed_frame)).c_str())) {
            cap.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(display_frame));
            next = true;
        }
        next |= ImGui::Button("Next");
        ImGui::End();

        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        int hovered = -1;
        if (show_target_window) {
            auto[h, rewind] = draw_target_window(repo, video_FPS, &show_target_window);
            hovered = h;
            if (rewind != -1) {
                cap.set(cv::CAP_PROP_POS_FRAMES, static_cast<double>(rewind));
                next = true;
            }
        }

        static auto prev = chrono::steady_clock::now();
        if (auto elapsed = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now() - prev);
                elapsed.count() > 1000 / video_FPS && (playing || next) && cap.grab()) {
            prev += elapsed;
            cap.retrieve(image);
        }

        processed_frame = repo.load() + 1;

        if (show_res_window)
            draw_res_window(image, repo, static_cast<uint32_t>(cap.get(cv::CAP_PROP_POS_FRAMES)) - 1, hovered,
                            texture[2], &show_res_window);

        // Rendering
        ImGui::Render();
        int display_w, display_h;
        glfwMakeContextCurrent(window);
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.45f, 0.55f, 0.60f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwMakeContextCurrent(window);
        glfwSwapBuffers(window);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}