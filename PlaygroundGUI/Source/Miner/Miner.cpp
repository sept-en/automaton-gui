/*
 * Automaton Playground
 * Copyright (c) 2019 The Automaton Authors.
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

#include <json.hpp>

#include "Miner.h"
#include "../Data/AutomatonContractData.h"
#include "Utils/Utils.h"

#include "automaton/core/crypto/cryptopp/Keccak_256_cryptopp.h"
#include "automaton/core/interop/ethereum/eth_contract_curl.h"
#include "automaton/core/interop/ethereum/eth_helper_functions.h"
#include "automaton/core/interop/ethereum/eth_transaction.h"
#include "automaton/core/io/io.h"
#include "automaton/tools/miner/miner.h"
#include <secp256k1_recovery.h>
#include <secp256k1.h>

using automaton::core::common::status;
using automaton::core::crypto::cryptopp::Keccak_256_cryptopp;
using automaton::core::interop::ethereum::eth_contract;
using automaton::core::io::bin2hex;
using automaton::core::io::dec2hex;
using automaton::core::io::hex2bin;
using automaton::core::io::hex2dec;
using automaton::tools::miner::gen_pub_key;
using automaton::tools::miner::mine_key;
using automaton::tools::miner::sign;


static std::string get_pub_key_x(const unsigned char* priv_key) {
  secp256k1_context* context = secp256k1_context_create(SECP256K1_CONTEXT_SIGN | SECP256K1_CONTEXT_VERIFY);
  secp256k1_pubkey* pubkey = new secp256k1_pubkey();

  if (!secp256k1_ec_pubkey_create(context, pubkey, priv_key)) {
    LOG(WARNING) << "Invalid priv_key " << bin2hex(std::string(reinterpret_cast<const char*>(priv_key), 32));
    delete pubkey;
    secp256k1_context_destroy(context);
    return "";
  }

  unsigned char pub_key_serialized[65];
  size_t outLen = 65;
  secp256k1_ec_pubkey_serialize(context, pub_key_serialized, &outLen, pubkey, SECP256K1_EC_UNCOMPRESSED);
  std::string pub_key_uncompressed(reinterpret_cast<char*>(pub_key_serialized), 65);
  delete pubkey;
  secp256k1_context_destroy(context);
  return std::string(reinterpret_cast<char*>(pub_key_serialized+1), 32);
}

class MinerThread: public Thread {
 public:
  uint64 keysMined;
  int64 startTime;

  Miner* owner;

  MinerThread(Miner* _owner) : Thread("Miner Thread"), keysMined(0), owner(_owner) {
  }

  void run() override {
    keysMined = 0;
    startTime = Time::getCurrentTime().toMilliseconds();

    unsigned char mask[32];
    unsigned char difficulty[32];
    unsigned char pk[32];

    memcpy(mask, owner->getMask(), 32);
    memcpy(difficulty, owner->getDifficulty(), 32);

    while (!threadShouldExit()) {
      unsigned int keys_generated = mine_key(mask, difficulty, pk);
      if (owner) {
        owner->processMinedKey(std::string(reinterpret_cast<const char*>(pk), 32), keys_generated);
      }
      // wait(20);
    }
  }
};

void Miner::addMinerThread() {
  Thread * miner = new MinerThread(this);
  miner->startThread();
  miners.add(miner);
}

void Miner::stopMining() {
  for (int i = 0; i < miners.size(); i++) {
    miners[i]->stopThread(3000);
  }
  miners.clear(true);
}

void Miner::processMinedKey(std::string _pk, int keys_generated) {
  auto cd = m_accountData->getContractData();
  ScopedLock lock(cd->m_criticalSection);

  total_keys_generated += abs(keys_generated);
  if (keys_generated <= 0) {
    return;
  }
  mined_slot ms;
  std::string x = get_pub_key_x(reinterpret_cast<const unsigned char *>(_pk.c_str()));
  ms.public_key = x;
  CryptoPP::Integer bn_x((bin2hex(x) + "h").c_str());
  ms.slot_index = uint32_t(bn_x % totalSlots);
  for (int i = 0; i < 32; i++) {
    x[i] ^= mask[i];
  }
  if (x <= std::string(reinterpret_cast<char*>(difficulty), 32)) {
    return;
  }
  if (x <= cd->m_slots[ms.slot_index].difficulty) {
    return;
  }
  ms.difficulty = x;
  ms.private_key = _pk;
  mined_slots.push_back(ms);
}

class TableSlots: public TableListBox, TableListBoxModel {
 public:
  Miner* owner;
  AutomatonContractData::Ptr contractData;

  TableSlots(Miner * _owner, AutomatonContractData::Ptr _contractData) : owner(_owner), contractData(_contractData) {
    // Create our table component and add it to this component..
    // addAndMakeVisible(table);
    setModel(this);

    // give it a border
    setColour(ListBox::outlineColourId, Colours::grey);
    setOutlineThickness(1);
    setRowHeight(16);

    struct column {
      String name;
      int ID;
      int width;
    };

    column columns[] = {
      {"Slot", 1, 40},
      {"Difficulty", 2, 200},
      {"Owner", 3, 350},
      // {"Private Key", 4, 400},
      {"", 0, 0},
    };

    // Add some columns to the table header, based on the column list in our database..
    for (int i = 0; columns[i].ID != 0; i++) {
      getHeader().addColumn(columns[i].name,
                                  columns[i].ID,
                                  columns[i].width,
                                  50, 400,
                                  TableHeaderComponent::defaultFlags);
    }

    // we could now change some initial settings..
    getHeader().setSortColumnId(1, true);  // sort forwards by the ID column
    getHeader().setColumnVisible(7, false);  // hide the "length" column until the user shows it

    // un-comment this line to have a go of stretch-to-fit mode
    // getHeader().setStretchToFitActive(true);

    setMultipleSelectionEnabled(false);
  }

  // This is overloaded from TableListBoxModel, and must return the total number of rows in our table
  int getNumRows() override {
    ScopedLock lock(contractData->m_criticalSection);
    return static_cast<int>(contractData->m_slots.size());
  }

  void selectedRowsChanged(int) override {
    // owner->createSignature();
  }

  // This is overloaded from TableListBoxModel, and should fill in the background of the whole row
  void paintRowBackground(Graphics& g, int rowNumber, int /*width*/, int /*height*/, bool rowIsSelected) override {
    auto alternateColour = getLookAndFeel().findColour(ListBox::backgroundColourId)
                                           .interpolatedWith(getLookAndFeel().findColour(ListBox::textColourId), 0.03f);
    if (rowIsSelected)
        g.fillAll(alternateColour.brighter(0.2f));
    else if (rowNumber % 2)
        g.fillAll(alternateColour);
  }

  // This is overloaded from TableListBoxModel, and must paint any cells that aren't using custom
  // components.
  void paintCell(Graphics& g, int rowNumber, int columnId,
                int width, int height, bool /*rowIsSelected*/) override {
    ScopedLock lock(contractData->m_criticalSection);

    g.setColour(getLookAndFeel().findColour(ListBox::textColourId));
    g.setFont(font);

    String text = "";
    switch (columnId) {
      case 1: {
        char b[2];
        b[0] = (rowNumber >> 8) & 0xFF;
        b[1] = rowNumber & 0xFF;
        std::string x(b, 2);
        text = bin2hex(x);
        break;
      }
      case 2: {
        text = bin2hex(contractData->m_slots[rowNumber].difficulty);
        break;
      }
      case 3: {
        text = "0x" + contractData->m_slots[rowNumber].owner;
        break;
      }
      case 4: {
        text = "N/A";
      }
    }
    g.drawText(text, 2, 0, width - 4, height, Justification::centredLeft, true);

    g.setColour(getLookAndFeel().findColour(ListBox::backgroundColourId));
    g.fillRect(width - 1, 0, 1, height);
  }

 private:
  // TableListBox table;     // the table component itself
  Font font  { 12.0f };

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TableSlots)
};

