/*
 * Copyright 2017 Google Inc.
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
 **/
#ifndef UPDATER_H_
#define UPDATER_H_

#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "resource.h"
#include "store.h"
#include "time.h"

namespace google {

// Configuration object.
class Configuration;

// An abstract class for asynchronous updates of the metadata mapping.
class MetadataUpdater {
 public:
  struct ResourceMetadata {
    ResourceMetadata(const std::vector<std::string>& ids,
                     const MonitoredResource& resource,
                     MetadataStore::Metadata&& metadata)
        : ids_(ids), resource_(resource), metadata_(std::move(metadata)) {}
    ResourceMetadata(ResourceMetadata&& other)
        : ResourceMetadata(other.ids_, other.resource_,
                           std::move(other.metadata_)) {}

    const MetadataStore::Metadata& metadata() const { return metadata_; }
    const MonitoredResource& resource() const { return resource_; }
    const std::vector<std::string>& ids() const { return ids_; }

   private:
    friend class MetadataUpdater;  // Needs write access to metadata_.

    std::vector<std::string> ids_;
    MonitoredResource resource_;
    MetadataStore::Metadata metadata_;
  };

  MetadataUpdater(const Configuration& config, MetadataStore* store,
                  const std::string& name);
  virtual ~MetadataUpdater();

  // Starts updating.
  void start();

  // Stops updating.
  void stop();

  using UpdateCallback =
      std::function<void(std::vector<MetadataUpdater::ResourceMetadata>&&)>;

 protected:
  friend class UpdaterTest;

  // Validates the relevant configuration and returns true if it's correct.
  // Returns a bool that represents if it's configured properly.
  virtual bool ValidateConfiguration() const {
    return true;
  }

  // Internal method for starting the updater's logic.
  virtual void StartUpdater() = 0;

  // Internal method for stopping the updater's logic.
  virtual void StopUpdater() = 0;

  // Updates the resource map in the store.
  void UpdateResourceCallback(const ResourceMetadata& result) {
    store_->UpdateResource(result.ids_, result.resource_);
  }

  // Updates the metadata in the store. Consumes result.
  void UpdateMetadataCallback(ResourceMetadata&& result) {
    store_->UpdateMetadata(result.resource_, std::move(result.metadata_));
  }

  const Configuration& config() {
    return config_;
  }

 private:
  // The name of the updater provided by subclasses.
  std::string name_;

  const Configuration& config_;

  // The store for the metadata.
  MetadataStore* store_;
};

// A class for all periodic updates of the metadata mapping.
class PollingMetadataUpdater : public MetadataUpdater {
 public:
  PollingMetadataUpdater(
      const Configuration& config, MetadataStore* store,
      const std::string& name, double period_s,
      std::function<std::vector<ResourceMetadata>()> query_metadata);
  ~PollingMetadataUpdater();

 protected:
  friend class UpdaterTest;

  bool ValidateConfiguration() const;
  void StartUpdater();
  void StopUpdater();

 private:
  friend class InstanceTest;

  // Metadata poller.
  void PollForMetadata();

  // The polling period in seconds.
  time::seconds period_;

  // The function to actually query for metadata.
  std::function<std::vector<ResourceMetadata>()> query_metadata_;

  // The timer.
  std::timed_mutex timer_;

  // The thread that polls for new metadata.
  std::thread reporter_thread_;
};

}

#endif  // UPDATER_H_
