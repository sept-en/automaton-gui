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

#include <json.hpp>
#include <functional>

#include "ProposalsManager.h"
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

static uint64 getNumSlots(std::shared_ptr<eth_contract> contract, status* resStatus);
static std::vector<std::string> getOwners(std::shared_ptr<eth_contract> contract, uint64 numOfSlots, status* resStatus);
static uint64 getLastProposalId(std::shared_ptr<eth_contract> contract, status* resStatus);
static void voteWithSlot(std::shared_ptr<eth_contract> contract,
                         uint64 id, uint64 slot, uint64 choice,
                         status* resStatus);

static std::shared_ptr<eth_contract> getContract(status* resStatus) {
  const auto cd = AutomatonContractData::getInstance();
  const auto contract = eth_contract::get_contract(cd->contract_address);
  if (contract == nullptr)
    *resStatus = status::internal("Contract is NULL. Read appropriate contract data first.");

  return contract;
}


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


ProposalsManager::ProposalsManager()
  : m_model(std::make_shared<ProposalsModel>()) {
}

ProposalsManager::~ProposalsManager() {
  clearSingletonInstance();
}

bool ProposalsManager::fetchProposals() {
  Array<Proposal> proposals;
  AsyncTask task([&](AsyncTask* task) {
    auto& s = task->m_status;

    auto contract = getContract(&s);
    if (!s.is_ok())
      return false;

    m_model->clear(false);

    const auto lastProposalId = getLastProposalId(contract, &s);
    if (!s.is_ok())
      return false;

    // ballotBoxIDs initial value is 99, and the first proposal is at 100
    static const uint32_t PROPOSAL_START_ID = 100;
    for (int i = PROPOSAL_START_ID; i <= lastProposalId; ++i) {
      json jInput;
      jInput.push_back(i);
      std::string params = jInput.dump();

      s = contract->call("getProposal", params);

      if (!s.is_ok())
        return false;

      Proposal proposal(i, String(s.msg));

      s = contract->call("calcVoteDifference", params);
      if (!s.is_ok())
        return false;

      json j_output = json::parse(s.msg);
      const int approvalRating = std::stoi((*j_output.begin()).get<std::string>());
      if (!s.is_ok())
        return false;

      proposal.setApprovalRating(approvalRating);

      addProposal(proposal, false);
    }

    return true;
  }, "Fetching proposals...");

  if (task.runThread()) {
    auto& s = task.m_status;
    if (s.is_ok()) {
      notifyProposalsUpdated();
      return true;
    } else {
      AlertWindow::showMessageBoxAsync(
        AlertWindow::WarningIcon,
        "ERROR",
        String("(") + String(s.code) + String(") :") + s.msg);
    }
  } else {
    AlertWindow::showMessageBoxAsync(
      AlertWindow::WarningIcon,
      "Fetching proposals canceled!",
      "Current proposals data may be broken.");
  }

  return false;
}

void ProposalsManager::addProposal(const Proposal& proposal, bool sendNotification) {
  m_model->addItem(std::make_shared<Proposal>(proposal), sendNotification);
}

