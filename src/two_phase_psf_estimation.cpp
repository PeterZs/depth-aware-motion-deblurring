#include <iostream>                     // cout, cerr, endl
#include <math.h>                       // sqrt
#include <opencv2/highgui/highgui.hpp>  // imread, imshow, imwrite

#include "two_phase_psf_estimation.hpp"

using namespace cv;
using namespace std;


namespace TwoPhaseKernelEstimation {

    float norm(float a, float b) {
        return sqrt(a * a + b * b);
    }

    /**
     * Compute usefulness of gradients:
     * 
     *           ||sum_y∈Nh(x) ∇B(y)||
     *  r(x) = ----------------------------
     *          sum_y∈Nh(x) ||∇B(y)|| + 0.5
     *          
     * @param confidence result
     * @param gradients  matrix with x and y gradients
     * @param width      width of window for Nh
     * @param mask       binary mask of region that should be computed
     */
    void computeGradientConfidence(Mat& confidence, const Mat& gradients, const int width,
                                   const Mat& mask) {
        confidence = Mat::zeros(gradients.rows, gradients.cols, CV_32F);

        // get range for Nh window
        int range = width / 2;

        // go through all pixels
        for (int x = width; x < (gradients.cols - width); x++) {
            for (int y = width; y < (gradients.rows - width); y++) {
                // check if inside region
                if (mask.at<uchar>(y, x) != 0) {
                    pair<float, float> sum = {0, 0};  // sum of the part: ||sum_y∈Nh(x) ∇B(y)||
                    float innerSum = 0;               // sum of the part: sum_y∈Nh(x) ||∇B(y)||

                    // sum all gradient values inside the window (width x width) around pixel
                    for (int xOffset = range * -1; xOffset <= range; xOffset++) {
                        for (int yOffset = range * -1; yOffset <= range; yOffset++) {
                            Vec2b gradient = gradients.at<Vec2b>(y + yOffset, x + xOffset);

                            sum.first += gradient[0];
                            sum.second += gradient[1];

                            // norm of gradient
                            innerSum += norm(gradient[0], gradient[1]);
                        }
                    }

                    confidence.at<float>(y, x) = norm(sum.first, sum.second) / (innerSum + 0.5);
                }
            }
        }
    }


    /**
     * Converts a matrix containing floats to a matrix
     * conatining uchars
     * 
     * @param ucharMat [description]
     * @param floatMat [description]
     */
    void convertFloatToUchar(Mat& ucharMat, const Mat& floatMat) {
        // find min and max value
        double min; double max;
        minMaxLoc(floatMat, &min, &max);

        // if the matrix is in the range [0, 1] just scale with 255
        if (min >= 0 && max < 1) {
            floatMat.convertTo(ucharMat, CV_8U, 255.0);
        } else {
            // handling that floats could be negative
            floatMat -= min;

            // convert and show
            floatMat.convertTo(ucharMat, CV_8U, 255.0/(max-min));
        }
    }


    void estimateKernel(Mat& psf, const Mat& image, const int psfWidth, const Mat& mask) {
        // set expected kernel witdh to odd number
        int width = (psfWidth % 2 == 0) ? psfWidth + 1 : psfWidth;

        // phase one: initialize kernel
        // 
        // all-zer kernel
        Mat kernel = Mat::zeros(psfWidth, psfWidth, CV_8U);

        // build an image pyramid
        int level = 1;

        vector<Mat> pyramid;
        pyramid.push_back(image);

        for (int i = 0; i < (level - 1); i++) {
            Mat downImage;
            pyrDown(pyramid[i], downImage, Size(pyramid[i].cols/2, pyramid[i].rows/2));
            pyramid.push_back(downImage);
        }

        // go through image image pyramid
        for (int i = 0; i < pyramid.size(); i++) {
            imshow("pyr " + i, pyramid[i]);
            // compute image gradient for x and y direction
            // 
            // gaussian blur
            GaussianBlur(pyramid[i], pyramid[i], Size(3,3), 0, 0, BORDER_DEFAULT);

            // convert it to gray
            Mat gray;
            cvtColor(pyramid[i], gray, CV_BGR2GRAY);

            Mat xGradients, yGradients;
            int delta = 0;
            int ddepth = CV_32F;
            int ksize = 3;
            int scale = 1;

            // gradient x
            Sobel(gray, xGradients, ddepth, 1, 0, ksize, scale, delta, BORDER_DEFAULT);

            // gradient y
            Sobel(gray, yGradients, ddepth, 0, 1, ksize, scale, delta, BORDER_DEFAULT);

            #ifndef NDEBUG
                // display gradients
                Mat xGradientsViewable, yGradientsViewable;
                convertFloatToUchar(xGradientsViewable, xGradients);
                convertFloatToUchar(yGradientsViewable, yGradients);
                imshow("x gradient", xGradientsViewable);
                imshow("y gradient", yGradientsViewable);
            #endif

            // merge gradients to one matrix with x and y gradients
            Mat gradients;
            vector<Mat> grads = {xGradients, yGradients};
            merge(grads, gradients);

            // TODO: remove borders of region - how?

            // compute gradient confidence for al pixels
            Mat gradientConfidence;
            computeGradientConfidence(gradientConfidence, gradients, width, mask);
            imshow("confidence", gradientConfidence);


        }

        psf = kernel;
    }
}