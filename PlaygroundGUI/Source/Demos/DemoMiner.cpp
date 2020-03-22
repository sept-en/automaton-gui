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

#include "DemoMiner.h"

static unsigned int leading_bits_char(char x) {
  unsigned int c = 0;
  while (x & 0x80) {
    c++;
    x <<= 1;
  }
  return c;
}

using automaton::core::io::bin2hex;

static unsigned int leading_bits(const std::string& s) {
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

static Colour HSV(double h, double s, double v) {
  double hh, p, q, t, ff;
  int64 i;
  double r, g, b;

  if (s <= 0.0) {
    r = v;
    g = v;
    b = v;
    return Colour(uint8(r*255), uint8(g*255), uint8(b*255));
  }

  hh = h;
  if (hh >= 360.0) hh = 0.0;
  hh /= 60.0;
  i = static_cast<int64>(hh);
  ff = hh - i;
  p = v * (1.0 - s);
  q = v * (1.0 - (s * ff));
  t = v * (1.0 - (s * (1.0 - ff)));

  switch (i) {
  case 0:
    r = v;
    g = t;
    b = p;
    break;
  case 1:
    r = q;
    g = v;
    b = p;
    break;
  case 2:
    r = p;
    g = v;
    b = t;
    break;
  case 3:
    r = p;
    g = q;
    b = v;
    break;
  case 4:
    r = t;
    g = p;
    b = v;
    break;
  case 5:
  default:
    r = v;
    g = p;
    b = q;
    break;
  }
  return Colour(uint8(r * 255), uint8(g * 255), uint8(b * 255));
}

//==============================================================================
DemoMiner::DemoMiner() {
  LBL("Mining Power Multiplier", 25, 45, 240, 20);
  StringArray btxtSpeed;
  btxtSpeed.add(TRANS("1x"));
  btxtSpeed.add(TRANS("16x"));
  btxtSpeed.add(TRANS("256x"));
  btxtSpeed.add(TRANS("4Kx"));
  btxtSpeed.add(TRANS("64Kx"));
  btxtSpeed.add(TRANS("1Mx"));
  GTB(100, 0, btxtSpeed, 25, 70, 40, 20);

  LBL("Number of Bootstrap Validator Slots", 25, 105, 240, 20);
  StringArray btxtSlotsNum;
  btxtSlotsNum.add(TRANS("256"));
  btxtSlotsNum.add(TRANS("1K"));
  btxtSlotsNum.add(TRANS("4K"));
  btxtSlotsNum.add(TRANS("16K"));
  btxtSlotsNum.add(TRANS("64K"));
  GTB(101, 1, btxtSlotsNum, 25, 130, 48, 20);

  // LBL("Mining Power (Keys/sec)", 25, 165, 240, 20);
  // StringArray btxtKps;
  // btxtKps.add(TRANS("1MKps"));
  // btxtKps.add(TRANS("10MKps"));
  // btxtKps.add(TRANS("100MKps"));
  // btxtKps.add(TRANS("1GKps"));
  // btxtKps.add(TRANS("10GKps"));
  // GTB(102, 2, btxtKps, 25, 190, 48, 20);

  LBL("Dishonest Mining Power %", 25, 165, 240, 20);
  StringArray btxtKps;
  btxtKps.add(TRANS("0%"));
  btxtKps.add(TRANS("1%"));
  btxtKps.add(TRANS("25%"));
  btxtKps.add(TRANS("50%"));
  btxtKps.add(TRANS("75%"));
  btxtKps.add(TRANS("100%"));
  GTB(102, 0, btxtKps, 25, 190, 40, 20);

  LBL("Validator Slots", 25, 225, 240, 20);
  LBL("Slots Difficulty Histogram", 325, 225, 240, 20);

  TB("Restart", 300, 60, 80, 20);
  TB("Pause", 400, 60, 80, 20);
}

DemoMiner::~DemoMiner() {
}

void DemoMiner::buttonClicked(Button* btn) {
  auto txt = btn->getButtonText();
  if (txt == "256") {
    m = 16;
    n = 16;
    sz = 16;
    gap = 1;
    reward_per_period = 5120000;
  }
  if (txt == "1K") {
    m = 32;
    n = 32;
    sz = 8;
    gap = 1;
    reward_per_period = 1280000;
  }
  if (txt == "4K") {
    m = 64;
    n = 64;
    sz = 4;
    gap = 1;
    reward_per_period = 320000;
  }
  if (txt == "16K") {
    m = 128;
    n = 128;
    sz = 2;
    gap = 0;
    reward_per_period = 80000;
  }
  if (txt == "64K") {
    m = 256;
    n = 256;
    sz = 1;
    gap = 0;
    reward_per_period = 20000;
  }
  if (txt == "1x") {
    mask1 = 0x00;
    mask2 = 0x00;
    mask3 = 0x00;
  }
  if (txt == "16x") {
    mask1 = 0xF0;
    mask2 = 0x00;
    mask3 = 0x00;
  }
  if (txt == "256x") {
    mask1 = 0xFF;
    mask2 = 0x00;
    mask3 = 0x00;
  }
  if (txt == "4Kx") {
    mask1 = 0xFF;
    mask2 = 0xF0;
    mask3 = 0x00;
  }
  if (txt == "64Kx") {
    mask1 = 0xFF;
    mask2 = 0xFF;
    mask3 = 0x00;
  }
  if (txt == "1Mx") {
    mask1 = 0xFF;
    mask2 = 0xFF;
    mask3 = 0xF0;
  }
  if (txt == "1MKps") {
    mining_power = 1;
  }
  if (txt == "10MKps") {
    mining_power = 10;
  }
  if (txt == "100MKps") {
    mining_power = 100;
  }
  if (txt == "1GKps") {
    mining_power = 1000;
  }
  if (txt == "10GKps") {
    mining_power = 10000;
  }
  if (txt == "0%") {
    mining_power = 0;
  }
  if (txt == "1%") {
    mining_power = 1;
  }
  if (txt == "25%") {
    mining_power = 25;
  }
  if (txt == "50%") {
    mining_power = 50;
  }
  if (txt == "75%") {
    mining_power = 75;
  }
  if (txt == "100%") {
    mining_power = 100;
  }
  if (txt == "Pause") {
    stopTimer();
    btn->setButtonText("Resume");
  }
  if (txt == "Resume") {
    startTimer(300);
    btn->setButtonText("Pause");
  }
  if (txt == "Restart") {
    t = 0;
    tx_count = 0;
    total_balance = 0;
    total_supply = 0;

    max_leading_bits = initial_difficulty_bits;

    for (int x = 0; x < 256; x++) {
      for (int y = 0; y < 256; y++) {
        slots[x][y].diff = std::string(32, 0);
        slots[x][y].bits = 0;
        slots[x][y].owner = 0;
        slots[x][y].tm = 0;
      }
    }

    startTimer(300);
  }
  repaint();
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

static unsigned int my_seed;

void DemoMiner::paint(Graphics& g) {
  int k = sz;
  int k2 = gap;
  unsigned int slots_owned = 0;

  g.setColour(Colours::white);
  g.drawRect(19 - k2, 249 - k2, m * k + k2 + 2, n * k + k2 + 2);

  unsigned int lb_hist[256] = {0};

  min_leading_bits = 257;
  for (int x = 0; x < m; x++) {
    for (int y = 0; y < n; y++) {
      lb_hist[slots[x][y].bits]++;
      if (slots[x][y].bits < min_leading_bits) {
        min_leading_bits = slots[x][y].bits;
      }
    }
  }

  int xx = 350;
  int yy = 260;
  for (int i = 255; i >= 0; i--) {
    if (lb_hist[i] == 0) {
      continue;
    }
    double lb = 1.0 * (i - min_leading_bits + 1) / (max_leading_bits - min_leading_bits + 1);
    g.setColour(HSV(200, 1.0 - lb, 0.5 + 0.4 * lb));
    // g.setColour(Colour::fromHSV(1.0 * (i - min_leading_bits) / (max_leading_bits - min_leading_bits), 1.0f, 1.0f, 1.0f));  // NOLINT
    for (int j = 0; j < i; j++) {
      g.fillRect(xx + 8 * j, yy + 2, 7, 7);
    }
    g.drawText(sepitoa(lb_hist[i]), xx - 60, yy, 50, 12, Justification::centredRight);
    g.drawText(sepitoa(i), xx + 8 * (i+1), yy, 50, 12, Justification::centredLeft);
    g.setColour(Colours::white);
    g.fillRect(xx, yy + 5, lb_hist[i] / m, 1);
    yy += 12;
  }

  for (int x = 0; x < m; x++) {
    for (int y = 0; y < n; y++) {
      // int i = y * 256 + x;
      double lb = 1.0 * (slots[x][y].bits - min_leading_bits) / (max_leading_bits - min_leading_bits);
      slots[x][y].bg = HSV(slots[x][y].owner ? 30 : 200, 1.0 - lb, 0.5 + 0.4 * lb);
      if (slots[x][y].bits == 0) {
        slots[x][y].bg = Colours::black;
      }
      g.setColour(slots[x][y].bg);
      g.fillRect(20 + x * k, 250 + y * k, k - k2, k - k2);
      if (slots[x][y].owner == 1) {
        slots_owned++;
        g.setColour(Colours::red);
        g.drawRect(19 + x * k, 249 + y * k, k + 1, k + 1);
      }
    }
  }

  double coeff = (1.0 - pow(total_supply, 2.0) / pow(supply_cap, 2.0));

  g.setColour(Colours::white);
  g.drawMultiLineText(
      " Transactions: " + sepitoa(tx_count) + "\n" +
      " coeff: " + String(coeff) + "\n" +
      " Slots owned: " + String(slots_owned) + "\n" +
      " My Balance: " + sepitoa(total_balance) + " AUTO\n" +
      " Total Supply: " + sepitoa(total_supply) + " AUTO\n" +
      " Supply Cap: " + sepitoa(supply_cap) + " AUTO\n" +
      " Treasury: " + sepitoa(total_supply / 2) + " AUTO",
      300, 100, 500, Justification::left);

  uint32 total_slots = m * n;
  uint32 honest_slots = total_slots - slots_owned;

  g.drawMultiLineText(
      " Day " + String(t / periods_per_day) + "\n" +
      " Honest slots: " + sepitoa(honest_slots) + "\n" +
      " Dishonest slots: " + sepitoa(slots_owned) + "\n" +
      " Honest: " + String(honest_slots * 100.0 / total_slots, 1) + "%\n",
      20, 550, 500, Justification::left);

  g.setColour(Colours::white);
  g.setFont(32.0f);
  g.drawText("Automaton Network Simulation", 0, 0, getWidth(), 40, Justification::centred);
}

void DemoMiner::resized() {
}

std::string get_rand() {
  static uint8_t b1[32] = {0};
  static uint8_t b2[32] = {0};
  static automaton::core::crypto::cryptopp::SHA256_cryptopp hasher;
  hasher.calculate_digest(b1, 32, b2);
  memcpy(b1, b2, 32);
  return std::string(reinterpret_cast<char *>(b1), 32);
}

void DemoMiner::update() {
  t++;
  double coeff = (1.0 - pow(total_supply, 2.0) / pow(supply_cap, 2.0));

  unsigned int mask = 0x80000000;
  unsigned int total_power = (t <= 1371) ? mp[t] : mp[1371];
  total_power += mining_power;
  unsigned int opt = 0;
  while (total_power > (0x10000 << opt)) {
    opt++;
    mask |= mask >> 1;
  }

  for (int i = 0; i < (total_power >> opt); i++) {
    std::string r = get_rand();
    r[0] |= mask1;
    r[1] |= mask2;
    r[2] |= mask3;
    unsigned int x = r[30] % m;
    unsigned int y = r[31] % n;
    // unsigned int r = rand_r(&my_seed) | mask;
    auto lb = leading_bits(r);
    if ((lb >= initial_difficulty_bits) && (r > slots[x][y].diff)) {
      tx_count++;
      unsigned int reward = coeff * (t - slots[x][y].tm) * reward_per_period;
      if (slots[x][y].tm != 0) {
        total_supply += reward;
      }
      if (slots[x][y].owner == 1) {
        total_balance += reward / 2;
      }
      slots[x][y].diff = r;
      slots[x][y].bits = lb;
      // slots[x][y].owner = (rand_r(&my_seed) % total_power <= mining_power) ? 1 : 0;
      // TODO(asen): don't use a secure random.
      // NOLINTNEXTLINE
      slots[x][y].owner = ((rand() % 10000) < (mining_power * 100)) ? 1 : 0;
      slots[x][y].tm = t;
    }
    if (leading_bits(r) > max_leading_bits) {
      max_leading_bits = leading_bits(r);
    }
  }
}

void DemoMiner::timerCallback() {
  update();
  repaint();
}

int DemoMiner::mp[] = {
  5500,
  27300,
  40200,
  46200,
  51000,
  55200,
  60200,
  62600,
  66700,
  71900,
  77500,
  83300,
  92300,
  100100,
  105900,
  121100,
  153100,
  164700,
  177200,
  198500,
  200400,
  202600,
  200100,
  218900,
  232900,
  234800,
  234500,
  253300,
  268600,
  283400,
  301800,
  307400,
  327000,
  346400,
  375500,
  378800,
  293300,
  282700,
  285800,
  287400,
  275500,
  281200,
  285100,
  297500,
  309800,
  318300,
  300400,
  291000,
  280100,
  273800,
  256300,
  251500,
  281200,
  296500,
  300300,
  312600,
  318600,
  316300,
  327600,
  328800,
  305900,
  274600,
  255000,
  256300,
  271200,
  285200,
  281300,
  284900,
  284500,
  271300,
  271000,
  277600,
  273500,
  278300,
  278600,
  288200,
  293900,
  281500,
  263600,
  272200,
  270800,
  268500,
  264500,
  266400,
  259900,
  266500,
  285100,
  298700,
  296500,
  296600,
  310400,
  319100,
  336800,
  353300,
  352700,
  348600,
  332400,
  351000,
  343700,
  344200,
  342300,
  349500,
  351500,
  351800,
  347800,
  351200,
  356800,
  358700,
  374500,
  377500,
  369600,
  387600,
  399300,
  361500,
  344600,
  341500,
  343000,
  347500,
  340800,
  339900,
  338600,
  350000,
  350000,
  352700,
  356900,
  354400,
  344000,
  355100,
  370100,
  356000,
  346700,
  342200,
  333200,
  352400,
  365400,
  364700,
  384100,
  388800,
  397500,
  388100,
  395900,
  398000,
  395300,
  387100,
  385900,
  396200,
  386800,
  388500,
  387400,
  383300,
  389400,
  386800,
  390100,
  384300,
  387300,
  387500,
  369300,
  376800,
  377800,
  376500,
  381800,
  372200,
  376400,
  382300,
  377900,
  400200,
  403000,
  384600,
  396000,
  407800,
  417200,
  411100,
  428600,
  432400,
  426800,
  435100,
  448700,
  456000,
  459400,
  468300,
  481000,
  490500,
  473700,
  493400,
  493000,
  484900,
  494900,
  483900,
  492200,
  503800,
  506600,
  492600,
  500900,
  523100,
  509800,
  528500,
  543700,
  523700,
  548400,
  566500,
  583700,
  575600,
  576700,
  552300,
  573500,
  580900,
  587500,
  608900,
  624900,
  617600,
  615400,
  636900,
  660300,
  668100,
  677100,
  655200,
  679500,
  707200,
  731700,
  746800,
  786200,
  778100,
  793000,
  810000,
  816300,
  846900,
  891000,
  913500,
  909700,
  796600,
  805900,
  817800,
  829600,
  845600,
  880500,
  873500,
  887100,
  850700,
  897600,
  946200,
  970800,
  996300,
  986800,
  1020600,
  1034600,
  1083000,
  1076600,
  1066200,
  1101400,
  1144500,
  1165300,
  1163600,
  1117000,
  1185000,
  1216900,
  1227800,
  1224500,
  1183000,
  1235000,
  1248700,
  1283300,
  1309300,
  1279400,
  1338400,
  1361300,
  1367100,
  1399500,
  1348200,
  1399400,
  1387400,
  1429100,
  1427700,
  1409100,
  1451800,
  1480600,
  1528100,
  1563300,
  1463400,
  1548700,
  1548900,
  1535500,
  1545000,
  1545200,
  1562300,
  1581400,
  1618500,
  1579400,
  1568900,
  1668900,
  1670400,
  1682500,
  1694400,
  1653000,
  1718200,
  1791400,
  1815500,
  1843000,
  1816000,
  1828400,
  1863400,
  1859400,
  1901700,
  1892300,
  1935900,
  1977400,
  1967100,
  1975900,
  1904000,
  1944900,
  2039600,
  2040000,
  2034600,
  2052600,
  2094700,
  2078600,
  2183200,
  2124400,
  2122400,
  2147300,
  2228000,
  2250300,
  2254600,
  2252400,
  2193000,
  2071100,
  2241200,
  2319800,
  2266600,
  2312200,
  2423400,
  2393200,
  2422900,
  2443100,
  2507200,
  2531100,
  2490500,
  2493300,
  2504700,
  2566400,
  2618000,
  2603700,
  2648500,
  2586400,
  2615800,
  2623100,
  2556700,
  2650400,
  2686500,
  2708100,
  2713200,
  2712500,
  2661600,
  2656100,
  2702400,
  2709800,
  2737800,
  2671400,
  2709300,
  2701700,
  2756100,
  2761300,
  2631200,
  2606000,
  2329500,
  2524500,
  2500000,
  2478800,
  2469900,
  2468000,
  2302800,
  2204700,
  2316100,
  2416500,
  2402800,
  2428900,
  2484700,
  2572900,
  2668300,
  2725400,
  2721400,
  2705200,
  2715400,
  2698000,
  2791000,
  2786300,
  2770300,
  2831500,
  2876300,
  2860700,
  2921600,
  2981000,
  2969000,
  3052700,
  3174900,
  3128900,
  3118800,
  3089400,
  3092600,
  3130700,
  3200400,
  3179200,
  3105800,
  2986200,
  3067300,
  3089800,
  3110100,
  3104700,
  3094800,
  3161000,
  3226700,
  3228900,
  3248300,
  3288800,
  3314600,
  3293200,
  3419100,
  3274200,
  2851500,
  3389000,
  3532800,
  3233300,
  3294400,
  3364200,
  3421200,
  3420100,
  3242300,
  3689200,
  3794900,
  3777300,
  3459100,
  3445600,
  3697400,
  3922000,
  3662900,
  3799900,
  3169200,
  3454900,
  3851100,
  3918000,
  4065800,
  3815100,
  3726300,
  4170300,
  4327200,
  4314300,
  4177800,
  4212900,
  4186900,
  4208100,
  4631100,
  4705300,
  4639500,
  4610000,
  4745000,
  4750200,
  4744500,
  4612000,
  3855000,
  3462100,
  3702100,
  3325200,
  2933900,
  2788700,
  2591300,
  2402900,
  2341300,
  2656500,
  3049300,
  3105400,
  2834800,
  2746400,
  2741100,
  2930800,
  3087600,
  3155700,
  3080100,
  3016000,
  3011900,
  3261700,
  3551200,
  3828400,
  3675500,
  3426700,
  3087700,
  2991400,
  3128800,
  2970400,
  3066800,
  3128500,
  3140900,
  3187400,
  3230700,
  3366100,
  3430500,
  3336600,
  3228100,
  3338500,
  3462100,
  3594000,
  3671300,
  3759900,
  3666500,
  3727500,
  3702700,
  3741500,
  3814300,
  3832200,
  3978500,
  4030200,
  4103000,
  4085000,
  4014400,
  3724900,
  3487400,
  3570000,
  3741300,
  3669600,
  3603200,
  3558600,
  3581200,
  3675300,
  3621900,
  3653500,
  3651400,
  3958400,
  4084400,
  4163300,
  4236200,
  4285500,
  4414700,
  4475400,
  4492400,
  4532200,
  4594800,
  4647400,
  4698400,
  4720700,
  4706400,
  4626400,
  4859000,
  4844200,
  4912200,
  4932800,
  5036900,
  4981500,
  4987600,
  5061600,
  5109400,
  5059100,
  5017400,
  5030400,
  5110700,
  5057300,
  5163000,
  5091900,
  5107500,
  5356700,
  5405300,
  5413200,
  5450900,
  5490500,
  5640400,
  5654400,
  5732900,
  5781000,
  5892200,
  5874800,
  5893100,
  6041900,
  6120600,
  6220600,
  6214600,
  6357700,
  6370600,
  6336300,
  6357700,
  6479100,
  6682300,
  6834600,
  7005400,
  6918300,
  6924700,
  7263200,
  7194200,
  7432700,
  7541900,
  7522300,
  7731500,
  7883200,
  7840200,
  7940200,
  8057800,
  8170700,
  8153000,
  8347300,
  8425400,
  8740700,
  8957900,
  8652600,
  9039800,
  9049900,
  9140700,
  9559800,
  9565300,
  9574600,
  10026600,
  10501800,
  10406200,
  10561800,
  10743200,
  10886300,
  10836800,
  10972300,
  10235300,
  10906700,
  11164600,
  11270700,
  11314100,
  11446000,
  11477300,
  11604100,
  11833700,
  11829400,
  11551200,
  12311400,
  12348700,
  12357000,
  12490800,
  12817500,
  13090000,
  12873400,
  13184000,
  13469000,
  13529800,
  13382900,
  13523300,
  14004900,
  14220900,
  14898000,
  15508200,
  15077100,
  15239800,
  15195700,
  15389700,
  15251100,
  15857100,
  15833100,
  16029500,
  16040200,
  16572500,
  16486800,
  16739900,
  17192700,
  17158300,
  18350000,
  18770200,
  18907600,
  18500700,
  18788300,
  19568100,
  19933700,
  20460200,
  20994000,
  20341800,
  20807000,
  20749600,
  20696600,
  21454800,
  21269400,
  22109700,
  23265800,
  23560800,
  25432000,
  26044300,
  26100600,
  27094700,
  27184400,
  27884400,
  28674900,
  29043000,
  29012800,
  30021200,
  30956200,
  30660300,
  31489300,
  32331200,
  33014000,
  33416300,
  34522700,
  34903200,
  35154500,
  38405200,
  39635300,
  39549700,
  39962800,
  40286800,
  41011000,
  41881300,
  42176300,
  43069400,
  43644600,
  44064400,
  44999500,
  45433400,
  45920000,
  46568800,
  46374000,
  47020500,
  47064400,
  47870300,
  48556900,
  54342700,
  55043900,
  53763500,
  54760200,
  53609200,
  54760200,
  56384800,
  54603700,
  55030000,
  55941300,
  57637600,
  59158700,
  57947800,
  59614600,
  59047000,
  60219700,
  60359600,
  59918000,
  60400700,
  60392100,
  61648800,
  64669800,
  72838400,
  72829100,
  70533000,
  72110300,
  71884600,
  72484900,
  73438500,
  74176900,
  75313900,
  74805800,
  74353500,
  76727900,
  76651700,
  78374800,
  78623800,
  77680100,
  78808600,
  79126900,
  81412000,
  81264200,
  80923900,
  81799400,
  83551900,
  82877900,
  97678500,
  99733600,
  98832700,
  96709700,
  99919100,
  101145500,
  100437700,
  101255100,
  101150500,
  102013100,
  101726300,
  101140500,
  101211200,
  102174200,
  106586600,
  105237000,
  103808600,
  104844500,
  107679800,
  105360900,
  105262900,
  107263100,
  106538600,
  108386600,
  106608300,
  107928400,
  107936700,
  109044000,
  133200100,
  134223000,
  133534400,
  132029800,
  130882600,
  132193900,
  131288600,
  127068000,
  125749900,
  129909400,
  129706400,
  128693400,
  131301300,
  134583200,
  132032400,
  129537200,
  132170300,
  132870000,
  132078000,
  135881500,
  131998300,
  137405900,
  132282600,
  134299600,
  133281100,
  133540800,
  131842900,
  136295100,
  135151100,
  137287300,
  138122400,
  135876500,
  139712600,
  138525800,
  134121700,
  135102000,
  137053100,
  136229200,
  135088700,
  136475900,
  140495500,
  134544700,
  132466400,
  131154100,
  134356400,
  133821800,
  131806700,
  135756900,
  132588700,
  133428600,
  134189400,
  138817600,
  135966000,
  135217100,
  132940100,
  130399300,
  130966800,
  129708900,
  130882900,
  138993300,
  137336500,
  136532500,
  141763400,
  144606100,
  145793500,
  144982100,
  143472500,
  140750700,
  141939500,
  143989600,
  144382000,
  146560700,
  140910100,
  142879000,
  137899300,
  137055300,
  138200400,
  144936400,
  144965500,
  144613200,
  147066500,
  145251000,
  150522100,
  153187900,
  160186000,
  162431000,
  163771100,
  166483600,
  162070100,
  160173900,
  162543700,
  161074900,
  167286700,
  170113500,
  172968000,
  174144200,
  173975400,
  178866200,
  174584800,
  174966400,
  179158500,
  180606700,
  179641500,
  177738700,
  176908700,
  180419300,
  177252300,
  182060400,
  187208900,
  187554900,
  187217800,
  193706100,
  195971700,
  199808200,
  202455700,
  204341400,
  205496400,
  210669600,
  212378700,
  211752800,
  220623500,
  221462700,
  223548000,
  223933500,
  229596900,
  234042900,
  233412900,
  231965200,
  233370100,
  241862300,
  242316700,
  245164700,
  248837800,
  253070500,
  258128700,
  257546100,
  260049700,
  260127200,
  264807000,
  263360500,
  268542200,
  264549500,
  275403200,
  274748400,
  277345700,
  275596300,
  269948200,
  279180800,
  277547100,
  280356700,
  277849400,
  273735000,
  271734000,
  278245100,
  276986500,
  277659700,
  289001600,
  282849700,
  278018300,
  284906300,
  289618700,
  287209400,
  297809400,
  293082000,
  292621100,
  291051700,
  298129900,
  302200500,
  294549200,
  295561900,
  293896500,
  297824900,
  296491000,
  292665100,
  295019700,
  305518500,
  304412500,
  304786300,
  306809400,
  306267500,
  304386000,
  301041500,
  306538900,
  300416600,
  304693000,
  311121200,
  305628000,
  304108300,
  297125900,
  299782100,
  297647100,
  302364300,
  298468700,
  292163800,
  296518300,
  294114400,
  303912000,
  302015900,
  297861500,
  288303300,
  292680100,
  293684100,
  292274600,
  288025400,
  292349400,
  292830300,
  290613100,
  292394300,
  287294800,
  289180400,
  286543800,
  295703000,
  293695400,
  296915800,
  292327600,
  292068600,
  299896900,
  294213300,
  292026600,
  297810800,
  298322800,
  290637500,
  290391100,
  295362100,
  298927400,
  302605000,
  305385000,
  310592800,
  312066400,
  305053400,
  306111300,
  306619800,
  314261000,
  317054300,
  310069100,
  304512500,
  306725500,
  306219800,
  302085200,
  303353100,
  308743400,
  311894100,
  310172900,
  303536100,
  300870300,
  298945900,
  303625300,
  300077300,
  301689900,
  300435600,
  296544600,
  303267500,
  294156300,
  298118700,
  298908700,
  294853700,
  301732200,
  295250000,
  295093700,
  298373400,
  308326400,
  303533200,
  302020900,
  311084000,
  305159200,
  305845100,
  303991500,
  307372100,
  307560200,
  317054400,
  320602800,
  315068900,
  322610400,
  320025300,
  316817100,
  322453600,
  317023600,
  313863900,
  325421700,
  317940100,
  315368700,
  323874600,
  313741600,
  316597000,
  322552300,
  320767000,
  332304700,
  327777100,
  329039900,
  322256200,
  329576200,
  326581800,
  313326700,
  321251700,
  321648900,
  319623800,
  319395600,
  321583800,
  322083000,
  324648600,
  320489400,
  319936700,
  302066300,
  312266400,
  328157900,
  319754100,
  324388200,
  329384600,
  326957900,
  330858800,
  332030400,
  332539600,
  329442300,
  328092600,
  331714200,
  324551300,
  332797000,
  332977800,
  335563900,
  332860500,
  335178800,
  336563400,
  334355600,
  331687000,
  325821200,
  326705200,
  330894400,
  327311300,
  320756300,
  325083200,
  321220800,
  323760900,
  326176000,
  328168000,
  335437100,
  322184000,
  320248200,
  326713800,
  333175200,
  326239200,
  325841100,
  322568700,
  320347400,
  323482500,
  319873800,
  316019300,
  317943500,
  316843700,
  320645000,
  317313500,
  317301200,
  315085900,
  299562000,
  300624800,
  298418400,
  292421900,
  284662600,
  293322700,
  293690600,
  292086200,
  293074800,
  296696600,
  295928300,
  296477300,
  290463100,
  305955100,
  303238300,
  304703700,
  300771800,
  306475800,
  305831400,
  306660400,
  304804500,
  304281700,
  302460900,
  299779700,
  301101300,
  304430300,
  305853400,
  303576800,
  305693600,
  309082900,
  307007000,
  307524900,
  300505200,
  302139500,
  298333700,
  300605800,
  300109800,
  299189100,
  297719700,
  302173900,
  299936500,
  294672600,
  291976100,
  290697800,
  289321300,
  293751300,
  291410500,
  286047300,
  291924400,
  289008800,
  284838200,
  289285700,
  290120200,
  285749100,
  280420200,
  280077600,
  283126200,
  279009500,
  281253000,
  280255500,
  285806500,
  280292700,
  279934800,
  287690200,
  280732400,
  283035100,
  285861200,
  286514200,
  288307800,
  281304100,
  279233400,
  279655500,
  273031300,
  260372300,
  257294500,
  258028400,
  257324400,
  252654300,
  251504500,
  234852200,
  223980000,
  225781200,
  226392400,
  234519200,
  226374700,
  226034000,
  230236300,
  223916400,
  228061000,
  220314600,
  215521500,
  213644000,
  220229100,
  219384100,
  210755800,
  207110100,
  206877400,
  207414900,
  207444300,
  209198800,
  207195600,
  210420500,
  209192400,
  211247400,
  211625000,
  214755500,
  222705700,
  229268700,
  235803500,
  233235200,
  228844900,
  227607300,
  232688000,
  233727400,
  223836400,
  226274600,
  241845300,
  241933100,
  246151200,
  251662100,
  252146900,
  249071900,
  252363500,
  249607200,
  251256400,
  244981400,
  243426400,
  245778500,
  252194500,
  244631200,
  240877600,
  240021900,
  246126500,
  249532800,
  265145400,
  268683500,
  256931300,
  261809800,
  262502200,
  256585800,
  255686800,
  243157500,
  228085000,
  224423500,
  230137400,
  224749300,
  220164900,
  219763600,
  220444300,
  220639900,
  219225100,
  217846600,
  221360300,
  221208400,
  261297500,
  260442400,
  265579700,
  262610400,
  257962700,
  257465200,
  260539300,
  260137200,
  261384100,
  273589500,
  273870800,
  274024100,
  271598600,
  281833000,
  281025700,
  276836000,
  277968500,
  277648800,
  273228900,
  271503300,
  256433700,
  254540400,
  251638400,
  253653500,
  254888700,
  253936700,
  257253300,
  251092300,
  251233600,
  251114400,
  247141500,
  245504700,
  247010500,
  247879900,
  252346500,
  253554800,
  251090700,
  241227600,
  248801600,
  248219500,
  245604100,
  244478900,
  246224300,
  245781500,
  239212800,
  234840000,
  236697700,
  238858300,
  228927900,
  229524000,
  241300800,
  239542400,
  238524300,
  242603200,
  242845300,
  245759200,
  243091900,
  243415600,
  246866400,
  250433600,
  252597600,
  250541200,
  253119100,
  257275700,
  256495100,
  252931500,
  248413600,
  247232400,
  250895200,
  249811500,
  250244700,
  254052700,
  252794300,
  247153900,
  246745600,
  246511600,
  252542700,
  251346000,
  254092000,
  257159300,
};
