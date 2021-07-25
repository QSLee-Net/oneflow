/*
Copyright 2020 The OneFlow Authors. All rights reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#include <pybind11/pybind11.h>
#include "oneflow/api/python/common.h"
#include "oneflow/api/python/of_api_registry.h"
#include "oneflow/core/framework/device.h"

namespace py = pybind11;

namespace oneflow {

namespace {

struct DeviceExportUtil final {
  static Symbol<Device> MakeDevice(const std::string& type_and_id) {
    std::string type;
    int device_id = -1;
    ParsingDeviceTag(type_and_id, &type, &device_id).GetOrThrow();
    if (device_id == -1) { device_id = 0; }
    return MakeDevice(type, device_id);
  }

  static Symbol<Device> MakeDevice(const std::string& type, int64_t device_id) {
    if (Device::type_supported.find(type) == Device::type_supported.end()) {
      std::string error_msg =
          "Expected one of cpu, cuda device type at start of device string " + type;
      throw std::runtime_error(error_msg);
    }
    return Device::New(type, device_id).GetOrThrow();
  }
};

}  // namespace

ONEFLOW_API_PYBIND11_MODULE("", m) {
  py::class_<Symbol<Device>, std::shared_ptr<Symbol<Device>>>(m, "device")
      .def(py::init(
          [](const std::string& type_and_id) { return DeviceExportUtil::MakeDevice(type_and_id); }))
      .def(py::init([](const std::string& type, int64_t device_id) {
        return DeviceExportUtil::MakeDevice(type, device_id);
      }))
      .def_property_readonly("type", [](const Symbol<Device>& d) { return d->type(); })
      .def_property_readonly("index", [](const Symbol<Device>& d) { return d->device_id(); })
      .def("__int__", [](Symbol<Device> p) { return reinterpret_cast<const int64_t>(&*p); })
      .def("__eq__", [](const Symbol<Device>& d1, const Symbol<Device>& d2) { return *d1 == *d2; })
      .def("__str__", [](const Symbol<Device>& d) { return d->ToString(); })
      .def("__repr__", [](const Symbol<Device>& d) { return d->ToRepr(); });
}

}  // namespace oneflow
