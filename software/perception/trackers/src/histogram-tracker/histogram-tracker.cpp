#include "histogram-tracker.hpp"

HistogramTracker::HistogramTracker() { 
    vTRACKING_MODE = DEFAULT; 
    internal_init(); 
  mask_initialized_=false;
  
  
          fx_ = 610.1778;
          fy_ = 610.1778;
          cx_ = 512.5;
          cy_ = 272.5;
  
}

HistogramTracker::HistogramTracker(int mode) { 
    vTRACKING_MODE = DEFAULT; 
    internal_init(); 
  mask_initialized_=false;
}

HistogramTracker::~HistogramTracker() {
}

void
HistogramTracker::internal_init() { 

    // Hue bins, params etc. 
  hue_info.size = 40, val_info.size = 40, sat_info.size = 40;
  hue_info.bin_dev = 5, val_info.bin_dev = 5, sat_info.bin_dev = 5;

  hue_info.ranges[0] = 0, hue_info.ranges[1] = 180;
  val_info.ranges[0] = 10, val_info.ranges[1] = 255;
  sat_info.ranges[0] = 10, sat_info.ranges[1] = 255;
#define AFFORDANCE_OFFSET 64

  hue_info.pranges = hue_info.ranges;
  val_info.pranges = val_info.ranges;
  sat_info.pranges = sat_info.ranges;

  hue_info.init();
  val_info.init();
  sat_info.init();
  return;
}

bool 
HistogramTracker::computeMaskROI(const cv::Mat& img, const cv::Mat& mask) { 
    std::vector<std::vector<cv::Point> > contours;
    findContours(mask, contours, CV_RETR_LIST, CV_CHAIN_APPROX_SIMPLE);

    int minx = img.cols, miny = img.rows; 
    int maxx = -1, maxy = -1;
    for (int j=0; j<contours.size(); j++) { 
        for (int k=0; k<contours[j].size(); k++) { 
            int x = contours[j][k].x; 
            int y = contours[j][k].y; 
            if (x < minx) minx = x; 
            if (y < miny) miny = y; 
            if (x > maxx) maxx = x; 
            if (y > maxy) maxy = y; 
        }
    }
    int rx = minx - 5, ry = miny - 5; 
    int rw = maxx-minx + 10, rh = maxy-miny + 10;
    if (!(rx >=0 && rx + rw < img.cols && 
          ry >=0 && ry + rh < img.rows) || rh <= 0 || rw <= 0)
        return false;

    cv::Mat roi = cv::Mat(img, cv::Rect(rx, ry, rw, rh));
    roi.copyTo(object_roi);
    return true;
}

bool
HistogramTracker::initialize(const cv::Mat& img, const cv::Mat& mask) {
  
  hue_info = HistogramInfo();
  val_info = HistogramInfo();
  sat_info = HistogramInfo();
  object_roi = cv::Mat();
  internal_init();

    // Compute mask roi for debug
    if (!computeMaskROI(img, mask)) 
        return false;

    // Convert to HSV space
    cv::Mat hsv; 
    cvtColor(img, hsv, CV_BGR2HSV);

    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    assert(channels.size() == 3);
    cv::Mat hue = channels[0]; 
    cv::Mat val = channels[1];
    cv::Mat sat = channels[2];
    // std::cerr << "hue: " << hue << std::endl;

    // Calculate Histogram
    calcHist(&hue, 1, 0, mask, hue_info.histogram, 1, &hue_info.size, &hue_info.pranges); 
    calcHist(&val, 1, 0, mask, val_info.histogram, 1, &val_info.size, &val_info.pranges); 
    calcHist(&sat, 1, 0, mask, sat_info.histogram, 1, &sat_info.size, &sat_info.pranges); 

    // std::cerr << "hue hist: " << hue_info.histogram << " " << std::endl;
    // std::cerr << "val hist: " << val_info.histogram << " " << std::endl;

    normalize(hue_info.histogram, hue_info.histogram, 0, 1, CV_MINMAX);
    normalize(val_info.histogram, val_info.histogram, 0, 1, CV_MINMAX);
    normalize(sat_info.histogram, sat_info.histogram, 0, 1, CV_MINMAX);

    // Compute unimodal histogram
    hue_info.computeUnimodalHistogram();
    val_info.computeUnimodalHistogram();
    sat_info.computeUnimodalHistogram();

    // Create debug histograms
    hue_info.createHistogramImage();
    val_info.createHistogramImage();
    sat_info.createHistogramImage();

    // cv::Mat display = img.clone();
    // showHistogramInfo(display);

    // cv::imshow("Initialize Histogram Tracker", display);
    mask_initialized_ = true;
    return true;
}

void
HistogramTracker::update_prediction(const cv::Mat& img) { 
    

    return;
}

