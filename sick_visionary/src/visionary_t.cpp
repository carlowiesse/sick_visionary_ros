/*!
 *****************************************************************
 * \file
 *
 * \note
 *   Copyright (c) 2015 \n
 *   Fraunhofer Institute for Manufacturing Engineering
 *   and Automation (IPA) \n\n
 *
 * \note
 *   Copyright (c) 2015 \n
 *   SICK AG \n\n
 *
 *****************************************************************
 *
 * \note
 *   ROS package name: sick_visionary
 *
 * \author
 *   Author: Joshua Hampp, Andreas Richert
 *
 * \date Date of creation: 05/21/2015
 *
 *****************************************************************
 *
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 ****************************************************************/

#include <memory>
#include <ros/ros.h>
#include <image_transport/image_transport.h>
#include <sensor_msgs/image_encodings.h>
#include <sensor_msgs/PointCloud2.h>
#include <cv_bridge/cv_bridge.h>
#include <boost/shared_ptr.hpp>
#include <boost/make_shared.hpp>
#include <boost/thread.hpp>
#include <sensor_msgs/CameraInfo.h>
#include <sensor_msgs/LaserScan.h>
#include <std_msgs/ByteMultiArray.h>

#include "VisionaryTData.h"    // Header specific for the Time of Flight data
#include "VisionaryDataStream.h"
#include "VisionaryControl.h"

///// Distance code for data outside the TOF range
//const uint16_t NARE_DISTANCE_VALUE = 0xffffU;

boost::shared_ptr<VisionaryControl> gControl;

boost::shared_ptr<VisionaryTData> gDataHandler;

image_transport::Publisher gPubDepth, gPubConfidence, gPubIntensity;
ros::Publisher gPubCameraInfo, gPubPoints, gPubCart, gPubScan/*, gPubIos*/;

std::string gFrameId;

boost::mutex gDataMtx;
bool gReceive = true;

void publishCameraInfo(std_msgs::Header header, VisionaryTData& dataHandler)
{
    sensor_msgs::CameraInfo ci;
    ci.header = header;

    ci.height = dataHandler.getHeight();
    ci.width  = dataHandler.getWidth();

    ci.D.clear();
    ci.D.resize(5,0);
    ci.D[0] = dataHandler.getCameraParameters().k1;
    ci.D[1] = dataHandler.getCameraParameters().k2;
    ci.D[2] = dataHandler.getCameraParameters().p1;
    ci.D[3] = dataHandler.getCameraParameters().p2;
    ci.D[4] = dataHandler.getCameraParameters().k3;

    for(int i=0; i<9; i++)
    {
       ci.K[i]=0;
    }
    ci.K[0] = dataHandler.getCameraParameters().fx;
    ci.K[4] = dataHandler.getCameraParameters().fy;
    ci.K[2] = dataHandler.getCameraParameters().cx;
    ci.K[5] = dataHandler.getCameraParameters().cy;
    ci.K[8] = 1;

    for(int i=0; i<12; i++)
        ci.P[i] = 0;//data.getCameraParameters().cam2worldMatrix[i];
    //TODO:....
    ci.P[0] = dataHandler.getCameraParameters().fx;
    ci.P[5] = dataHandler.getCameraParameters().fy;
    ci.P[10] = 1;
    ci.P[2] = dataHandler.getCameraParameters().cx;
    ci.P[6] = dataHandler.getCameraParameters().cy;

    gPubCameraInfo.publish(ci);
}

void publishDepth(std_msgs::Header header, VisionaryTData& dataHandler)
{
    std::vector<uint16_t> vec = dataHandler.getDistanceMap();
    cv::Mat m = cv::Mat(dataHandler.getHeight(), dataHandler.getWidth(), CV_16UC1);
    memcpy(m.data, vec.data(), vec.size()*sizeof(uint16_t));
    sensor_msgs::ImagePtr msg = cv_bridge::CvImage(std_msgs::Header(), sensor_msgs::image_encodings::TYPE_16UC1, m).toImageMsg();

    msg->header = header;
    gPubDepth.publish(msg);
}

void publishConfidence(std_msgs::Header header, VisionaryTData& dataHandler)
{
    std::vector<uint16_t> vec = dataHandler.getConfidenceMap();
    cv::Mat m = cv::Mat(dataHandler.getHeight(), dataHandler.getWidth(), CV_16UC1);
    memcpy(m.data, vec.data(), vec.size()*sizeof(uint16_t));
    sensor_msgs::ImagePtr msg = cv_bridge::CvImage(std_msgs::Header(), sensor_msgs::image_encodings::TYPE_16UC1, m).toImageMsg();

    msg->header = header;
    gPubConfidence.publish(msg);
}

