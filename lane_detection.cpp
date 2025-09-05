#include <iostream>
#include <cstdlib>
#include <string>
#include <chrono>
#include <thread>
#include <csignal>
#include <wiringPi.h>
#include <opencv2/opencv.hpp>

bool capturing = true;
cv::Mat perspectiveMatrix;
cv::Point2f srcPoints[4], dstPoints[4];
bool matrixSet = false;

const int birdWidth = 400;
const int birdHeight = 600;

cv::Point2f predefinedPoints[4] = {
    cv::Point2f(460, 300),
    cv::Point2f(1200, 300),
    cv::Point2f(1400, 700),
    cv::Point2f(100, 700)
};

void signalHandler(int signal) {
    std::cout << "\nStopping image capture..." << std::endl;
    capturing = false;
}

cv::Mat brightenImage(const cv::Mat& input, double alpha = 1.4, int beta = 50) {
    cv::Mat output;
    input.convertTo(output, -1, alpha, beta);
    return output;
}

void setupPerspectiveMatrix() {
    for (int i = 0; i < 4; ++i) {
        srcPoints[i] = predefinedPoints[i];
    }

    dstPoints[0] = cv::Point2f(0, 0);
    dstPoints[1] = cv::Point2f(birdWidth, 0);
    dstPoints[2] = cv::Point2f(birdWidth, birdHeight);
    dstPoints[3] = cv::Point2f(0, birdHeight);

    perspectiveMatrix = cv::getPerspectiveTransform(srcPoints, dstPoints);
    matrixSet = true;
}

void drawAOI(cv::Mat& img) {
    std::vector<cv::Point> roi = {
        srcPoints[0], srcPoints[1], srcPoints[2], srcPoints[3]
    };
    cv::polylines(img, roi, true, cv::Scalar(0, 0, 255), 2);
}

cv::Mat createBinaryLaneMask(const cv::Mat& birdsEye) {
    cv::Mat gray, binary;
    cv::cvtColor(birdsEye, gray, cv::COLOR_BGR2GRAY);
    cv::threshold(gray, binary, 250, 255, cv::THRESH_BINARY);
    return binary;
}

