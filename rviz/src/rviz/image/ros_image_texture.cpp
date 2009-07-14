/*
 * Copyright (c) 2009, Willow Garage, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     * Neither the name of the Willow Garage, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "ros_image_texture.h"

#include <OGRE/OgreTextureManager.h>

namespace rviz
{

ROSImageTexture::ROSImageTexture(const ros::NodeHandle& nh)
: nh_(nh)
, new_image_(false)
, width_(0)
, height_(0)
{
  const static uint32_t texture_data[4] = { 0x00ffff80, 0x00ffff80, 0x00ffff80, 0x00ffff80 };
  Ogre::DataStreamPtr pixel_stream;
  pixel_stream.bind(new Ogre::MemoryDataStream( (void*)&texture_data[0], 16 ));

  static uint32_t count = 0;
  std::stringstream ss;
  ss << "ROSImageTexture" << count++;
  texture_ = Ogre::TextureManager::getSingleton().loadRawData(ss.str(), Ogre::ResourceGroupManager::DEFAULT_RESOURCE_GROUP_NAME, pixel_stream, 2, 2, Ogre::PF_R8G8B8A8, Ogre::TEX_TYPE_2D, 0);
}

ROSImageTexture::~ROSImageTexture()
{
}

void ROSImageTexture::clear()
{
  const static uint32_t texture_data[4] = { 0x00ffff80, 0x00ffff80, 0x00ffff80, 0x00ffff80 };
  Ogre::DataStreamPtr pixel_stream;
  pixel_stream.bind(new Ogre::MemoryDataStream( (void*)&texture_data[0], 16 ));

  texture_->unload();
  texture_->loadRawData(pixel_stream, 2, 2, Ogre::PF_R8G8B8A8);

  new_image_ = false;
  current_image_.reset();
}

void ROSImageTexture::setTopic(const std::string& topic)
{
  sub_.shutdown();
  if (!topic.empty())
  {
    sub_ = nh_.subscribe(topic, 1, &ROSImageTexture::callback, this);
  }
}

bool ROSImageTexture::update()
{
  sensor_msgs::Image::ConstPtr image;
  bool new_image = false;
  {
    boost::mutex::scoped_lock lock(mutex_);

    image = current_image_;
    new_image = new_image_;
  }

  if (!image || !new_image)
  {
    return false;
  }

  if (image->depth != "uint8")
  {
    ROS_ERROR("Unsupported image depth [%s]", image->depth.c_str());
    return false;
  }

  if (image->uint8_data.layout.dim.size() != 3)
  {
    ROS_ERROR("Unsupported # of dimensions [%d]", image->uint8_data.layout.dim.size());
    return false;
  }

  if (image->uint8_data.layout.dim[0].label != "height"
   || image->uint8_data.layout.dim[1].label != "width"
   || image->uint8_data.layout.dim[2].label != "channel")
  {
    ROS_ERROR("Unsupported image layout.  Currently only 0=height/1=width/2=channel is supported");
    return false;
  }

  Ogre::PixelFormat format = Ogre::PF_R8G8B8;

  if (image->encoding == "rgb")
  {
    format = Ogre::PF_R8G8B8;
  }
  else if (image->encoding == "bgr")
  {
    format = Ogre::PF_B8G8R8;
  }
  else if (image->encoding == "rgba")
  {
    format = Ogre::PF_R8G8B8A8;
  }
  else if (image->encoding == "bgra")
  {
    format = Ogre::PF_B8G8R8A8;
  }
  else if (image->encoding == "mono")
  {
    format = Ogre::PF_L8;
  }
  else
  {
    ROS_ERROR("Unsupported image encoding [%s]", image->encoding.c_str());
    return false;
  }

  width_ = image->uint8_data.layout.dim[1].size;
  height_ = image->uint8_data.layout.dim[0].size;

  uint32_t size = image->uint8_data.layout.dim[0].stride;
  Ogre::DataStreamPtr pixel_stream;
  pixel_stream.bind(new Ogre::MemoryDataStream((void*)(&image->uint8_data.data[0] + image->uint8_data.layout.data_offset), size));
  texture_->unload();
  texture_->loadRawData(pixel_stream, width_, height_, format);

  return true;
}

void ROSImageTexture::callback(const sensor_msgs::Image::ConstPtr& msg)
{
  boost::mutex::scoped_lock lock(mutex_);
  current_image_ = msg;
  new_image_ = true;
}

}
