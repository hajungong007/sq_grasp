#include<sq_fitting/segmentation.h>

LccpSegmentationAlgorithm::LccpSegmentationAlgorithm(ros::NodeHandle *handle, const SegmentationParameters &param, std::string name):
  nh_(*handle), service_name_(name){
  ROS_INFO("Segmentation server started");
  this->param_.zmin = param.zmin;
  this->param_.zmax = param.zmax;
  this->param_.th_points = param.th_points;
  this->param_.disable_transform = param.disable_transform;
  this->param_.voxel_resolution = param.voxel_resolution;
  this->param_.seed_resolution = param.seed_resolution;
  this->param_.color_importance = param.color_importance;
  this->param_.spatial_importance = param.spatial_importance;
  this->param_.normal_importance = param.normal_importance;
  this->param_.concavity_tolerance_threshold = param.concavity_tolerance_threshold;
  this->param_.smoothness_threshold = param.smoothness_threshold;
  this->param_.min_segment_size = param.min_segment_size;
  this->param_.use_extended_convexity = param.use_extended_convexity;
  this->param_.use_sanity_criterion = param.use_sanity_criterion;
  segmentation_server_ = nh_.advertiseService(service_name_, &LccpSegmentationAlgorithm::segmentationCallback, this);
  pthread_mutex_init(&this->obj_seg_mutex_, NULL);
}

LccpSegmentationAlgorithm::~LccpSegmentationAlgorithm(void){
  pthread_mutex_destroy(&this->obj_seg_mutex_);
}

void LccpSegmentationAlgorithm::obj_seg_mutex_enter_(void){
  pthread_mutex_lock(&this->obj_seg_mutex_);
}

void LccpSegmentationAlgorithm::obj_seg_mutex_exit_(void){
  pthread_mutex_unlock(&this->obj_seg_mutex_);
}



bool LccpSegmentationAlgorithm::segmentationCallback(sq_fitting::segment_object::Request &req,
                                                     sq_fitting::segment_object::Response &res){
  this->obj_seg_mutex_enter_();
  CloudPtr cloud(new PointCloud);
  pcl::fromROSmsg(req.input_cloud, *cloud);
  std::unique_ptr<lccp_segmentation> seg(new lccp_segmentation);
  seg->init(*cloud, this->param_);
  std::vector<Object> seg_objs;
  seg->segment();
  seg_objs = seg->get_segmented_objects();

  //Table cloud
  CloudPtr table_cloud = seg->get_plane_cloud();
  for(uint i = 0; i < table_cloud->points.size(); ++i){
    table_cloud->at(i).r = 0;
    table_cloud->at(i).g = 255;
    table_cloud->at(i).b = 0;
    table_cloud->at(i).a = 255;
  }
  pcl::toROSMsg(*table_cloud, res.plane_cloud);

  //Object clouds
  for(uint i=0;i<seg_objs.size();++i){
    sensor_msgs::PointCloud2 obj_msg;
    pcl::toROSMsg(seg_objs[i].obj_cloud, obj_msg);
    obj_msg.header.seq = i;
    obj_msg.header.frame_id = req.input_cloud.header.frame_id;
    obj_msg.header.stamp = ros::Time::now();
    res.object_cloud.push_back(obj_msg);
  }
  this->obj_seg_mutex_exit_();
  return true;
}


SegmentationParameters::SegmentationParameters(){
  this->disable_transform = this->DISABLE_TRANSFORM;
  this->voxel_resolution = this->VOXEL_RESOLUTION;
  this->seed_resolution = this->SEED_RESOLUTION;
  this->color_importance = this->COLOR_IMPORTANCE;
  this->spatial_importance = this->SPATIAL_IMPORTANCE;
  this->normal_importance = this->NORMAL_IMPORTANCE;

  this->concavity_tolerance_threshold = this->CONCAVITY_TOLERANCE_THRESHOLD;
  this->smoothness_threshold = this->SMOOTHNESS_THRESHOLD;
  this->min_segment_size = this->MIN_SEGMENT_SIZE;
  this->use_extended_convexity = this->USE_EXTENDED_CONVEXITY;
  this->use_sanity_criterion = this->USE_SANITY_CRITERION;

  this->zmin = this->ZMIN;
  this->zmax = this->ZMAX;
  this->th_points = this->TH_POINTS;
}

