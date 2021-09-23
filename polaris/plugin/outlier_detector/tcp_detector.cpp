//  Copyright (C) 2019 THL A29 Limited, a Tencent company. All rights reserved.
//
//  Licensed under the BSD 3-Clause License (the "License"); you may not use this file
//  except in compliance with the License. You may obtain a copy of the License at
//
//  https://opensource.org/licenses/BSD-3-Clause
//
//  Unless required by applicable law or agreed to in writing, software distributed
//  under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
//  CONDITIONS OF ANY KIND, either express or implied. See the License for the specific
//  language governing permissions and limitations under the License.
//

#include "plugin/outlier_detector/tcp_detector.h"

#include "logger.h"
#include "plugin/outlier_detector/outlier_detector.h"
#include "plugin/plugin_manager.h"
#include "polaris/config.h"
#include "polaris/model.h"
#include "utils/netclient.h"
#include "utils/time_clock.h"
#include "utils/utils.h"

namespace polaris {

TcpOutlierDetector::TcpOutlierDetector() { timeout_ms_ = 0; }

TcpOutlierDetector::~TcpOutlierDetector() {}

ReturnCode TcpOutlierDetector::Init(Config* config, Context* /*context*/) {
  std::string send_package = config->GetStringOrDefault(
      OutlierDetectorConfig::kTcpSendPackageKey, OutlierDetectorConfig::kTcpSendPackageDefault);
  if (send_package.empty()) {
    POLARIS_LOG(LOG_ERROR, "outlier detector[%s] config %s is empty", kPluginTcpOutlierDetector,
                OutlierDetectorConfig::kTcpSendPackageKey);
    return kReturnInvalidConfig;
  } else if (!Utils::HexStringToBytes(send_package, &send_package_)) {
    POLARIS_LOG(LOG_ERROR, "outlier detector[%s] config %s hexstring to bytes failed",
                kPluginTcpOutlierDetector, OutlierDetectorConfig::kTcpSendPackageKey);
    return kReturnInvalidConfig;
  }
  std::string receive_package =
      config->GetStringOrDefault(OutlierDetectorConfig::kTcpReceivePackageKey,
                                 OutlierDetectorConfig::kTcpReceivePackageDefault);
  if (!receive_package.empty()) {  // 需要匹配应答
    if (!Utils::HexStringToBytes(receive_package, &receive_package_)) {
      POLARIS_LOG(LOG_ERROR, "outlier detector[%s] config %s hexstring to bytes failed",
                  kPluginTcpOutlierDetector, OutlierDetectorConfig::kTcpReceivePackageKey);
      return kReturnInvalidConfig;
    }
  }
  timeout_ms_ = config->GetIntOrDefault(OutlierDetectorConfig::kTimeoutKey,
                                        OutlierDetectorConfig::kTimeoutDefault);
  return kReturnOk;
}

ReturnCode TcpOutlierDetector::DetectInstance(Instance& instance, DetectResult& detect_result) {
  uint64_t start_time_ms    = Time::GetCurrentTimeMs();
  detect_result.detect_type = kPluginTcpOutlierDetector;
  std::string host          = instance.GetHost();
  int port                  = instance.GetPort();
  std::string tcp_response;
  int retcode = NetClient::TcpSendRecv(host, port, timeout_ms_, send_package_, &tcp_response);
  if (retcode < 0) {
    detect_result.return_code = kReturnNetworkFailed;
    detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
    return kReturnNetworkFailed;
  }
  // 需要匹配应答包
  if (!receive_package_.empty() && receive_package_ != tcp_response) {
    detect_result.return_code = kReturnServerError;
    detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
    return kReturnServerError;
  }
  detect_result.return_code = kReturnOk;
  detect_result.elapse      = Time::GetCurrentTimeMs() - start_time_ms;
  return kReturnOk;
}

}  // namespace polaris