static unsigned int leading_bits_char(char x) {
  unsigned int c = 0;
  while (x & 0x80) {
    c++;
    x <<= 1;
  }
  return c;
}

using automaton::core::io::bin2hex;

unsigned int leading_bits(const std::string& s) {
  unsigned int result = 0;
  for (unsigned int i = 0; i < s.size(); i++) {
    unsigned int lb = leading_bits_char(s[i]);
    result += lb;
    if (lb != 8) {
      break;
    }
  }
  return result;
}

static String sepitoa(uint64 n, bool lz = false) {
  if (n < 1000) {
    if (!lz) {
      return String(n);
    } else {
      return
          ((n < 100) ? String("0") : String("")) +
          ((n < 10) ? String("0") : String("")) +
          String(n);
    }
  } else {
    return sepitoa(n / 1000, lz) + "," + sepitoa(n % 1000, true);
  }
}

//==============================================================================
Miner::Miner(Account::Ptr accountData) : m_accountData(accountData) {
  private_key = m_accountData->getPrivateKey();
  eth_address = m_accountData->getAddress();

  int y = 0;

  LBL(String(__DATE__) + " " + String(__TIME__), 0, 0, 140, 20);

/*
  y += 50;
  LBL("RPC Server:", 20, y, 100, 24);
  txtRpcServer = TXT("RPC", 120, y, 500, 24);
  txtRpcServer->setText("HTTP://127.0.0.1:7545");

  y += 30;
  LBL("Contract:" , 20, y, 100, 24);
  txtContract = TXT("CONTR", 120, y, 500, 24);
  txtContract->setText("0xdb95dbb1be9e82f8caaa90f0feacabb5e7fa36ab");

  y += 30;
  btnContract = TB("Read Contract", 120, y, 120, 24);
*/

  y += 50;
  LBL("Slots: ", 20, y, 100, 24);
  txtSlotsNum = TXT("SLOTS", 120, y, 100, 24);
  txtSlotsNum->setInputRestrictions(5, "0123456789");
  txtSlotsNum->setReadOnly(true);

  // y += 30;
  // LBL("Mask: ", 20, y, 100, 24);
  // txtMask = TXT("MASK", 120, y, 600, 24);
  // txtMask->setInputRestrictions(78, "0123456789");

  y += 30;
  LBL("Mask (Hex):", 20, y, 100, 24);
  txtMaskHex = TXT("MASKHEX", 120, y, 600, 24);
  txtMaskHex->setInputRestrictions(64, "0123456789abcdefABCDEF");
  txtMaskHex->setReadOnly(true);

  y += 30;
  LBL("Min Difficulty:", 20, y, 100, 24);
  // txtMinDifficulty = TXT("MINDIFF", 120, y, 25, 24);
  // txtMinDifficulty->setInputRestrictions(2, "0123456789");
  txtMinDifficultyHex = TXT("MINDIFFHEX", 120, y, 600, 24);
  txtMinDifficultyHex->setReadOnly(true);

  y += 50;
  TB("Add Miner", 120, y, 80, 24);
  TB("Stop Miners", 220, y, 80, 24);
  txtMinerInfo = TXT("MINFO", 320, y, 400, 80);
  txtMinerInfo->setText("Not running.");
  txtMinerInfo->setReadOnly(true);
  txtMinerInfo->setMultiLine(true);

  y += 100;
  LBL("Validator Slots: ", 20, y, 300, 24);
  LBL("Claim Slot Parameters:", 650, y, 300, 24);

  y += 30;
  tblSlots = new TableSlots(this, m_accountData->getContractData());
  tblSlots->setBounds(20, y, 600, 300);
  addComponent(tblSlots);
  txtClaim = TXT("CLAIM", 650, y, 600, 300);
  txtClaim->setReadOnly(true);

  updateContractData();

  /*
  CryptoPP::byte buf[1024] = {0};
  CryptoPP::Integer n("53272589901149737477561849970166322707710816978043543010898806474236585144509");
  n = -n - 1;
  size_t min_size = n.MinEncodedSize(CryptoPP::Integer::SIGNED);
  std::cout << min_size << std::endl;
  n.Encode(buf, min_size, CryptoPP::Integer::SIGNED);
  std::string bn(reinterpret_cast<char*>(buf), min_size);
  // std::cout << bin2hex(bn) << std::endl;

  y += 30;
  LBL("Mined Keys: ", 20, y, 80, 24);
  auto keys = TXT("KEYS", 100, y, 550, 400);
  keys->setMultiLine(true);
  keys->setReturnKeyStartsNewLine(true);
  addr->setInputRestrictions(0, "0123456789abcdefABCDEF");

  y += 410;
  TB("Claim", 100, y, 80, 30);
  TB("Start Miner", 200, y, 80, 30);
  */

  startTimer(1000);
}

