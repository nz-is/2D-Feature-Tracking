/* INCLUDES FOR THIS PROJECT */
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <vector>
#include <cmath>
#include <limits>
#include <numeric>
#include <opencv2/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/features2d.hpp>
#include <opencv2/xfeatures2d.hpp>
#include <opencv2/xfeatures2d/nonfree.hpp>

#include "dataStructures.h"
#include "matching2D.hpp"

using namespace std;

/* MAIN PROGRAM */
int main(int argc, const char *argv[])
{
  	string detectorType, descriptorType, descriptorMatchType;

  	if(argc > 1){
      	//command line parameter 
      	// argv[1] -> Detector
      	// argv[2] - > Descriptor
      	detectorType= (argv[1]);
      	if (detectorType.compare("SIFT") == 0)
          descriptorMatchType = "DES_HOG";
      	else
          descriptorMatchType = "DES_BINARY";
          
          
        descriptorType = (argv[2]);      
    } else {
        //det default
        cout << "Default BRISK det, BRIEF desc has been selected" << endl;
        detectorType =  "BRISK";
        descriptorType= "BRIEF";
        descriptorMatchType="DES_BINARY";
    }
    /* INIT VARIABLES AND DATA STRUCTURES */

    // data location
    string dataPath = "../";

    // camera
    string imgBasePath = dataPath + "images/";
    string imgPrefix = "KITTI/2011_09_26/image_00/data/000000"; // left camera, color
    string imgFileType = ".png";
    int imgStartIndex = 0; // first file index to load (assumes Lidar and camera names have identical naming convention)
    int imgEndIndex = 9;   // last file index to load
    int imgFillWidth = 4;  // no. of digits which make up the file index (e.g. img-0001.png)

    // misc
    int dataBufferSize = 2;       // no. of images which are held in memory (ring buffer) at the same time
    vector<DataFrame> dataBuffer; // list of data frames which are held in memory at the same time
    bool bVis = true;            // visualize results

  	//Init var.
    string matcherType = "MAT_FLANN";        // MAT_BF, MAT_FLANN
    string selectorType = "SEL_KNN";       // SEL_NN, SEL_KNN
  
  	//stats for each Keypoints Detectors & Descriptors
  	int kpts_cnt=0;
  	int kpts_matches_cnt=0;
  	vector<float> det_t;
  	vector<float> desc_t;
    vector<float> match_t;
 
    /* MAIN LOOP OVER ALL IMAGES */
    for (size_t imgIndex = 0; imgIndex <= imgEndIndex - imgStartIndex; imgIndex++)
    {
        /* LOAD IMAGE INTO BUFFER */

        // assemble filenames for current index
        ostringstream imgNumber;
        imgNumber << setfill('0') << setw(imgFillWidth) << imgStartIndex + imgIndex;
        string imgFullFilename = imgBasePath + imgPrefix + imgNumber.str() + imgFileType;

        // load image from file and convert to grayscale
        cv::Mat img, imgGray;
        img = cv::imread(imgFullFilename);
        cv::cvtColor(img, imgGray, cv::COLOR_BGR2GRAY);

        //// STUDENT ASSIGNMENT
        //// TASK MP.1 -> replace the following code with ring buffer of size dataBufferSize

        // push image into data frame buffer
        DataFrame frame;
        frame.cameraImg = imgGray;
        dataBuffer.push_back(frame);
      	//Remove first element (old image) if exceed databuffer
		if(dataBuffer.size() > dataBufferSize){
         	dataBuffer.erase(dataBuffer.begin()); 
        }
      	//cout << "dataBuffer size " << dataBuffer.size() << endl;
        //// EOF STUDENT ASSIGNMENT
        //cout << "#1 : LOAD IMAGE INTO BUFFER done" << endl;

        /* DETECT IMAGE KEYPOINTS */
        // extract 2D keypoints from current image
        vector<cv::KeyPoint> keypoints; // create empty feature list for current image

        //// STUDENT ASSIGNMENT
        //// TASK MP.2 -> add the following keypoint detectors in file matching2D.cpp and enable string-based selection based on detectorType
        //// -> HARRIS, FAST, BRISK, ORB, AKAZE, SIFT

        if (detectorType.compare("SHITOMASI") == 0)
        {
            detKeypointsShiTomasi(keypoints, imgGray, det_t, bVis);
        } else if(detectorType.compare("HARRIS") == 0)
        {
          	detKeypointsHarris(keypoints, imgGray, det_t, bVis);
        } else{
 			//calls to all generic keypointss
          	detKeypointsModern(keypoints, imgGray, detectorType, det_t, bVis);
        }
     
        //// EOF STUDENT ASSIGNMENT

        //// STUDENT ASSIGNMENT
        //// TASK MP.3 -> only keep keypoints on the preceding vehicle

        // only keep keypoints on the preceding vehicle
        bool bFocusOnVehicle = true;
        cv::Rect vehicleRect(535, 180, 180, 150);
        if (bFocusOnVehicle)
        {
			vector<cv::KeyPoint> kpt;
          	for(auto itr=keypoints.begin(); itr != keypoints.end(); itr++){
             	int x = (int) itr->pt.x;
              	int y = (int) itr->pt.y;
              	if (x>=vehicleRect.x && x <= vehicleRect.x + vehicleRect.width && y >= vehicleRect.y && y <= vehicleRect.y + vehicleRect.height){
                 kpt.push_back(*itr);
                }
            }
            keypoints = kpt;
        }
		kpts_cnt+=keypoints.size();
        //// EOF STUDENT ASSIGNMENT

        // optional : limit number of keypoints (helpful for debugging and learning)
        bool bLimitKpts = false;
        if (bLimitKpts)
        {
            int maxKeypoints = 50;

            if (detectorType.compare("SHITOMASI") == 0)
            { // there is no response info, so keep the first 50 as they are sorted in descending quality order
                keypoints.erase(keypoints.begin() + maxKeypoints, keypoints.end());
            }
            cv::KeyPointsFilter::retainBest(keypoints, maxKeypoints);
            //cout << " NOTE: Keypoints have been limited!" << endl;
        }

        // push keypoints and descriptor for current frame to end of data buffer
        (dataBuffer.end() - 1)->keypoints = keypoints;
      
        //cout << "#2 : DETECT KEYPOINTS done" << endl;

        /* EXTRACT KEYPOINT DESCRIPTORS */

        //// STUDENT ASSIGNMENT
        //// TASK MP.4 -> add the following descriptors in file matching2D.cpp and enable string-based selection based on descriptorType
        //// -> BRIEF, ORB, FREAK, AKAZE, SIFT

        cv::Mat descriptors;
        descKeypoints((dataBuffer.end() - 1)->keypoints, (dataBuffer.end() - 1)->cameraImg, descriptors, descriptorType, desc_t);
        //// EOF STUDENT ASSIGNMENT

        // push descriptors for current frame to end of data buffer
        (dataBuffer.end() - 1)->descriptors = descriptors;

        //cout << "#3 : EXTRACT DESCRIPTORS done" << endl;

        if (dataBuffer.size() > 1) // wait until at least two images have been processed
        {

            /* MATCH KEYPOINT DESCRIPTORS */

            vector<cv::DMatch> matches;
           

            //// STUDENT ASSIGNMENT
            //// TASK MP.5 -> add FLANN matching in file matching2D.cpp
            //// TASK MP.6 -> add KNN match selection and perform descriptor distance ratio filtering with t=0.8 in file matching2D.cpp

            matchDescriptors((dataBuffer.end() - 2)->keypoints, (dataBuffer.end() - 1)->keypoints,
                             (dataBuffer.end() - 2)->descriptors, (dataBuffer.end() - 1)->descriptors,
                             matches, descriptorMatchType, matcherType, match_t, selectorType);

            //// EOF STUDENT ASSIGNMENT

            // store matches in current data frame
            (dataBuffer.end() - 1)->kptMatches = matches;
			kpts_matches_cnt+=matches.size();
            cout << matches.size() << endl;
            //cout << "#4 : MATCH KEYPOINT DESCRIPTORS done" << endl;

            // visualize matches between current and previous image
            if (bVis)
            {
                cv::Mat matchImg = ((dataBuffer.end() - 1)->cameraImg).clone();
                cv::drawMatches((dataBuffer.end() - 2)->cameraImg, (dataBuffer.end() - 2)->keypoints,
                                (dataBuffer.end() - 1)->cameraImg, (dataBuffer.end() - 1)->keypoints,
                                matches, matchImg,
                                cv::Scalar::all(-1), cv::Scalar::all(-1),
                                vector<char>(), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);

                string windowName = "Matching keypoints between two camera images";
                cv::namedWindow(windowName, 7);
                cv::imshow(windowName, matchImg);
                cv::imwrite(windowName+ detectorType + descriptorType+".jpg", matchImg);
                //cout << "Press key to continue to next image" << endl;
                cv::waitKey(0); // wait for key to be pressed
            }
            bVis = false;
        }
    } // eof loop over all images
  	
  	cout << "Detector: " << detectorType << " Descriptor: " << descriptorType << endl;
	cout << "Total avg. Kpts: " << kpts_cnt /10 << endl;
  	cout << "Total avg. Matched kpts: " << kpts_matches_cnt  /10 << endl;
  	float det_avg = std::accumulate(det_t.begin(), det_t.end(), 0.0) / det_t.size();
  	cout << "Avg. time taken " << detectorType << ": " << det_avg << " ms" << endl;
  	
  	float desc_avg = std::accumulate(desc_t.begin(), desc_t.end(), 0.0) / desc_t.size();
   	cout << "Avg. time taken " << descriptorType << ": " << desc_avg << " ms" << endl;
  	float match_avg = std::accumulate(match_t.begin(), match_t.end(), 0.0)/ match_t.size();
    cout << "Avg. time taken for matching kpts: " << match_avg << " ms" << endl; 
  	cout << "Total time taken: " << match_avg + desc_avg + det_avg << " ms" << endl;
    return 0;

}