bool ProposalsManager::createProposal(Proposal::Ptr proposal, const std::string& contributor) {
  AsyncTask task([=](AsyncTask* task) {
    auto& s = task->m_status;

    const auto cd = AutomatonContractData::getInstance();
    const auto contract = getContract(&s);
    if (!s.is_ok())
      return false;

    task->setProgress(0.1);

    const auto lastProposalId = getLastProposalId(contract, &s);
    proposal->setId(lastProposalId + 1);
    task->setProgress(0.25);

    s = eth_getTransactionCount(cd->eth_url, cd->eth_address);
    const auto nonce = s.is_ok() ? s.msg : "0";
    if (!s.is_ok())
      return false;

    json jProposal;
    jProposal.push_back(contributor);
    jProposal.push_back(proposal->getTitle().toStdString());
    jProposal.push_back("google.com");
    jProposal.push_back("BA5EC0DE");
    jProposal.push_back(proposal->getLengthDays());
    jProposal.push_back(proposal->getNumPeriods());
    jProposal.push_back(proposal->getBudget());

    json jSignature;
    jSignature.push_back("address");
    jSignature.push_back("string");
    jSignature.push_back("string");
    jSignature.push_back("bytes");
    jSignature.push_back("uint256");
    jSignature.push_back("uint256");
    jSignature.push_back("uint256");

    std::stringstream txData;
    txData << "f8a1ff12" << bin2hex(encode(jSignature.dump(), jProposal.dump()));
    task->setProgress(0.5);

    eth_transaction transaction;
    transaction.nonce = nonce;
    transaction.gas_price = "1388";  // 5 000
    transaction.gas_limit = "5B8D80";  // 6M
    transaction.to = cd->contract_address.substr(2);
    transaction.value = "";
    transaction.data = txData.str();
    transaction.chain_id = "01";
    s = contract->call("createProposal", transaction.sign_tx(cd->private_key));

    if (!s.is_ok())
      return false;

    DBG("Call result: " << s.msg << "\n");
    proposal->setStatus(Proposal::Status::Started);
    task->setProgress(1.0);

    return true;
  }, "Creating proposal...");


  if (task.runThread()) {
    auto& s = task.m_status;
    if (s.is_ok()) {
      m_model->addItem(proposal);
      return true;
    } else {
      AlertWindow::showMessageBoxAsync(
        AlertWindow::WarningIcon,
        "ERROR",
        String("(") + String(s.code) + String(") :") + s.msg);
    }
  } else {
    AlertWindow::showMessageBoxAsync(
      AlertWindow::WarningIcon,
      "Canceled!",
      "Operation aborted.");
  }

  return false;
}

bool ProposalsManager::payForGas(Proposal::Ptr proposal, uint64 slotsToPay) {
  if (proposal->getId() <= 0) {
    AlertWindow::showMessageBoxAsync(
      AlertWindow::WarningIcon,
      "ERROR",
      "The proposal is not valid");
  }

  AsyncTask task([=](AsyncTask* task) {
    auto& s = task->m_status;

    const auto cd = AutomatonContractData::getInstance();
    const auto contract = getContract(&s);
    if (!s.is_ok())
      return false;

    task->setProgress(0.1);

    s = eth_getTransactionCount(cd->eth_url, cd->eth_address);
    const auto nonce = s.is_ok() ? s.msg : "0";
    if (!s.is_ok())
      return false;

    json jInput;
    jInput.push_back(proposal->getId());
    jInput.push_back(slotsToPay);

    json jSignature;
    jSignature.push_back("uint256");
    jSignature.push_back("uint256");

    std::stringstream txData;
    txData << "10075c50" << bin2hex(encode(jSignature.dump(), jInput.dump()));
    task->setProgress(0.5);

    eth_transaction transaction;
    transaction.nonce = nonce;
    transaction.gas_price = "1388";  // 5 000
    transaction.gas_limit = "5B8D80";  // 6M
    transaction.to = cd->contract_address.substr(2);
    transaction.value = "";
    transaction.data = txData.str();
    transaction.chain_id = "01";
    s = contract->call("payForGas", transaction.sign_tx(cd->private_key));

    if (!s.is_ok())
      return false;

    task->setProgress(1.0);

    return true;
  }, "Paying for gas....");


  if (task.runThread()) {
    auto& s = task.m_status;
    if (s.is_ok()) {
      return true;
    } else {
      AlertWindow::showMessageBoxAsync(
        AlertWindow::WarningIcon,
        "ERROR",
        String("(") + String(s.code) + String(") :") + s.msg);
    }
  } else {
    AlertWindow::showMessageBoxAsync(
     AlertWindow::WarningIcon,
     "Canceled!",
     "Operation aborted.");
  }

  return false;
}

void ProposalsManager::notifyProposalsUpdated() {
  m_model->notifyModelChanged();
}

