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
#include <mqtt_streaming_module/common.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

BEGIN_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE

namespace utils
{
    class StatusContainer;

    class Error
    {
    public:
        Error() = default;
        Error(std::weak_ptr<StatusContainer> container, std::string name)
            : container(std::move(container))
            , name(std::move(name))
        {
        }

        bool isValid() const
        {
            return !container.expired() && !name.empty();
        }

        void set(std::string msg);
        void add(const std::string& msg);
        void reset();

        bool ok() const;
        explicit operator bool() const
        {
            return ok();
        }
        std::string getMessage() const;
        bool isUpdated() const;
        const std::string& getName() const;
        std::string buildStatusMessage() const;
        std::string rawStatusMessage() const;

    private:
        std::weak_ptr<StatusContainer> container;
        std::string name;
    };

    class StatusContainer : public std::enable_shared_from_this<StatusContainer>
    {
    public:
        StatusContainer() = default;

        Error addStatus(const std::string& name)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            if (entries.count(name))
                return Error{};
            entries.emplace(name, Entry{});
            return Error(weak_from_this(), name);
        }

        Error getError(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            if (!entries.count(name))
                return Error{};
            return Error(std::const_pointer_cast<StatusContainer>(shared_from_this()), name);
        }

        bool isUpdated() const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            bool result = false;
            for (const auto& e : entries)
                result |= isUpdatedImpl(e.second);
            return result;
        }

        void resetAll()
        {
            std::scoped_lock<std::mutex> lock(mtx);
            for (auto& e : entries)
            {
                if (e.second.present)
                    e.second.updated = true;
                e.second.present = false;
                e.second.message.clear();
            }
        }

    private:
        friend class Error;

        struct Entry
        {
            std::string message;
            bool present = false;
            mutable bool updated = true;
        };

        static bool isUpdatedImpl(const Entry& e)
        {
            bool u = e.updated;
            e.updated = false;
            return u;
        }

        void set(const std::string& name, std::string msg)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            auto& e = entries.at(name);
            if (!e.present || e.message != msg)
            {
                e.updated = true;
                e.present = true;
                e.message = std::move(msg);
            }
        }

        void add(const std::string& name, const std::string& msg)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            auto& e = entries.at(name);
            e.updated = true;
            e.present = true;
            if (e.message.empty())
            {
                e.message = msg;
            }
            else
            {
                e.message.reserve(e.message.size() + msg.size() + 2);
                e.message.append("; ");
                e.message.append(msg);
            }
        }

        void reset(const std::string& name)
        {
            std::scoped_lock<std::mutex> lock(mtx);
            auto& e = entries.at(name);
            if (e.present)
            {
                e.updated = true;
                e.present = false;
                e.message.clear();
            }
        }

        bool ok(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return !entries.at(name).present;
        }

        std::string getMessage(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return entries.at(name).message;
        }

        bool isUpdated(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return isUpdatedImpl(entries.at(name));
        }

        std::string buildStatusMessage(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return name + " error: " + entries.at(name).message;
        }

        std::string rawStatusMessage(const std::string& name) const
        {
            std::scoped_lock<std::mutex> lock(mtx);
            return entries.at(name).message;
        }

        mutable std::mutex mtx;
        std::unordered_map<std::string, Entry> entries;
    };

    inline void Error::set(std::string msg)
    {
        if (auto c = container.lock())
            c->set(name, std::move(msg));
    }

    inline void Error::add(const std::string& msg)
    {
        if (auto c = container.lock())
            c->add(name, msg);
    }

    inline void Error::reset()
    {
        if (auto c = container.lock())
            c->reset(name);
    }

    inline bool Error::ok() const
    {
        auto c = container.lock();
        return !c || c->ok(name);
    }

    inline std::string Error::getMessage() const
    {
        auto c = container.lock();
        return c ? c->getMessage(name) : std::string{};
    }

    inline bool Error::isUpdated() const
    {
        auto c = container.lock();
        return c && c->isUpdated(name);
    }

    inline const std::string& Error::getName() const
    {
        return name;
    }

    inline std::string Error::buildStatusMessage() const
    {
        auto c = container.lock();
        return c ? c->buildStatusMessage(name) : std::string{};
    }

    inline std::string Error::rawStatusMessage() const
    {
        auto c = container.lock();
        return c ? c->rawStatusMessage(name) : std::string{};
    }
}

END_NAMESPACE_OPENDAQ_MQTT_STREAMING_MODULE
