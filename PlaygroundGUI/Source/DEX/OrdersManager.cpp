/*
 * Automaton Playground
 * Copyright (c) 2020 The Automaton Authors.
 * Copyright (c) 2020 The automaton.network Authors.
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

#include "OrdersManager.h"
#include "OrdersModel.h"
#include "../Data/AutomatonContractData.h"
#include "automaton/core/interop/ethereum/eth_contract_curl.h"
#include "automaton/core/interop/ethereum/eth_transaction.h"
#include "automaton/core/interop/ethereum/eth_helper_functions.h"
#include "automaton/core/common/status.h"

using automaton::core::common::status;
using automaton::core::interop::ethereum::dec_to_32hex;
using automaton::core::interop::ethereum::eth_transaction;
using automaton::core::interop::ethereum::eth_getTransactionCount;
using automaton::core::interop::ethereum::eth_getTransactionReceipt;
using automaton::core::interop::ethereum::encode;
using automaton::core::interop::ethereum::eth_contract;
using automaton::core::io::bin2hex;
using automaton::core::io::dec2hex;
using automaton::core::io::hex2dec;

class AsyncTask : public ThreadWithProgressWindow {
 public:
  AsyncTask(std::function<bool(AsyncTask*)> fun, const String& title)
    : ThreadWithProgressWindow(title, true, true)
    , m_status(status::ok()) {
    m_fun = fun;
  }

  void run() {
    m_fun(this);
  }

  status m_status;

 private:
  std::function<bool(AsyncTask*)> m_fun;
};

static std::shared_ptr<eth_contract> getContract(status* resStatus) {
  const auto cd = AutomatonContractData::getInstance();
  const auto contract = eth_contract::get_contract(cd->contract_address);
  if (contract == nullptr)
    *resStatus = status::internal("Contract is NULL. Read appropriate contract data first.");

  return contract;
}

OrdersManager::OrdersManager() {
  m_model = std::make_shared<OrdersModel>();
}

OrdersManager::~OrdersManager() {
  clearSingletonInstance();
}

std::shared_ptr<OrdersModel> OrdersManager::getModel() {
  return m_model;
}

static uint64 getNumOrders(std::shared_ptr<eth_contract> contract, status* resStatus) {
  *resStatus = contract->call("getOrdersLength", "");
  if (!resStatus->is_ok())
    return 0;

  const json jsonData = json::parse(resStatus->msg);
  const uint64 ordersLength = std::stoul((*jsonData.begin()).get<std::string>());
  return ordersLength;
}

bool OrdersManager::fetchOrders() {
  Array<Order::Ptr> orders;
  AsyncTask task([&](AsyncTask* task) {
    auto& s = task->m_status;

    auto contract = getContract(&s);
    if (!s.is_ok())
      return false;

    m_model->clear(false);

    const auto numOfOrders = getNumOrders(contract, &s);
    if (!s.is_ok())
      return false;

    for (int i = 1; i <= numOfOrders; ++i) {
      json jInput;
      jInput.push_back(i);

      s = contract->call("getOrder", jInput.dump());

      if (!s.is_ok())
        return false;

      auto order = std::make_shared<Order>(String(s.msg));
      orders.add(order);
    }

    return true;
  }, "Fetching orders...");

  if (task.runThread()) {
    auto& s = task.m_status;
    if (s.is_ok()) {
      m_model->clear(false);
      m_model->addItems(orders, true);
      return true;
    } else {
      AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                       "ERROR",
                                       String("(") + String(s.code) + String(") :") + s.msg);
    }
  } else {
    AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                     "Fetching orders canceled!",
                                     "Current orders data may be broken.");
  }

  return false;
}

JUCE_IMPLEMENT_SINGLETON(OrdersManager);
