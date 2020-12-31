#include <numeric>
#include "matching2D.hpp"

using namespace std;

// Find best matches for keypoints in two camera images based on several matching methods
void matchDescriptors(std::vector<cv::KeyPoint> &kPtsSource, std::vector<cv::KeyPoint> &kPtsRef, cv::Mat &descSource, cv::Mat &descRef,
                      std::vector<cv::DMatch> &matches, std::string descriptorType, std::string matcherType, std::vector<float>& match_t, std::string selectorType)
{
    // configure matcher
    bool crossCheck = false;
    cv::Ptr<cv::DescriptorMatcher> matcher;
    if (matcherType.compare("MAT_BF") == 0)
    {
        int normType = descriptorType.compare("DES_BINARY") ==0 ? cv::NORM_HAMMING : cv::NORM_L2;
        matcher = cv::BFMatcher::create(normType, crossCheck);
    }
    else if (matcherType.compare("MAT_FLANN") == 0)
    {
      	if(descSource.type() != CV_32F){
      		descSource.convertTo(descSource, CV_32F);
        }
        if(descRef.type() != CV_32F){
            descRef.convertTo(descRef, CV_32F);
        }   
        matcher = cv::DescriptorMatcher::create(cv::DescriptorMatcher::FLANNBASED);
    } 
  
   double t = ((double) cv::getTickCount());
   if (selectorType.compare("SEL_NN") == 0)
    { // nearest neighbor (best match)

        matcher->match(descSource, descRef, matches); // Finds the best match for each descriptor in desc1
    }
    else if (selectorType.compare("SEL_KNN") == 0)
    { // k nearest neighbors (k=2)
      	std::vector<std::vector<cv::DMatch>> knn_matches;
        matcher->knnMatch(descSource, descRef, knn_matches, 2);
      
    	//-- Filter matches using the Lowe's ratio test
        const float ratio_thresh = 0.8f;
        for (size_t i = 0; i < knn_matches.size(); i++)
        {
            if (knn_matches[i][0].distance < ratio_thresh * knn_matches[i][1].distance)
            {
                matches.push_back(knn_matches[i][0]);
            }
        }
    }

    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
    match_t.push_back(1000 * t / 1.0 );
}

// Use one of several types of state-of-art descriptors to uniquely identify keypoints
void descKeypoints(vector<cv::KeyPoint> &keypoints, cv::Mat &img, cv::Mat &descriptors, string descriptorType, vector<float>& desc_t)
{
    // select appropriate descriptor
    cv::Ptr<cv::DescriptorExtractor> extractor;
    if (descriptorType.compare("BRISK") == 0)
    {

        int threshold = 30;        // FAST/AGAST detection threshold score.
        int octaves = 3;           // detection octaves (use 0 to do single scale)
        float patternScale = 1.0f; // apply this scale to the pattern used for sampling the neighbourhood of a keypoint.

        extractor = cv::BRISK::create(threshold, octaves, patternScale);
    }
    else if (descriptorType.compare("BRIEF") == 0){
      	extractor = cv::xfeatures2d::BriefDescriptorExtractor::create();
    } else if (descriptorType.compare("ORB")==0){
    	extractor = cv::ORB::create();  
    } else if (descriptorType.compare("FREAK")==0){
     	extractor = cv::xfeatures2d::FREAK::create(); 
    }else if (descriptorType.compare("SIFT")==0){
     	extractor = cv::SIFT::create(); 
    }else if (descriptorType.compare("AKAZE")==0){
     	cout << "AKAZE descriptor" << endl;
        extractor = cv::AKAZE::create(); 
    }else {
        throw std::invalid_argument("unknown descriptor type");
    }

    // perform feature description
    double t = (double)cv::getTickCount();
    extractor->compute(img, keypoints, descriptors);
    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
  	desc_t.push_back(1000*t/1.0);
    //cout << descriptorType << " descriptor extraction in " << 1000 * t / 1.0 << " ms" << endl;
}