void findLanesSlidingWindow(const cv::Mat& binaryMask, cv::Mat& outputFrame) {
    int midpoint = binaryMask.cols / 2;
    int windowHeight = 50;
    int numWindows = binaryMask.rows / windowHeight;

    std::vector<cv::Point> leftLanePoints, rightLanePoints;

    cv::Mat lowerHalf = binaryMask(cv::Rect(0, binaryMask.rows / 2, binaryMask.cols, binaryMask.rows / 2));
    cv::Mat hist;
    cv::reduce(lowerHalf, hist, 0, cv::REDUCE_SUM, CV_32SC1);

    int maxLeft = 0, maxRight = 0;
    for (int i = 0; i < midpoint; ++i) {
        if (hist.at<int>(0, i) > hist.at<int>(0, maxLeft)) maxLeft = i;
    }
    for (int i = midpoint; i < binaryMask.cols; ++i) {
        if (hist.at<int>(0, i) > hist.at<int>(0, maxRight)) maxRight = i;
    }

    int leftX = maxLeft;
    int rightX = maxRight;

    for (int window = 0; window < numWindows; ++window) {
        int winYLow = binaryMask.rows - (window + 1) * windowHeight;
        int winYHigh = binaryMask.rows - window * windowHeight;

        int winXLeftLow = std::max(0, leftX - 25);
        int winXLeftHigh = std::min(binaryMask.cols, leftX + 25);
        cv::Rect leftWindow(winXLeftLow, winYLow, winXLeftHigh - winXLeftLow, windowHeight);
        cv::Mat leftROI = binaryMask(leftWindow);
        cv::Moments leftM = cv::moments(leftROI, true);
        if (leftM.m00 > 0) {
            int newX = static_cast<int>(leftM.m10 / leftM.m00);
            leftX = winXLeftLow + newX;
            leftLanePoints.push_back(cv::Point(leftX, (winYLow + winYHigh) / 2));
        }

        int winXRightLow = std::max(0, rightX - 25);
        int winXRightHigh = std::min(binaryMask.cols, rightX + 25);
        cv::Rect rightWindow(winXRightLow, winYLow, winXRightHigh - winXRightLow, windowHeight);
        cv::Mat rightROI = binaryMask(rightWindow);
        cv::Moments rightM = cv::moments(rightROI, true);
        if (rightM.m00 > 0) {
            int newX = static_cast<int>(rightM.m10 / rightM.m00);
            rightX = winXRightLow + newX;
            rightLanePoints.push_back(cv::Point(rightX, (winYLow + winYHigh) / 2));
        }
    }

    if (leftLanePoints.size() > 1)
        for (size_t i = 1; i < leftLanePoints.size(); ++i)
            cv::line(outputFrame, leftLanePoints[i - 1], leftLanePoints[i], cv::Scalar(0, 255, 0), 5);

    if (rightLanePoints.size() > 1)
        for (size_t i = 1; i < rightLanePoints.size(); ++i)
            cv::line(outputFrame, rightLanePoints[i - 1], rightLanePoints[i], cv::Scalar(0, 255, 0), 5);

    if (!leftLanePoints.empty() && !rightLanePoints.empty()) {
        int laneCenterX = (leftLanePoints.back().x + rightLanePoints.back().x) / 2;
        int frameCenterX = binaryMask.cols / 2;

        cv::line(outputFrame, cv::Point(laneCenterX, 0), cv::Point(laneCenterX, birdHeight), cv::Scalar(255, 0, 0), 2);
        cv::line(outputFrame, cv::Point(frameCenterX, 0), cv::Point(frameCenterX, birdHeight), cv::Scalar(0, 0, 255), 2);

        int result = frameCenterX - laneCenterX;
        std::string text = "Result: " + std::to_string(result);
        cv::putText(outputFrame, text, cv::Point(10, 40), cv::FONT_HERSHEY_SIMPLEX, 1.0, cv::Scalar(255, 255, 255), 2);
        
        if (result == 0) {
            digitalWrite(21, 0);
            digitalWrite(22, 0);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "Move Forward" << std::endl;
        } else if (result > 0 && result < 10) {
            digitalWrite(21, 1);
            digitalWrite(22, 0);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "right1" << std::endl;
        } else if (result >= 10 && result < 20) {
            digitalWrite(21, 0);
            digitalWrite(22, 1);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "right2" << std::endl;
        } else if (result > 20) {
            digitalWrite(21, 1);
            digitalWrite(22, 1);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "right3" << std::endl;
        } else if (result < 0 && result > -10) {
            digitalWrite(21, 1);
            digitalWrite(22, 0);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "left1" << std::endl;
        } else if (result <= -10 && result > -20) {
            digitalWrite(21, 0);
            digitalWrite(22, 1);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "left2" << std::endl;
        } else if (result <= -20) {
            digitalWrite(21, 1);
            digitalWrite(22, 1);
            digitalWrite(23, 0);
            digitalWrite(24, 0);
            std::cout << "left3" << std::endl;
        }
    }
}

int main() {
    std::signal(SIGINT, signalHandler);
    wiringPiSetup();
    pinMode(21, OUTPUT);
    pinMode(22, OUTPUT);
    pinMode(23, OUTPUT);
    pinMode(24, OUTPUT);
    setupPerspectiveMatrix();

    while (capturing) {
        std::string tempFile = "temp.jpg";
        std::string command = "libcamera-still -n -t 1 --width 1280 --height 720 --mode 1280:720 -o " + tempFile;
        std::system(command.c_str());

        cv::Mat frame = cv::imread(tempFile);
        if (frame.empty()) {
            std::cerr << "Failed to load image!" << std::endl;
            continue;
        }

        cv::Mat bright = brightenImage(frame);
        drawAOI(bright);

        cv::Mat birdsEye;
        cv::warpPerspective(bright, birdsEye, perspectiveMatrix, cv::Size(birdWidth, birdHeight));

        cv::Mat binaryMask = createBinaryLaneMask(birdsEye);

        cv::Mat frameFinal;
        binaryMask.copyTo(frameFinal);
        cv::cvtColor(frameFinal, frameFinal, cv::COLOR_GRAY2BGR);

        findLanesSlidingWindow(binaryMask, frameFinal);

        cv::imshow("Original", bright);
        cv::imshow("Perspective", birdsEye);
        cv::imshow("Final", frameFinal);

        if (cv::waitKey(1) == 'q') {
            capturing = false;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }

    cv::destroyAllWindows();
    return 0;
}