SegmentationParameters::~SegmentationParameters(){

}

lccp_segmentation::lccp_segmentation(){
  this->initialized_ = true;
}

lccp_segmentation::~lccp_segmentation(){

}

void lccp_segmentation::init(PointCloud input_cloud, SegmentationParameters &opt){
  this->cloud_ = input_cloud.makeShared();
  this->detected_objects_.resize(0);
  set_parameters(opt);
  this->initialized_ = true;
}

void lccp_segmentation::init(PointCloud input_cloud){
  this->cloud_ = input_cloud.makeShared();
  this->detected_objects_.resize(0);
  set_default_parameters();
  this->initialized_ = true;
}

void lccp_segmentation::set_parameters(const SegmentationParameters &opt){
  this->disable_transform = opt.disable_transform;
  this->voxel_resolution = opt.voxel_resolution;
  this->seed_resolution = opt.seed_resolution;
  this->color_importance = opt.color_importance;
  this->spatial_importance = opt.spatial_importance;
  this->normal_importance = opt.normal_importance;

  this->concavity_tolerance_threshold = opt.concavity_tolerance_threshold;
  this->smoothness_threshold = opt.smoothness_threshold;
  this->min_segment_size = opt.min_segment_size;
  this->use_extended_convexity = opt.use_extended_convexity;
  this->use_sanity_criterion = opt.use_sanity_criterion;

  this->zmin = opt.zmin;
  this->zmax = opt.zmax;
  this->th_points = opt.th_points;
}

void lccp_segmentation::set_default_parameters(){
  this->disable_transform = this->DISABLE_TRANSFORM;
  this->voxel_resolution = this->VOXEL_RESOLUTION;
  this->seed_resolution = this->SEED_RESOLUTION;
  this->color_importance = this->COLOR_IMPORTANCE;
  this->spatial_importance = this->SPATIAL_IMPORTANCE;
  this->normal_importance = this->NORMAL_IMPORTANCE;

  this->concavity_tolerance_threshold = this->CONCAVITY_TOLERANCE_THRESHOLD;
  this->smoothness_threshold = this->SMOOTHNESS_THRESHOLD;
  this->min_segment_size = this->MIN_SEGMENT_SIZE;
  this->use_extended_convexity = this->USE_EXTENDED_CONVEXITY;
  this->use_sanity_criterion = this->USE_SANITY_CRITERION;

  this->zmin = this->ZMIN;
  this->zmax = this->ZMAX;
  this->th_points = this->TH_POINTS;
}

void lccp_segmentation::reset(){
  this->cloud_->points.resize(0);
  this->detected_objects_.resize(0);
  this->lccp_labeled_cloud_->points.resize(0);
  this->labeled_voxel_cloud_->points.resize(0);
  this->normal_cloud_->points.resize(0);
  this->supervoxel_clusters_.clear();
  this->supervoxel_adjacency_.clear();
  set_default_parameters();
}


CloudPtr lccp_segmentation::get_input_cloud(){
  return this->cloud_;
}

CloudPtr lccp_segmentation::get_plane_cloud(){
  return this->table_plane_cloud_;
}

PointCloudl lccp_segmentation::get_labeled_voxel_cloud(){
  return *(this->labeled_voxel_cloud_);
}

std::multimap<uint32_t, uint32_t> lccp_segmentation::get_supervoxel_adjacency(){
  return this->supervoxel_adjacency_;
}

std::map<uint32_t, pcl::Supervoxel<PointT>::Ptr> lccp_segmentation::get_supervoxel_clusters(){
  return this->supervoxel_clusters_;
}

pcl::PointCloud<pcl::PointNormal> lccp_segmentation::get_normal_cloud(){
  return *(this->normal_cloud_);
}

void lccp_segmentation::set_disable_transform(bool disable_transform_in){
  this->disable_transform = disable_transform_in;
}

void lccp_segmentation::set_voxel_resolution(double voxel_resolution_in){
  this->voxel_resolution = voxel_resolution_in;
}

void lccp_segmentation::set_seed_resolution(double seed_resolution_in){
  this->seed_resolution = seed_resolution_in;
}

void lccp_segmentation::set_color_importance(double color_importance_in){
  this->color_importance = color_importance_in;
}

void lccp_segmentation::set_spatial_importance(double spatial_importance_in){
  this->spatial_importance = spatial_importance_in;
}

