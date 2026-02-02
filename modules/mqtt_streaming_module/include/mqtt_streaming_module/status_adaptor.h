/*
 * Copyright 2022-2025 openDAQ d.o.o.
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
 */

#pragma once
#include <coretypes/type_manager_ptr.h>
#include <mqtt_streaming_module/common.h>
#include <opendaq/component_status_container_private_ptr.h>
#include <opendaq/component_status_container_ptr.h>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

class StatusAdaptor
{
public:
    StatusAdaptor(const std::string typeName,
                 const std::string statusName,
                 ComponentStatusContainerPtr statusContainer,
                 std::string initState,
                 TypeManagerPtr typeManager)
        : typeName(typeName),
          statusName(statusName),
          statusContainer(statusContainer),
          typeManager(typeManager)
    {
        currentStatus = Enumeration(typeName, initState, typeManager);
        currentMessage = "";
        statusContainer.template asPtr<IComponentStatusContainerPrivate>(true).addStatus(statusName, currentStatus);
    }

    bool setStatus(const std::string& status, const std::string& message = "")
    {
        std::scoped_lock lock(statusMutex);
        const auto newStatus = Enumeration(typeName, String(status), typeManager);
        bool changed = (newStatus != currentStatus || message != currentMessage);
        if (changed)
        {
            currentStatus = newStatus;
            currentMessage = message;
            statusContainer.template asPtr<IComponentStatusContainerPrivate>(true).setStatusWithMessage(statusName, currentStatus, message);
        }
        return changed;
    }
    std::string getStatus()
    {
        std::scoped_lock lock(statusMutex);
        return currentStatus.getValue().toStdString();
    }

private:
    const std::string typeName;
    const std::string statusName;
    std::string currentMessage;
    ComponentStatusContainerPtr statusContainer;
    EnumerationPtr currentStatus;
    TypeManagerPtr typeManager;
    std::mutex statusMutex;
};

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
