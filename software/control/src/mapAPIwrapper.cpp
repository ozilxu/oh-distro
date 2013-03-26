/* Calls the perception teams' MAP interface API */

#include <mex.h>

#include <Eigen/Dense>
#include <lcm/lcm-cpp.hpp>
#include <boost/thread/thread.hpp>

#include <maps/ViewClient.hpp>
#include <maps/BotWrapper.hpp>
#include <maps/DepthImageView.hpp>

#include <lcmtypes/drc/data_request_t.hpp>

using namespace Eigen;
using namespace maps;

struct ViewWrapperData {
  boost::shared_ptr<lcm::LCM> lcm;
  ViewClient* view_client;
  boost::thread* lcm_thread;
};

void lcmThreadMain(boost::shared_ptr<lcm::LCM>& lcm) {
  while(0 == lcm->handle()) {
    if (boost::this_thread::interruption_requested()) return;
  }
}

void mexFunction(int nlhs, mxArray *plhs[], int nrhs, const mxArray *prhs[])
{
  // usage:
  // ptr = mapAPIwrapper()  - initializes MAP interface
  // mapAPIwrapper(ptr)     - frees MAP interface
  // [closest_terrain_pos,normal] = mapAPIwrapper(ptr,pos)  
  //     - pos is a 3xn matrix
  //     - closest_terrain_pos is a 3xn matrix of the closest points on the terrain
  //     - normal is a 3xn matrix of the surface normals of the terrain at closest_terrain_pos

  struct ViewWrapperData* pdata = NULL;

  if (nrhs<1) { // then it's ptr = mapAPIWrapper()
    pdata = new struct ViewWrapperData;
    pdata->view_client = new ViewClient();

    // create lcm instance, and fork listener thread
    pdata->lcm.reset(new lcm::LCM());  //lcm_create(NULL); 
    if (!pdata->lcm->good()) mexErrMsgIdAndTxt("DRC:mapAPIwrapper:LCMFailed","Failed to create LCM instance");
    pdata->lcm_thread = new boost::thread(lcmThreadMain,pdata->lcm);

    BotWrapper::Ptr botWrapper(new BotWrapper(pdata->lcm, NULL, NULL));
    pdata->view_client->setBotWrapper(botWrapper);

    // start listening for view data
    pdata->view_client->start();

    // return a pointer to the model
    mxClassID cid;
    if (sizeof(pdata)==4) cid = mxUINT32_CLASS;
    else if (sizeof(pdata)==8) cid = mxUINT64_CLASS;
    else mexErrMsgIdAndTxt("DRC:mapAPIWrapper:PointerSize","Are you on a 32-bit machine or 64-bit machine??");
    plhs[0] = mxCreateNumericMatrix(1,1,cid,mxREAL);
    memcpy(mxGetData(plhs[0]),&pdata,sizeof(pdata));

    return;
  } 

  // load the data ptr
  if (!mxIsNumeric(prhs[0]) || mxGetNumberOfElements(prhs[0])!=1)
    mexErrMsgIdAndTxt("DRC:mapAPIWrapper:BadInputs","the first argument should be the ptr");
  memcpy(&pdata,mxGetData(prhs[0]),sizeof(pdata));
  
  if (nrhs<2) { // then free the memory and exit
    delete pdata->view_client;
    pdata->lcm_thread->interrupt();
    delete pdata->lcm_thread;
    //    lcm_destroy(pdata->lcm);  // now handled by shared_ptr and lcm destructor
    delete pdata;
    return;
  } 

  ViewClient::ViewPtr vptr = pdata->view_client->getView(drc::data_request_t::HEIGHT_MAP_SCENE);
  if (!vptr) { // check if null
    mexErrMsgIdAndTxt("DRC:mapAPIwrapper:NullViewPtr","could not get the heightmap view\n");
  }

  int N = mxGetN(prhs[1]);
  if (mxGetM(prhs[1])!=3) mexErrMsgIdAndTxt("DRC:mapAPIwrapper:BadInputs","pos must be 3xn\n"); 

  plhs[0] = mxCreateDoubleMatrix(3,N,mxREAL);
  mxArray* pmxNormal = mxCreateDoubleMatrix(3,N,mxREAL);

  Vector3f pos, closest_terrain_pos, normal;
  double *ppos = mxGetPr(prhs[1]), *pclosest_terrain_pos = mxGetPr(plhs[0]), *pnormal = mxGetPr(pmxNormal);
 
  int j;
  for (int i=0; i<N; i++) {
    pos << (float) ppos[3*(i-1)], (float) ppos[3*(i-1)+1], (float) ppos[3*(i-1)+2];
    if (vptr->getClosest(pos,closest_terrain_pos,normal)) {
	for (j=0; j<3; j++) {
	  pclosest_terrain_pos[3*(i-1)+j] = (double) closest_terrain_pos(j);
	  pnormal[3*(i-1)+j] = (double) normal(j);
	}
    } else { // returns false if off the grid		   
	for (j=0; j<3; j++) {
	  pclosest_terrain_pos[3*(i-1)+j] = NAN;
	  pnormal[3*(i-1)+j] = NAN;
	}
    }
  }

  if (nlhs>1) {
    plhs[1] = pmxNormal;
  } else {
    mxDestroyArray(pmxNormal);
  }
}

