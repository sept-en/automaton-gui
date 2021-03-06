/*
 * Automaton Playground
 * Copyright (c) 2019 The Automaton Playground Authors.
 * Copyright (c) 2019 The automaton.network Authors.
 *
 * Automaton Playground is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * any later version.
 *
 * Automaton Playground is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Automaton Playground.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#include "../../JuceLibraryCode/JuceHeader.h"

#include <json.hpp>

#include "automaton/core/common/status.h"

using json = nlohmann::json;

class Config {
 public:
  Config();
  ~Config();

  automaton::core::common::status load();
  automaton::core::common::status save_to_local_file();

  void set_json(const std::string& field, const std::string& data);
  void set_string(const std::string& field, const std::string& data);
  void set_bool(const std::string& field, bool data);
  void set_number(const std::string& field, int64_t data);

  std::string get_json(const std::string& field) const;
  std::string get_string(const std::string& field) const;
  bool get_bool(const std::string& field) const;
  int64_t get_number(const std::string& field) const;

  std::string get_abi() const;

  File get_local_config_file();

  void lock();
  void unlock();

  JUCE_DECLARE_SINGLETON(Config, false)
 private:
  std::string contract_abi;
  json json_obj;

  CriticalSection critical_section;
};
