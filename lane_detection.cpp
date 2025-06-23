#include <iostream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>
#include <ctime>
#include <csignal>
#include <opencv2/opencv.hpp>

bool capturing = true;
cv::Mat perspectiveMatrix;
cv::Point2f srcPoints[4], dstPoints[4];
std::vector<cv::Point2f> collectedPoints;
bool matrixSet = false;
std::vector<int> histogramLane;
cv::Mat frameFinal;

void signalHandler(int signal) {
    std::cout << "\nStopping image capture gracefully..." << std::endl;
    capturing = false;
}

void mouseHandler(int event, int x, int y, int flags, void* userdata) {
    if (event == cv::EVENT_LBUTTONDOWN && collectedPoints.size() < 4) {
        collectedPoints.emplace_back(x, y);
        std::cout << "Point " << collectedPoints.size() << ": (" << x << ", " << y << ")" << std::endl;
    }
}

cv::Mat brightenImage(const cv::Mat& input, double alpha = 1.3, int beta = 40) {
    cv::Mat output;
    input.convertTo(output, -1, alpha, beta);
    return output;
}

void setupPerspectiveFromClicks() {
    for (int i = 0; i < 4; ++i) {
        srcPoints[i] = collectedPoints[i];
    }

    int birdWidth = 400;
    int birdHeight = 600;

    dstPoints[0] = cv::Point2f(0, 0);
    dstPoints[1] = cv::Point2f(birdWidth, 0);
    dstPoints[2] = cv::Point2f(birdWidth, birdHeight);
    dstPoints[3] = cv::Point2f(0, birdHeight);

    perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    matrixSet = true;
    std::cout << "Perspective matrix set using mouse clicks." << std::endl;
}

void drawLaneAOI(cv::Mat& image) {
    if (!matrixSet) return;
    std::vector<cv::Point> roi = {
        srcPoints[0], srcPoints[1], srcPoints[2], srcPoints[3]
    };
    cv::polylines(image, roi, true, cv::Scalar(0, 0, 255), 2);
    cv::putText(image, "AOI - Lane", srcPoints[0] + cv::Point2f(0, -10),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 0, 255), 2);
    for (int i = 0; i < 4; i++) {
        cv::circle(image, srcPoints[i], 5, cv::Scalar(0, 255, 0), -1);
    }
}

void Threshold(const cv::Mat& framePers) {
    cv::Mat frameGray, frameEdge, frameThresh;
    cv::cvtColor(framePers, frameGray, cv::COLOR_RGB2GRAY);
    cv::inRange(frameGray, 240, 255, frameThresh);
    cv::Canny(frameGray, frameEdge, 900, 3, false);
    cv::add(frameThresh, frameEdge, frameFinal);
    cv::cvtColor(frameFinal, frameFinal, cv::COLOR_GRAY2RGB);
}

void Histogram() {
    histogramLane.resize(frameFinal.size().width);
    std::fill(histogramLane.begin(), histogramLane.end(), 0);
    for (int i = 0; i < frameFinal.size().width; i++) {
        cv::Mat ROILane = frameFinal(cv::Rect(i, frameFinal.rows - 100, 1, 100));
        histogramLane[i] = cv::countNonZero(ROILane);
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    const int delayMs = 300;

    cv::namedWindow("Full Frame with AOI");
    cv::setMouseCallback("Full Frame with AOI", mouseHandler);

    while (capturing) {
        std::string tempFile = "temp.jpg";
        std::string command = "libcamera-still -n -t 1 --width 1280 --height 720 --mode 1280:720 -o " + tempFile;
        std::system(command.c_str());

        cv::Mat img = cv::imread(tempFile);
        if (img.empty()) {
            std::cerr << "Failed to load captured image!" << std::endl;
            continue;
        }

        cv::Mat bright = brightenImage(img);

        if (!matrixSet && collectedPoints.size() == 4) {
            setupPerspectiveFromClicks();
        }

        drawLaneAOI(bright);

        if (matrixSet) {
            cv::Mat birdsEye;
            cv::warpPerspective(bright, birdsEye, perspectiveMatrix, cv::Size(400, 600));

            Threshold(birdsEye);
            Histogram();

            cv::imshow("Bird's Eye View", birdsEye);
            cv::imshow("Gray & Binary", frameFinal);
        }

        cv::imshow("Full Frame with AOI", bright);

        if (cv::waitKey(1) == 'q') {
            capturing = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
    }

    cv::destroyAllWindows();
    std::cout << "Preview ended." << std::endl;
    return 0;
}