void lccp_segmentation::set_normal_importance(double normal_importance_in){
  this->normal_importance = normal_importance_in;
}

void lccp_segmentation::set_concavity_tolerance_threshold(double concavity_tolerance_threshold_in){
  this->concavity_tolerance_threshold = concavity_tolerance_threshold_in;
}

void lccp_segmentation::set_smoothness_threshold(double smoothness_threshold_in){
  this->smoothness_threshold = smoothness_threshold_in;
}

void lccp_segmentation::set_min_segment_size(int min_segment_size_in){
  this->min_segment_size = min_segment_size_in;
}

void lccp_segmentation::set_use_extended_convexity(bool use_extended_convexity_in){
  this->use_extended_convexity = use_extended_convexity_in;
}

void lccp_segmentation::set_use_sanity_criterion(bool use_sanity_criterion_in){
  this->use_sanity_criterion = use_sanity_criterion_in;
}

void lccp_segmentation::set_zmin(double zmin_in){
  this->zmin = zmin_in;
}

void lccp_segmentation::set_zmax(double zmax_in){
  this->zmax = zmax_in;
}

void lccp_segmentation::set_th_points(int th_points_in){
  this->th_points = th_points_in;
}

bool lccp_segmentation::get_disable_transform(){
  return this->disable_transform;
}

double lccp_segmentation::get_voxel_resolution(){
  return this->voxel_resolution;
}

double lccp_segmentation::get_seed_resolution(){
  return this->seed_resolution;
}

double lccp_segmentation::get_color_importance(){
  return this->color_importance;
}

double lccp_segmentation::get_spatial_importance(){
  return this->spatial_importance;
}

double lccp_segmentation::get_normal_importance(){
  return this->normal_importance;
}

double lccp_segmentation::get_concavity_tolerance_threshold(){
  return this->concavity_tolerance_threshold;
}

double lccp_segmentation::get_smoothness_threshold(){
  return this->smoothness_threshold;
}

int lccp_segmentation::get_min_segment_size(){
  return this->min_segment_size;
}

bool lccp_segmentation::get_use_extended_convexity(){
  return this->use_extended_convexity;
}

bool lccp_segmentation::get_use_sanity_criterion(){
  return this->use_sanity_criterion;
}

double lccp_segmentation::get_zmin(){
  return this->zmin;
}

double lccp_segmentation::get_zmax(){
  return this->zmax;
}

int lccp_segmentation::get_th_points(){
  return this->th_points;
}


void lccp_segmentation::detectObjectsOnTable(CloudPtr cloud, double zmin, double zmax, pcl::PointIndices::Ptr objectIndices, bool filter_input_cloud){
  //objects for storing point clouds
  CloudPtr plane(new PointCloud);
  CloudPtr convexHull(new PointCloud);

  //Get the plane model, if present
  pcl::SACSegmentation<PointT> segmentation;
  segmentation.setInputCloud(cloud);
  segmentation.setModelType(pcl::SACMODEL_PLANE);
  segmentation.setMethodType(pcl::SAC_RANSAC);
  segmentation.setDistanceThreshold(0.01);
  segmentation.setOptimizeCoefficients(true);
  pcl::PointIndices::Ptr planeIndices(new pcl::PointIndices);
  segmentation.segment(*planeIndices, this->plane_coefficients_);

  if(planeIndices->indices.size() == 0)
    std::cout<<"Could not find a plane in the scene."<<std::endl;
  else{
    //Copy the points of the plane to a new cloud
    pcl::ExtractIndices<PointT> extract;
    extract.setInputCloud(cloud);
    extract.setIndices(planeIndices);
    extract.filter(*plane);

    //Retrive the convex hull
    pcl::ConvexHull<PointT> hull;
    hull.setInputCloud(plane);
    hull.setDimension(2);
    hull.reconstruct(*convexHull);

    //redundant check
    if(hull.getDimension() == 2){
      //prism object
      pcl::ExtractPolygonalPrismData<PointT> prism;
      prism.setInputCloud(cloud);
      prism.setInputPlanarHull(convexHull);
      prism.setHeightLimits(zmin, zmax);
      prism.segment(*objectIndices);
      extract.setIndices(objectIndices);
      if(filter_input_cloud)
        extract.filter(*cloud);
    }
    else std::cout<<"The chosen hull is not planar."<<std::endl;
    this->table_plane_cloud_ = plane;
  }
}