void Miner::updateContractData() {
  auto cd = m_accountData->getContractData();
  ScopedLock lock(cd->m_criticalSection);
  setSlotsNumber(cd->m_slotsNumber);
  setMaskHex(cd->m_mask);
  setMinDifficultyHex(cd->m_minDifficulty);
}

Miner::~Miner() {
}

// TODO(asen): Fix this.
/*
void Miner::setMask(std::string _mask) {
  CryptoPP::Integer m(_mask.c_str());
  m.Encode(mask, 32);
  txtMask->setText(_mask);
  // const unsigned int UPPER = (1 << 31);
  // txtMaskHex->setText(CryptoPP::IntToString(m, UPPER | 16), false);
  txtMaskHex->setText(bin2hex(std::string(reinterpret_cast<char*>(mask), 32)));
}
*/

void Miner::setMaskHex(std::string _mask) {
  auto mask_bin = hex2bin(_mask);
  memcpy(mask, mask_bin.c_str(), sizeof(mask));
  txtMaskHex->setText(_mask);
}

void Miner::setMinDifficultyHex(std::string _minDifficulty) {
  auto diff_bin = hex2bin(_minDifficulty);
  memcpy(difficulty, diff_bin.c_str(), sizeof(difficulty));
  txtMinDifficultyHex->setText(_minDifficulty, false);
}