void detKeypointsHarris(vector<cv::KeyPoint> &keypoints, cv::Mat &img, vector<float>& det_t, bool bVis){
	int blockSize = 2;
  	int apertureSize = 3;
  	int thresh(100);
  	double maxOverlap=0.0; //dont allow overlap
  	double k = 0.04; 
  	double t = ((double) cv::getTickCount());	
  
  	cv::Mat dst = cv::Mat::zeros(img.size(), CV_32FC1);
  	cv::cornerHarris(img, dst, blockSize, apertureSize, k);
  
  	cv::Mat dst_norm, dst_norm_scaled;
  	cv::normalize(dst, dst_norm, 0, 255, cv::NORM_MINMAX, CV_32FC1, cv::Mat());
  	cv::convertScaleAbs(dst_norm, dst_norm_scaled);
  
  	for(size_t j =0; j < dst_norm.rows; j++){
      for(size_t i = 0; i < dst_norm.cols; i++){
        int response = (int) dst_norm.at<float>(j, i);
        bool bOverlap = false;
        if(response > thresh){
         	cv::KeyPoint kpt;
          	kpt.pt = cv::Point2f(i, j);
          	kpt.response = response;
          	kpt.size = apertureSize * 2;
          	
          /* apply NMS */

            for(auto itr=keypoints.begin(); itr != keypoints.end(); itr++){
              double overlap = cv::KeyPoint::overlap(*itr, kpt);
              if(overlap > maxOverlap){
                bOverlap=true;
                if(kpt.response > itr->response){
                 	*itr = kpt;
                  	break;
                }
              }
          }
          
          if(!bOverlap){
           	keypoints.push_back(kpt); 
          }
        }
      }
      
    }
  	// visualize results
    if (bVis)
    {
        cv::Mat visImage = img.clone();
        cv::drawKeypoints(img, keypoints, visImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        string windowName = "Harris Corner Detector Results";
        cv::namedWindow(windowName, 6);
        imshow(windowName, visImage);
        cv::waitKey(0);
    }
  t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
  det_t.push_back(1000 * t / 1.0 );
  cout << "Harris corner detection with n=" << keypoints.size() << endl;
}

void detKeypointsModern(std::vector<cv::KeyPoint> &keypoints, cv::Mat &img, std::string detectorType, vector<float>& det_t, bool bVis){
  	cv::Ptr<cv::Feature2D> kpt_det;
    double t = ((double)cv::getTickCount());
  	if(detectorType.compare("FAST") == 0){
      	bool nms = true;
      	double threshold = 10;
	 	kpt_det = cv::FastFeatureDetector::create(threshold, nms);
  		kpt_det->detect(img, keypoints);
    } else if(detectorType.compare("ORB") == 0){
     	kpt_det = cv::ORB::create(); 
    	kpt_det->detect(img, keypoints);
    } else if(detectorType.compare("BRISK") == 0){
        kpt_det = cv::BRISK::create(); 
    	kpt_det->detect(img, keypoints);
    } else if(detectorType.compare("AKAZE") == 0){
        kpt_det = cv::AKAZE::create(); 
    	kpt_det->detect(img, keypoints);
    } else if(detectorType.compare("SIFT") == 0){
        kpt_det = cv::SIFT::create(); 
    	kpt_det->detect(img, keypoints);
    }
    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
  	det_t.push_back(1000 * t / 1.0);
    //cout << detectorType << " detection with n=" << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << endl;
  
    if(bVis){
          cv::Mat visImage =img.clone();
          cv::drawKeypoints(img, keypoints, visImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
          string windowName = detectorType +" Detector Results";
          cv::namedWindow(windowName, 6);
          cv::imshow(windowName, visImage);
          cv::waitKey(0);
      }
  	
}

// Detect keypoints in image using the traditional Shi-Thomasi detector
void detKeypointsShiTomasi(vector<cv::KeyPoint> &keypoints, cv::Mat &img, vector<float>& det_t, bool bVis)
{
    // compute detector parameters based on image size
    int blockSize = 4;       //  size of an average block for computing a derivative covariation matrix over each pixel neighborhood
    double maxOverlap = 0.0; // max. permissible overlap between two features in %
    double minDistance = (1.0 - maxOverlap) * blockSize;
    int maxCorners = img.rows * img.cols / max(1.0, minDistance); // max. num. of keypoints

    double qualityLevel = 0.01; // minimal accepted quality of image corners
    double k = 0.04;

    // Apply corner detection
    double t = (double)cv::getTickCount();
    vector<cv::Point2f> corners;
    cv::goodFeaturesToTrack(img, corners, maxCorners, qualityLevel, minDistance, cv::Mat(), blockSize, false, k);

    // add corners to result vector
    for (auto it = corners.begin(); it != corners.end(); ++it)
    {

        cv::KeyPoint newKeyPoint;
        newKeyPoint.pt = cv::Point2f((*it).x, (*it).y);
        newKeyPoint.size = blockSize;
        keypoints.push_back(newKeyPoint);
    }
    t = ((double)cv::getTickCount() - t) / cv::getTickFrequency();
  	det_t.push_back(1000 * t / 1.0);
    //cout << "Shi-Tomasi detection with n=" << keypoints.size() << " keypoints in " << 1000 * t / 1.0 << " ms" << endl;

    // visualize results
    if (bVis)
    {
        cv::Mat visImage = img.clone();
        cv::drawKeypoints(img, keypoints, visImage, cv::Scalar::all(-1), cv::DrawMatchesFlags::DRAW_RICH_KEYPOINTS);
        string windowName = "Shi-Tomasi Corner Detector Results";
        cv::namedWindow(windowName, 6);
        cv::imshow(windowName, visImage);
        cv::waitKey(0);
    }
}