static void voteWithSlot(std::shared_ptr<eth_contract> contract,
                         uint64 id, uint64 slot, uint64 choice,
                         status* resStatus) {
  const auto cd = AutomatonContractData::getInstance();
  const auto s = eth_getTransactionCount(cd->eth_url, cd->eth_address);
  *resStatus = s;
  const auto nonce = s.is_ok() ? s.msg : "0";
  if (!s.is_ok())
    return;

  json jInput;
  jInput.push_back(id);
  jInput.push_back(slot);
  jInput.push_back(choice);

  json jSignature;
  jSignature.push_back("uint256");
  jSignature.push_back("uint256");
  jSignature.push_back("uint8");

  std::stringstream txData;
  txData << "dea112a6" << bin2hex(encode(jSignature.dump(), jInput.dump()));

  eth_transaction transaction;
  transaction.nonce = nonce;
  transaction.gas_price = "1388";  // 5 000
  transaction.gas_limit = "5B8D80";  // 6M
  transaction.to = cd->contract_address.substr(2);
  transaction.value = "";
  transaction.data = txData.str();
  transaction.chain_id = "01";
  *resStatus = contract->call("castVote", transaction.sign_tx(cd->private_key));
}

static uint64 getNumSlots(std::shared_ptr<eth_contract> contract, status* resStatus) {
  auto s = contract->call("numSlots", "");
  *resStatus = s;
  if (!s.is_ok()) {
    std::cout << "ERROR: " << s.msg << std::endl;
    return 0;
  }

  json j_output = json::parse(s.msg);
  const uint64 slots_number = std::stoul((*j_output.begin()).get<std::string>());
  return slots_number;
}

static std::vector<std::string> getOwners(std::shared_ptr<eth_contract> contract,
                                          uint64 numOfSlots,
                                          status* resStatus) {
  json j_input;
  j_input.push_back(0);
  j_input.push_back(numOfSlots);
  const std::string params = j_input.dump();

  // Fetch owners.
  auto s = contract->call("getOwners", params);
  *resStatus = s;
  if (!s.is_ok())
    return std::vector<std::string>();

  // Parse owners.
  const auto j_output = json::parse(s.msg);
  std::vector<std::string> owners = (*j_output.begin()).get<std::vector<std::string>>();
  return owners;
}

static uint64 getLastProposalId(std::shared_ptr<eth_contract> contract, status* resStatus) {
  auto s = contract->call("proposalsData", "");
  *resStatus = s;
  if (!s.is_ok())
    return 0;

  const json ballotBoxJson = json::parse(s.msg);

  if (ballotBoxJson.size() >= 3) {
    const uint64 lastProposalId = std::stoul(ballotBoxJson[3].get<std::string>());
    return lastProposalId;
  }

  return 0;
}

bool ProposalsManager::castVote(Proposal::Ptr proposal, uint64 choice) {
  if (proposal->getId() <= 0) {
    AlertWindow::showMessageBoxAsync(
      AlertWindow::WarningIcon,
      "ERROR",
      "The proposal is not valid");
  }

  AsyncTask task([=](AsyncTask* task) {
    auto& s = task->m_status;

    auto contract = getContract(&s);
    if (!s.is_ok())
      return false;

    task->setProgress(0.1);

    const auto numOfSlots = getNumSlots(contract, &s);

    if (!s.is_ok())
      return false;

    const auto owners = getOwners(contract, numOfSlots, &s);

    if (!s.is_ok())
      return false;

    const auto cd = AutomatonContractData::getInstance();
    const String callAddress = cd->eth_address.substr(2);

    bool isOwnerForAnySlot = false;
    for (uint64 slot = 0; slot < owners.size(); ++slot) {
      String owner = owners[slot];
      if (owner.equalsIgnoreCase(callAddress)) {
        isOwnerForAnySlot = true;
        voteWithSlot(contract, proposal->getId(), slot, choice, &s);

        if (!s.is_ok() || task->threadShouldExit())
          return false;
      }

      task->setProgress(slot / static_cast<double>(numOfSlots));
    }

    if (!isOwnerForAnySlot) {
      s = status::internal("You own no single slot. Voting is impossible");
      return false;
    }

    return true;
  }, "Voting....");


  if (task.runThread()) {
    auto& s = task.m_status;
    if (s.is_ok()) {
      return true;
    } else {
      AlertWindow::showMessageBoxAsync(
        AlertWindow::WarningIcon,
        "ERROR",
        String("(") + String(s.code) + String(") :") + s.msg);
    }
  } else {
    AlertWindow::showMessageBoxAsync(
      AlertWindow::WarningIcon,
      "Canceled!",
      "Operation aborted.");
  }

  return false;
}

JUCE_IMPLEMENT_SINGLETON(ProposalsManager);