std::vector<float>
HistogramTracker::update(cv::Mat& img, float scale, std::vector< Eigen::Vector3d > & pts,
        Eigen::Isometry3d local_to_camera) { 
    // Assign uniform likelihoods:
    std::vector<float> loglikelihoods;
    loglikelihoods.assign ( pts.size() ,0);    
  
    if (hue_info.histogram.empty() || val_info.histogram.empty()) return loglikelihoods;
 
    // Downsample image
    cv::Mat _img; 
    if (scale == 1.f)
        _img = img;
    else
        cv::resize(img, _img, cv::Size(), scale, scale, cv::INTER_LINEAR);

    // Convert to HSV space
    cv::Mat hsv; 
    cvtColor(_img, hsv, CV_BGR2HSV);

    std::vector<cv::Mat> channels;
    cv::split(hsv, channels);
    assert(channels.size() == 3);
    cv::Mat_<float> hue = channels[0]; 
    cv::Mat_<float> val = channels[1];
    cv::Mat_<float> sat = channels[2];
    // std::cerr << "hue: " << hue << std::endl;

    // Calculate likelihood
    cv::Mat hue_bp, val_bp, sat_bp;
    hue_info.backProject(hue, hue_bp);
    val_info.backProject(val, val_bp);
    sat_info.backProject(sat, sat_bp);
    //hue_info.backProjectUnimodal(hue, hue_bp);
    //val_info.backProjectUnimodal(val, val_bp);


    cv::Mat_<float> _bp;
    multiply(hue_bp, val_bp, _bp);
    multiply(sat_bp, _bp, _bp);
    normalize(_bp, _bp, 0, 1, CV_MINMAX);
    
    // for (int y=0; y<_bp.rows; y++)  
    //   for (int x=0; x<_bp.cols; x++)
    // 	std::cerr << _bp(y,x) << std::endl
          
    // // Apply bell curved mapping instead of linear scale
    // float sigma2 = 0.5;
    // cv::Mat_<float> bp_xmu = (1.f - _bp);
    // multiply(bp_xmu, bp_xmu, bp_xmu);
    // cv::exp(-bp_xmu / (sigma2), _bp);

    cv::Mat_<float> _bp8 = _bp * 255;

    cv::Mat bp8;
    _bp8.convertTo(bp8, CV_8UC1);

    
    // Upsample image
    cv::Mat bp;
    if (scale == 1.f)
        bp = bp8;
    else
        cv::resize(bp8, bp, cv::Size(), 1.f/scale, 1.f/scale, cv::INTER_LINEAR);
    
    
    /// Use the confidence mask to determine the object location: (very basic algorithm, added mfallon)
    // 1. Use image moments to determine ML object location:
    cv::Moments m=cv::moments(bp8); 
    //printf("moments %f %f %f %f %f %f %f %f\n",m.m00,m.m01,m.m20,m.m11,m.m02,m.m30,m.m21,m.m03); 
    http://opencv.willowgarage.com/documentation/cpp/structural_analysis_and_shape_descriptors.html
    int u_estimated = m.m10/m.m00;
    int v_estimated = m.m01/m.m00;
    std::cout << u_estimated << " and " << v_estimated << "\n";

    // 2. Project particles into camera frame:
    Eigen::Affine3d transform;
    transform.setIdentity();
    Eigen::Translation3d translation(local_to_camera.translation());
    Eigen::Quaterniond quat(local_to_camera.rotation());
    transform = transform * translation * quat;
    for (size_t i = 0; i < pts.size (); ++i){
      pts[i] = transform*pts[i];
    }    
      
    // 3. Determine Likelihood in Image space:
    for (size_t i=0; i< pts.size(); i++) {
      // u = pt.x fx/pt.z   ... project point to pixel
      Eigen::Vector3d pt1 = pts[i];
      int u = floor( ((pt1[0] * fx_)/pt1[2]) + cx_);
      int v = floor( ((pt1[1] * fy_)/pt1[2]) + cy_);
      int dist = sqrt( pow( u - u_estimated ,2) + pow( v - v_estimated ,2) );
      // Crude Binary Likelihood:
      if (dist < 13){
        loglikelihoods[i] =1; //was 1
      }
    }        
      
      
    /// Visualization:
    cv::Mat bp3;
    cv::cvtColor(bp8, bp3, CV_GRAY2BGR);
    addWeighted(img, 0.05, bp3, 0.95, 0, img); 
    // cv::imshow( "Hue Belief", hue_bp); 
    // cv::imshow( "Val Belief", val_bp); 
    // cv::imshow( "Sat Belief", val_bp); 

    for (size_t i=0; i< pts.size(); i++) { // draw particles on image
      Eigen::Vector3d pt1 = pts[i];
      int u = floor( ((pt1[0] * fx_)/pt1[2])  +  512.5);
      int v = floor( ((pt1[1] * fy_)/pt1[2]) + 272.5);
      Point center( u, v );
      circle( img, center, 3, Scalar(0,255,0), -1, 8, 0 );
    }      
    Point center( u_estimated, v_estimated );
    circle( img, center, 10, Scalar(0,0,255), -1, 8, 0 );
      
    showHistogramInfo(img);

    return loglikelihoods;
}