void Miner::setMinerAddress(std::string _address) {
  if (_address.substr(0, 2) == "0x") {
    _address = _address.substr(2);
  }
  _address += "h";
  CryptoPP::Integer a(_address.c_str());

  a.Encode(minerAddress, 32);
  const unsigned int UPPER = (1 << 31);
  // txtMinerAddress->setText("0x" + IntToString(a, UPPER | 16), false);
}

void Miner::setSlotsNumber(int _slotsNum) {
  _slotsNum = jmax(0, jmin(65536, _slotsNum));
  totalSlots = _slotsNum;
  txtSlotsNum->setText(String(totalSlots), false);
  repaint();
  tblSlots->updateContent();
}

void Miner::initSlots() {
  unsigned char difficulty[32];
  mined_slots.clear();

  memset(mask, 0, 32);
  memset(difficulty, 0, 32);
  difficulty[0] = 0xff;
}

void Miner::textEditorTextChanged(TextEditor & txt) {
  // if (txtMask == &txt) {
  //   setMask(txtMask->getText().toStdString());
  // }
  // if (txtMinDifficulty == &txt) {
  //   auto minDiff = jmax(0, jmin(48, txtMinDifficulty->getText().getIntValue()));
  //   setMinDifficulty(minDiff);
  // }
  if (txtSlotsNum == &txt) {
    setSlotsNumber(txtSlotsNum->getText().getIntValue());
  }
}

void Miner::buttonClicked(Button* btn) {
  auto txt = btn->getButtonText();
  if (txt == "Add Miner") {
    addMinerThread();
  } else if (txt == "Stop Miners") {
    stopMining();
  }
  repaint();
}

void Miner::paint(Graphics& g) {
  g.setColour(Colours::white);
  g.setFont(32.0f);
  g.drawText("King of the Hill Miner", 0, 0, getWidth(), 40, Justification::centred);
}

void Miner::resized() {
}

void Miner::update() {
  auto cur_time = Time::getCurrentTime().toMilliseconds();
  auto delta = cur_time - last_time;
  auto delta_keys = total_keys_generated - last_keys_generated;
  if (total_keys_generated > 0) {
    txtMinerInfo->setText(
      "Total keys generated: " + String(total_keys_generated) + "\n" +
      "Mining power: " + String(delta_keys * 1000 / delta) + " keys/s\n" +
      "Mined unclaimed keys: " + String(mined_slots.size()) + "\n" +
      "Active miners: " + String(miners.size()));
  }
  last_time = cur_time;
  last_keys_generated = total_keys_generated;
}

