// ==============================================================================
// Copyright 2020 The LatticeX Foundation
// This file is part of the Rosetta library.
//
// The Rosetta library is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// The Rosetta library is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with the Rosetta library. If not, see <http://www.gnu.org/licenses/>.
// ==============================================================================
#include "snn_helper.h"

#include <helper.h>
#include "AESObject.h"
#include "generate_key.h"
#include "logger.h"
#include "model_tool.h"
#include "tools.h"
#include "AESObject.h"
#include "ParallelAESObject.h"
#include "internal/opsets.h"
#include "communication.h"
#include <string>

int NUM_OF_PARTIES;

// global variable. For this player number
int partyNum;

// global varible parsed from config file to specify which parties
// to store the plain result with MpcSaveV2
int SAVER_MODE;

// global variable. For faster DGK computation
small_mpc_t additionModPrime[PRIME_NUMBER][PRIME_NUMBER];
small_mpc_t multiplicationModPrime[PRIME_NUMBER][PRIME_NUMBER];

static void initializeMPC() {
  // populate offline module prime addition and multiplication tables
  for (int i = 0; i < PRIME_NUMBER; ++i)
    for (int j = 0; j < PRIME_NUMBER; ++j) {
      additionModPrime[i][j] = (i + j) % PRIME_NUMBER;
      multiplicationModPrime[i][j] = (i * j) % PRIME_NUMBER;
    }
}

static int init_params(const Params_Fields& params) {
  log_info << __FUNCTION__ << endl;
  NUM_OF_PARTIES = params.PATRIY_NUM;
  partyNum = (int)params.PID;
  SAVER_MODE = (int)params.SAVER_MODE;
  if ((partyNum < 0) || (partyNum > 3)) {
    LOGE("party num [%d] is error. only supports 3PC", partyNum);
    exit(0);
  }

  return 0;
}

static int init_keys(const Params_Fields& params) {
  log_info << __FUNCTION__ << endl;
#if MPC_DEBUG_USE_FIXED_AESKEY
  return 0;
#endif
  using namespace rosetta::mpc;
  if (params.TYPE == "3PC") {
    // gen private key
    // 3PC: P0-->keyA, keyCD; P1-->keyB; P2-->keyC
    if (params.PID == 0) {
      AESKeyStrings::keys.key_a = gen_key_str();
    } else if (params.PID == 1) {
      AESKeyStrings::keys.key_b = gen_key_str();
    } else if (params.PID == 2) {
      AESKeyStrings::keys.key_c = gen_key_str();
      AESKeyStrings::keys.key_cd = gen_key_str();
    }
    usleep(1000);

    // public aes key
    string kab, kac, kbc, k0;
    k0 = gen_key_str();
    kab = gen_key_str();
    kac = gen_key_str();
    kbc = gen_key_str();

    auto sync_aes_key = GetMpcOpDefault(SyncAesKey);
    sync_aes_key->Run(PARTY_A, PARTY_B, kab, kab);
    sync_aes_key->Run(PARTY_A, PARTY_C, kac, kac);
    sync_aes_key->Run(PARTY_B, PARTY_C, kbc, kbc);
    sync_aes_key->Run(PARTY_C, PARTY_A, k0, k0);
    sync_aes_key->Run(PARTY_C, PARTY_B, k0, k0);
    AESKeyStrings::keys.key_0 = k0;
    AESKeyStrings::keys.key_ab = kab;
    AESKeyStrings::keys.key_bc = kbc;
    AESKeyStrings::keys.key_ac = kac;
  }
  //synchronize(1);
  usleep(1000);

  return 0;
}

static int init_comm(const Params_Fields& params) {
  log_info << __FUNCTION__ << endl;
  if (MPC) {
    CommOptions opt;
    opt.party_id = params.PID;
    opt.parties = params.PATRIY_NUM;
    opt.base_port = params.BASE_PORT;
    for (int i = 0; i < 3; i++) {
      opt.hosts.push_back(params.P[i].HOST);
    }
    opt.server_cert_ = params.SERVER_CERT;
    opt.server_prikey_ = params.SERVER_PRIKEY;
    opt.server_prikey_password_ = params.SERVER_PRIKEY_PASSWORD;

    if (!initialize_communication(opt))
      return -1;
    //synchronize(1);
  }

  return 0;
}

static int init_aes(const Params_Fields& params) {
  log_info << __FUNCTION__ << endl;
  if (!STANDALONE)
    initializeMPC();

  LOGI("init aes objects ok.");
  return 0;
}

static int uninit_aes() {
  log_info << __FUNCTION__ << endl;
  return 0;
}
static int uninit_comm() {
  log_info << __FUNCTION__ << endl;
  uninitialize_communication();
  return 0;
}

/*
** 
** 
*/
int initialize_mpc(const Params_Fields& params) {
  // uncomment for logging
  //Logger::Get().log_to_stdout(true);
  init_params(params);
  init_comm(params);
  init_keys(params);
  init_aes(params);
  return 0;
}
int uninitialize_mpc() {
  uninit_comm();
  uninit_aes();
  return 0;
}
int partyid() { return partyNum; }

void convert_mpctype_to_double(const vector<mpc_t>& vec1, vector<double>& vec2) {
  vec2.resize(vec1.size());
  for (int i = 0; i < vec1.size(); i++)
    vec2[i] = MpcTypeToFloat(vec1[i]);
}
void convert_double_to_mpctype(const vector<double>& vec1, vector<mpc_t>& vec2) {
  vec2.resize(vec1.size());
  for (int i = 0; i < vec1.size(); i++)
    vec2[i] = FloatToMpcType(vec1[i]);
}

void convert_mytype_to_double_bc(const vector<mpc_t>& vec1, vector<double>& vec2) {
  vec2.resize(vec1.size());
  for (int i = 0; i < vec1.size(); i++)
    vec2[i] = MpcTypeToFloatBC(vec1[i]);
}
void convert_double_to_mytype_bc(const vector<double>& vec1, vector<mpc_t>& vec2) {
  vec2.resize(vec1.size());
  for (int i = 0; i < vec1.size(); i++)
    vec2[i] = FloatToMpcTypeBC(vec1[i]);
}