cv::Mat 
HistogramTracker::get_belief() { 
    return belief; 
}

void 
HistogramTracker::showHistogramInfo(cv::Mat& img) { 
    float scale = 0.2;
    int w = img.cols, h = img.rows;
    int sw = scale * w, sh = scale * h; 
    
    int off = 5;
    float ow = object_roi.cols, oh = object_roi.rows;
    cv::Mat obj_roi = cv::Mat(img, cv::Rect(w-3*sw-3*off-ow,off,ow,oh));
    cv::Mat hue_roi = cv::Mat(img, cv::Rect(w-3*sw-3*off,off,sw,sh));
    cv::Mat val_roi = cv::Mat(img, cv::Rect(w-2*sw-2*off,off,sw,sh));
    cv::Mat sat_roi = cv::Mat(img, cv::Rect(w-sw-off,off,sw,sh));

    if (!hue_info.histogram.empty()) { 
      cv::Mat pip_hist, pip_val, pip_sat, pip_obj;
        cv::resize(hue_info.histogram_img, pip_hist, hue_roi.size());
        cv::resize(val_info.histogram_img, pip_val, val_roi.size());
	cv::resize(sat_info.histogram_img, pip_sat, sat_roi.size());

    ConciseArgs opt(argc, (char**)argv);
    opt.add(options.vCHANNEL, "c", "camera-channel","Camera Channel [CAMERALEFT]");
    opt.add(options.vAFFORDANCE_CHANNEL, "a", "affordance-channel","Affordance Channel [CAMERALEFT_MASKZIPPED]");
    opt.add(options.vSCALE, "s", "scale","Scale Factor");
    opt.add(options.vAFFORDANCE_ID, "i", "id","Affordance ID");
    opt.add(options.vDEBUG, "d", "debug","Debug mode");
    opt.parse();
  
    std::cerr << "===========  Histogram Tracker ============" << std::endl;
    std::cerr << "=> CAMERA CHANNEL : " << options.vCHANNEL << std::endl;
    std::cerr << "=> AFFORDANCE CHANNEL : " << options.vAFFORDANCE_CHANNEL << std::endl;
    std::cerr << "=> SCALE : " << options.vSCALE << std::endl;
    std::cerr << "=> AFFORDANCE ID : " << options.vAFFORDANCE_ID << std::endl;
    std::cerr << "=> DEBUG : " << options.vDEBUG << std::endl;
    std::cerr << "=> Note: Hit 'c' to capture mask" << std::endl;
    std::cerr << "===============================================" << std::endl;
  
    // Install signal handler to free data.
    signal(SIGINT, INThandler);

    // Param server, botframes
    state = new state_t();

    // Subscriptions
    bot_core_image_t_subscribe(state->lcm, options.vCHANNEL.c_str(), on_image_frame, (void*)state);
    bot_core_image_t_subscribe(state->lcm, options.vAFFORDANCE_CHANNEL.c_str(), on_affordance_frame, (void*)state);
    perception_image_roi_t_subscribe(state->lcm, "TLD_OBJECT_ROI", on_segmentation_frame, (void*)state);

    // Main lcm handle
    while(1) { 
        unsigned char c = cv::waitKey(1) & 0xff;
        lcm_handle(state->lcm);
        if (c == 'q') {
            break;  
        } else if ( c == 'c' ) { 
            if (state->aff_img.empty()) { 
                std::cerr << "AFFORDANCE IMAGE UNAVAILABLE" << std::endl;
                continue;
            }
            std::cerr << "Capturing Affordance : " << options.vAFFORDANCE_ID << std::endl;
            cv::Mat1b mask = (state->aff_img == AFFORDANCE_OFFSET + options.vAFFORDANCE_ID);
	    cv::Mat1b maskd = mask.clone();
            if (!state->tracker) { 
                std::cerr << "Tracker Not Initialized!" << std::endl;
                assert(0);
            }

            // Initialize with image and mask
            state->tracker->initialize(state->img, mask);

            if (options.vDEBUG) { 
                cv::imshow("Captured Image", state->img);
                cv::imshow("Captured Mask", maskd);
            }
        }
    }
    return;
}

void HistogramTracker::plot_histogram(char* win) { 
    // cv::imshow(win, hist_img); 
    return;
} 