std::vector<PointCloud> lccp_segmentation::get_segmented_objects_simple(){
  std::vector<PointCloud> obj_vec;
  for(int i = 0; i < this->detected_objects_.size(); ++i)
    obj_vec.push_back(detected_objects_[i].obj_cloud);
  return obj_vec;
}

std::vector<Object> lccp_segmentation::get_segmented_objects(){
  return this->detected_objects_;
}

bool lccp_segmentation::segment(){
  if(!this->initialized_){
    pcl::console::print_error("No valid input given to the algorithm. The class has not been initialized");
    return false;
  }
  pcl::PointIndices::Ptr obj_idx(new pcl::PointIndices());
  detectObjectsOnTable(this->cloud_, this->zmin, this->zmax, obj_idx, true);
  if(this->seed_resolution < 0.013)
    pcl::console::print_warn("seed resolution very low, the segmentation could be fragmented.");
  pcl::SupervoxelClustering<PointT> super(this->voxel_resolution, this->seed_resolution);
  detected_objects_.resize(0);
  if(this->cloud_->points.size() == 0){
    pcl::console::print_warn("No objects on the table");
    return false;
  }
  super.setInputCloud(this->cloud_);
  super.setColorImportance(this->color_importance);
  super.setSpatialImportance(this->spatial_importance);
  super.setNormalImportance(this->normal_importance);

  super.extract(supervoxel_clusters_);
  labeled_voxel_cloud_ = super.getLabeledVoxelCloud();
  CloudPtr voxel_cetroid_cloud = super.getVoxelCentroidCloud();
  normal_cloud_ = super.makeSupervoxelNormalCloud(supervoxel_clusters_);
  PointCloudl::Ptr full_labeled_cloud = super.getLabeledCloud();
  super.getSupervoxelAdjacency(supervoxel_adjacency_);
  std::map<uint32_t, pcl::Supervoxel<PointT>::Ptr> refined_supervoxel_clusters;

  PointCloudl::Ptr refined_labeled_voxel_cloud = super.getLabeledVoxelCloud();
  pcl::PointCloud<pcl::PointNormal>::Ptr refined_sv_normal_cloud = super.makeSupervoxelNormalCloud(refined_supervoxel_clusters);
  PointCloudl::Ptr refined_full_labeled_cloud = super.getLabeledCloud();

  typedef boost::adjacency_list<boost::setS, boost::setS, boost::undirectedS, uint32_t, float> VoxelAdjacencyList;
  VoxelAdjacencyList supervoxel_adjacency_list;
  super.getSupervoxelAdjacencyList(supervoxel_adjacency_list);

  uint k_factor = 0;
  if(use_extended_convexity)
    k_factor = 1;

  pcl::LCCPSegmentation<PointT> lccp;
  lccp.setConcavityToleranceThreshold(this->concavity_tolerance_threshold);
  lccp.setSanityCheck(this->use_sanity_criterion);
  lccp.setSmoothnessCheck(true, this->voxel_resolution, this->seed_resolution, this->smoothness_threshold);
  lccp.setKFactor(k_factor);
  lccp.setInputSupervoxels(this->supervoxel_clusters_, this->supervoxel_adjacency_);
  lccp.setMinSegmentSize(this->min_segment_size);
  lccp.segment();

  PointCloudl::Ptr labeled_cloud = super.getLabeledCloud();
  lccp_labeled_cloud_ = labeled_cloud->makeShared();
  lccp.relabelCloud(*lccp_labeled_cloud_);

  for(int i=0;i < lccp_labeled_cloud_->points.size(); ++i){
    uint32_t idx = lccp_labeled_cloud_->points.at(i).label;
    if(idx >= detected_objects_.size())
      detected_objects_.resize(idx+1);
    PointT tmp_point_rgb;
    tmp_point_rgb = cloud_->points.at(i);
    detected_objects_[idx].obj_cloud.points.push_back(tmp_point_rgb);
    detected_objects_[idx].label = (int)idx;
  }

  int size = detected_objects_.size();
  int i = 0;
  while(i<size){
    if(detected_objects_[i].obj_cloud.size() < this->th_points){
      detected_objects_.erase(detected_objects_.begin()+i);
      size = detected_objects_.size();
    }
    else
      i++;
  }
  return true;
}