void publishIntensity(std_msgs::Header header, VisionaryTData& dataHandler)
{
    std::vector<uint16_t> vec = dataHandler.getIntensityMap();
    cv::Mat m = cv::Mat(dataHandler.getHeight(), dataHandler.getWidth(), CV_16UC1);
    memcpy(m.data, vec.data(), vec.size()*sizeof(uint16_t));
    sensor_msgs::ImagePtr msg = cv_bridge::CvImage(std_msgs::Header(), sensor_msgs::image_encodings::TYPE_16UC1, m).toImageMsg();

    msg->header = header;
    gPubIntensity.publish(msg);
}

void publishPointCloud(std_msgs::Header header, VisionaryTData& dataHandler)
{
    typedef sensor_msgs::PointCloud2 PointCloud;

    // Allocate new point cloud message
    PointCloud::Ptr cloudMsg (new PointCloud);
    cloudMsg->header = header;
    cloudMsg->height = dataHandler.getHeight();
    cloudMsg->width  = dataHandler.getWidth();
    cloudMsg->is_dense = false;
    cloudMsg->is_bigendian = false;

    cloudMsg->fields.resize(5);
    cloudMsg->fields[0].name = "x";
    cloudMsg->fields[1].name = "y";
    cloudMsg->fields[2].name = "z";
    cloudMsg->fields[3].name = "confidence";
    cloudMsg->fields[4].name = "intensity";
    int offset = 0;
    for (size_t d = 0; d < 3; ++d, offset += sizeof(float))
    {
        cloudMsg->fields[d].offset = offset;
        cloudMsg->fields[d].datatype = int(sensor_msgs::PointField::FLOAT32);
        cloudMsg->fields[d].count  = 1;
    }

    cloudMsg->fields[3].offset = offset;
    cloudMsg->fields[3].datatype = int(sensor_msgs::PointField::UINT16);
    cloudMsg->fields[3].count  = 1;
    offset += sizeof(uint16_t);

    cloudMsg->fields[4].offset = offset;
    cloudMsg->fields[4].datatype = int(sensor_msgs::PointField::UINT16);
    cloudMsg->fields[4].count  = 1;
    offset += sizeof(uint16_t);

    cloudMsg->point_step = offset;
    cloudMsg->row_step   = cloudMsg->point_step * cloudMsg->width;
    cloudMsg->data.resize (cloudMsg->height * cloudMsg->row_step);

    std::vector<PointXYZ> pointCloud;
    dataHandler.generatePointCloud(pointCloud);
    dataHandler.transformPointCloud(pointCloud);

    //simple copy to create a XYZ point cloud
    //memcpy(&cloud_msg->data[0], &pointCloud[0], pointCloud.size()*sizeof(PointXYZ));

    std::vector<uint16_t>::const_iterator itConf = dataHandler.getConfidenceMap().begin();
    std::vector<uint16_t>::const_iterator itIntens = dataHandler.getIntensityMap().begin();
    std::vector<PointXYZ>::const_iterator itPC = pointCloud.begin();
    size_t cloudSize = dataHandler.getHeight() * dataHandler.getWidth();
    for (size_t index = 0; index < cloudSize; ++index, ++itConf, ++itIntens, ++itPC)
    {
      memcpy (&cloudMsg->data[index * cloudMsg->point_step + cloudMsg->fields[0].offset], &*itPC, sizeof(PointXYZ));

      memcpy (&cloudMsg->data[index * cloudMsg->point_step + cloudMsg->fields[3].offset], &*itConf, sizeof (uint16_t));
      memcpy (&cloudMsg->data[index * cloudMsg->point_step + cloudMsg->fields[4].offset], &*itIntens, sizeof (uint16_t));
    }
    gPubPoints.publish(cloudMsg);
}