void Miner::createSignature() {
/*
  txtClaim->setText("");
  int s = tblSlots->getSelectedRow();
  if (s < 0 || s >= slots.size()) {
    return;
  }
  auto& slot = slots[s];
  std::string priv_key = slot.private_key;
  if (priv_key.size() != 32) {
    return;
  }

  std::string pub_key = gen_pub_key((unsigned char*)priv_key.c_str());
  std::string sig =
      sign(reinterpret_cast<const unsigned char*>(priv_key.c_str()),
           reinterpret_cast<const unsigned char*>(slot.owner.c_str()));

  std::stringstream ss;
  ss
      << "koh.claimSlot('0x" << bin2hex(pub_key.substr(0, 32))
      << "', '0x" << bin2hex(pub_key.substr(32, 32))
      << "', '0x" << bin2hex(sig.substr(64, 1))
      << "', '0x" << bin2hex(sig.substr(0, 32))
      << "', '0x" << bin2hex(sig.substr(32, 32))
      << "')" << std::endl;

  txtClaim->setText(
    "Signature: \n"
    "PubKeyX = 0x" + bin2hex(pub_key.substr(0, 32)) + " \n"
    "PubKeyY = 0x" + bin2hex(pub_key.substr(32, 32)) + " \n"
    "R = 0x" + bin2hex(sig.substr(0, 32)) + " \n"
    "S = 0x" + bin2hex(sig.substr(32, 32)) + " \n"
    "V = 0x" + bin2hex(sig.substr(64, 1)) + " \n"
    + ss.str());
*/
}

class ClaimSlotThread: public ThreadWithProgressWindow {
 public:
  AutomatonContractData::Ptr cd;
  std::string private_key;
  std::string mined_key;
  std::string address;
  std::string bin_address;
  status s;

  ClaimSlotThread(AutomatonContractData::Ptr _cd, std::string _private_key, std::string _mined_key):
      ThreadWithProgressWindow("Claim Slot Thread", true, true),
      cd(_cd),
      private_key(_private_key),
      mined_key(_mined_key),
      s(status::ok()) {
    address = Utils::gen_ethereum_address(private_key);
    bin_address = hex2bin(std::string(24, '0') + address.substr(2));
  }

  void run() override {
    ScopedLock lock(cd->m_criticalSection);

    setStatusMessage("Generating signature...");

    // Generate signature.
    std::string pub_key = gen_pub_key(reinterpret_cast<const unsigned char *>(mined_key.c_str()));
    std::string sig = sign(
        reinterpret_cast<const unsigned char *>(mined_key.c_str()),
        reinterpret_cast<const unsigned char *>(bin_address.c_str()));

    int32_t v = sig[64];
    json jInput;
    jInput.push_back(bin2hex(pub_key.substr(0, 32)));  // public key X
    jInput.push_back(bin2hex(pub_key.substr(32, 32)));  // public key Y
    jInput.push_back(std::to_string(v));
    jInput.push_back(bin2hex(sig.substr(0, 32)));  // R
    jInput.push_back(bin2hex(sig.substr(32, 32)));  // S

    setStatusMessage("Claiming slot...");
    s = cd->call("claimSlot", jInput.dump(), private_key);
    if (!s.is_ok()) {
      return;
    }
  }
};

void Miner::claimMinedSlots() {
  if (mined_slots.size() > 0) {
    auto ms = mined_slots.back();
    mined_slots.pop_back();
    ClaimSlotThread t(m_accountData->getContractData(), private_key, ms.private_key);
    if (!t.runThread(9)) {
      AlertWindow::showMessageBoxAsync(AlertWindow::WarningIcon,
                                       "Claim slot failed!",
                                       t.s.msg);
    }
  }
}

void Miner::timerCallback() {
  claimMinedSlots();
  updateContractData();
  update();
  repaint();
  for (auto c : components) {
    c->repaint();
  }
}