void publishScan(std_msgs::Header header, VisionaryTData& dataHandler)
{
    sensor_msgs::LaserScan msg;
    msg.range_min = 0.;
    msg.range_max = 8.0; // maximal value given by interface //TODO: get max value from camera
    msg.angle_min = dataHandler.getPolarStartAngle()*M_PI/180;
    msg.angle_increment = dataHandler.getPolarAngularResolution()*M_PI/180;

    std::vector<float> distances = dataHandler.getPolarDistanceData();
    std::transform(distances.begin(), distances.end(), distances.begin(), std::bind2nd(std::divides<float>(),1000));        // millimeter to meter
    msg.ranges = distances;

    std::vector<float> confidences = dataHandler.getPolarConfidenceData();
    std::transform(confidences.begin(), confidences.end(), confidences.begin(), std::bind2nd(std::divides<float>(),1000));  // millimeter to meter
    msg.intensities = confidences;

    msg.angle_max = msg.angle_min + msg.angle_increment*msg.ranges.size();
    msg.header = header;

    gPubScan.publish(msg);
}

void publishCartesian(std_msgs::Header header, VisionaryTData& dataHandler)
{
    typedef sensor_msgs::PointCloud2 PointCloud;

    // Allocate new point cloud message
    PointCloud::Ptr cloudMsg (new PointCloud);
    cloudMsg->header = header; // Use depth image time stamp
    cloudMsg->height = dataHandler.getCartesianSize();
    cloudMsg->width  = 1;
    cloudMsg->is_dense = true;
    cloudMsg->is_bigendian = false;

    cloudMsg->fields.resize (4);
    cloudMsg->fields[0].name = "x"; cloudMsg->fields[1].name = "y"; cloudMsg->fields[2].name = "z";
    cloudMsg->fields[3].name = "confidence";
    int offset = 0;
    for (size_t d = 0; d < cloudMsg->fields.size (); ++d, offset += sizeof(float))
    {
        cloudMsg->fields[d].offset = offset;
        cloudMsg->fields[d].datatype = int(sensor_msgs::PointField::FLOAT32);
        cloudMsg->fields[d].count  = 1;
    }
    cloudMsg->point_step = offset;
    cloudMsg->row_step   = cloudMsg->point_step * cloudMsg->width;
    cloudMsg->data.resize (cloudMsg->height * cloudMsg->width * cloudMsg->point_step);

    std::vector<PointXYZC> cartesianData = dataHandler.getCartesianData();
    //simple copy to have the values in millimeters
    //memcpy(&cloud_msg->data[0], &cartesianData[0], cartesianData.size()*sizeof(PointXYZC));

    size_t index = 0;
    for (std::vector<PointXYZC>::iterator itCart = cartesianData.begin(), itEnd = cartesianData.end(); itCart != itEnd; ++itCart, ++index)
    {
      PointXYZC point;
      point.x = itCart->x / 1000.;
      point.y = itCart->y / 1000.;
      point.z = itCart->z / 1000.;
      point.c = itCart->c;
      memcpy (&cloudMsg->data[index * cloudMsg->point_step + cloudMsg->fields[0].offset], &point, sizeof(PointXYZC));
    }

    gPubCart.publish(cloudMsg);
}

void publish_frame(VisionaryTData& dataHandler) {
    bool publishedAnything = false;

    std_msgs::Header header;
    header.stamp = ros::Time::now();
    header.frame_id = gFrameId;

    if (gPubCameraInfo.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishCameraInfo(header, dataHandler);
    }
    if (gPubDepth.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishDepth(header, dataHandler);
    }
    if (gPubConfidence.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishConfidence(header, dataHandler);
    }
    if (gPubIntensity.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishIntensity(header, dataHandler);
    }
    if (gPubPoints.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishPointCloud(header, dataHandler);
    }
    if (gPubScan.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishScan(header, dataHandler);
    }
    if (gPubCart.getNumSubscribers() > 0)
    {
        publishedAnything = true;
        publishCartesian(header, dataHandler);
    }

    if (!publishedAnything)
    {
        ROS_DEBUG("Nothing published");
        if(gControl) gControl->stopAcquisition();
    }
}

void thr_publish_frame()
{
    gDataMtx.lock();
    publish_frame(*gDataHandler);
    gDataMtx.unlock();
}

void thr_receive_frame(boost::shared_ptr<VisionaryDataStream> pDataStream, boost::shared_ptr<VisionaryTData> pDataHandler)
{
    while (gReceive)
    {
        if (!pDataStream->getNextFrame())
        {
            continue;     // No valid frame received
        }
        if(gDataMtx.try_lock())
        {
            gDataHandler = pDataHandler;
            gDataMtx.unlock();
            boost::thread thr(&thr_publish_frame);
        }
        else
            ROS_INFO("skipping frame with number %d", pDataHandler->getFrameNum());
    }
}

void _on_new_subscriber() {
    ROS_DEBUG("Got new subscriber");
    if(gControl) gControl->startAcquisition();
}

void on_new_subscriber_ros(const ros::SingleSubscriberPublisher& pub) {        
    _on_new_subscriber();
}

void on_new_subscriber_it(const image_transport::SingleSubscriberPublisher& pub) {        
    _on_new_subscriber();
}

int main(int argc, char **argv) {
    ros::console::set_logger_level(ROSCONSOLE_DEFAULT_NAME, ros::console::levels::Debug);
    ros::init(argc, argv, "sick_visionary_t");
    ros::NodeHandle nh("~");

    //default parameters
    std::string remoteDeviceIp="192.168.1.10";
    gFrameId = "camera";

    ros::param::get("~remote_device_ip", remoteDeviceIp);
    ros::param::get("~frame_id", gFrameId);

    boost::shared_ptr<VisionaryTData> pDataHandler = boost::make_shared<VisionaryTData> ();
    boost::shared_ptr<VisionaryDataStream> pDataStream = boost::make_shared<VisionaryDataStream> (pDataHandler);
    gControl = boost::make_shared<VisionaryControl>();

    ROS_INFO("Connecting to device at %s", remoteDeviceIp.c_str());
    if(!gControl->open(VisionaryControl::ProtocolType::COLA_B, remoteDeviceIp.c_str(), 5000/*ms*/))
    {
        ROS_ERROR("Connection with devices control channel failed");
        return -1;
    }
    // To be sure the acquisition is currently stopped.
    gControl->stopAcquisition();

    if(!pDataStream->open(remoteDeviceIp.c_str(), htons(2114)))
    {
        ROS_ERROR("Connection with devices data channel failed");
        return -1;
    }

    // TODO: add get device name and device version and print to ros info.
    ROS_INFO("Connected with Visionary-T");

    //make me public (after init.)
    image_transport::ImageTransport it(nh);
    gPubCameraInfo = nh.advertise<sensor_msgs::CameraInfo>("camera_info", 1, (ros::SubscriberStatusCallback)on_new_subscriber_ros, ros::SubscriberStatusCallback());
    //gPubIos = nh.advertise<std_msgs::ByteMultiArray>("ios", 1, (ros::SubscriberStatusCallback)on_new_subscriber_ros, ros::SubscriberStatusCallback());

    // capture one frame to get the information, which parts are included
    gControl->stepAcquisition();
    if (pDataStream->getNextFrame())
    {
        if (!pDataHandler->getDistanceMap().empty())
        {
          gPubDepth             = it.advertise("depth", 1, (image_transport::SubscriberStatusCallback)on_new_subscriber_it, image_transport::SubscriberStatusCallback());
          gPubPoints            = nh.advertise<sensor_msgs::PointCloud2>("points", 2, (ros::SubscriberStatusCallback)on_new_subscriber_ros, ros::SubscriberStatusCallback());
          gPubConfidence        = it.advertise("confidence", 1, (image_transport::SubscriberStatusCallback)on_new_subscriber_it, image_transport::SubscriberStatusCallback());
          gPubIntensity         = it.advertise("intensity", 1, (image_transport::SubscriberStatusCallback)on_new_subscriber_it, image_transport::SubscriberStatusCallback());
        }
        if (!pDataHandler->getPolarDistanceData().empty())
        {
          gPubScan      = nh.advertise<sensor_msgs::LaserScan>("scan", 2, (ros::SubscriberStatusCallback)on_new_subscriber_ros, ros::SubscriberStatusCallback());
        }
        if (!pDataHandler->getCartesianData().empty())
        {
          gPubCart      = nh.advertise<sensor_msgs::PointCloud2>("cartesian", 2, (ros::SubscriberStatusCallback)on_new_subscriber_ros, ros::SubscriberStatusCallback());
        }
    }
    else
    {
        ROS_ERROR("No valid initial frame received to get information about available data.");
        return -1;
    }

    //start receiver thread for camera images
    boost::thread rec_thr(boost::bind(&thr_receive_frame, pDataStream, pDataHandler));

    //wait til end of exec.
    ros::spin();

    gReceive = false;
    rec_thr.join();

    gControl->startAcquisition();
    gControl->close();
    pDataStream->close();

    gPubDepth.shutdown();
    gPubPoints.shutdown();
    gPubConfidence.shutdown();
    gPubIntensity.shutdown();
    gPubScan.shutdown();
    gPubCart.shutdown();
    gPubCameraInfo.shutdown();
    //gPubIos.shutdown();

    return 0;
}

