//==================================================================================
// BSD 2-Clause License
//
// Copyright (c) 2014-2022, NJIT, Duality Technologies Inc. and other contributors
//
// All rights reserved.
//
// Author TPOC: contact@openfhe.org
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this
//    list of conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice,
//    this list of conditions and the following disclaimer in the documentation
//    and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//==================================================================================

/*
  Implementation file for Boolean Circuit FHE context class
 */

#include "binfhecontext.h"
#include <string>
#include <unordered_map>

namespace lbcrypto {

void BinFHEContext::GenerateBinFHEContext(uint32_t n, uint32_t N, const NativeInteger& q, const NativeInteger& Q,
                                          double std, uint32_t baseKS, uint32_t baseG, uint32_t baseR,
                                          BINFHE_METHOD method) {
    auto lweparams  = std::make_shared<LWECryptoParams>(n, N, q, Q, Q, std, baseKS);
    auto rgswparams = std::make_shared<RingGSWCryptoParams>(N, Q, q, baseG, baseR, method, std, UNIFORM_TERNARY, true);
    m_params        = std::make_shared<BinFHECryptoParams>(lweparams, rgswparams);
    m_binfhescheme  = std::make_shared<BinFHEScheme>(method);
}

void BinFHEContext::GenerateBinFHEContext(BINFHE_PARAMSET set, bool arbFunc, uint32_t logQ, int64_t N,
                                          BINFHE_METHOD method, bool timeOptimization) {
    if (GINX != method) {
        std::string errMsg("ERROR: CGGI is the only supported method");
        OPENFHE_THROW(not_implemented_error, errMsg);
    }
    if (set != STD128 && set != TOY) {
        std::string errMsg("ERROR: STD128 and TOY are the only supported sets");
        OPENFHE_THROW(not_implemented_error, errMsg);
    }

    if (logQ > 29) {
        std::string errMsg("ERROR: logQ > 29 is not supported");
        OPENFHE_THROW(not_implemented_error, errMsg);
    }
    if (logQ < 11) {
        std::string errMsg("ERROR: logQ < 11 is not supported");
        OPENFHE_THROW(not_implemented_error, errMsg);
    }
    auto logQprime = 54;
    uint32_t baseG = 0;
    if (logQ > 25) {
        baseG = 1 << 14;
    }
    else if (logQ > 16) {
        baseG = 1 << 18;
    }
    else if (logQ > 11) {
        baseG = 1 << 27;
    }
    else {  // if (logQ == 11)
        baseG     = 1 << 5;
        logQprime = 27;
    }

    m_timeOptimization = timeOptimization;
    SecurityLevel sl   = HEStd_128_classic;
    // choose minimum ringD satisfying sl and Q
    uint32_t ringDim = StdLatticeParm::FindRingDim(HEStd_ternary, sl, logQprime);
    if (N >= ringDim) {  // if specified some larger N, security is also satisfied
        ringDim = N;
    }
    // find prime Q for NTT
    NativeInteger Q = PreviousPrime<NativeInteger>(FirstPrime<NativeInteger>(logQprime, 2 * ringDim), 2 * ringDim);
    // q = 2*ringDim by default for maximum plaintext space, if needed for arbitrary function evaluation, q = ringDim
    uint32_t q = arbFunc ? ringDim : 2 * ringDim;

    uint64_t qKS = 1 << 30;
    qKS <<= 5;

    uint32_t n      = (set == TOY) ? 32 : 1305;
    auto lweparams  = std::make_shared<LWECryptoParams>(n, ringDim, q, Q, qKS, 3.19, 32);
    auto rgswparams = std::make_shared<RingGSWCryptoParams>(ringDim, Q, q, baseG, 23, method, 3.19, UNIFORM_TERNARY,
                                                            ((logQ != 11) && timeOptimization));

    m_params       = std::make_shared<BinFHECryptoParams>(lweparams, rgswparams);
    m_binfhescheme = std::make_shared<BinFHEScheme>(method);

#if defined(BINFHE_DEBUG)
    std::cout << ringDim << " " << Q < < < < " " << n << " " << q << " " << baseG << std::endl;
#endif
}

void BinFHEContext::GenerateBinFHEContext(BINFHE_PARAMSET set, BINFHE_METHOD method, uint32_t num_of_parties) {
    struct BinFHEContextParams {
        // for intermediate prime, modulus for RingGSW / RLWE used in bootstrapping
        usint numberBits;
        usint cyclOrder;

        // for LWE crypto parameters
        usint latticeParam;
        usint mod;  // modulus for additive LWE
        // modulus for key switching; if it is zero, then it is replaced with intermediate prime for LWE crypto parameters
        usint modKS;
        double stdDev;
        usint baseKS;  // base for key switching

        // for Ring GSW + LWE parameters
        usint gadgetBase;  // gadget base used in the bootstrapping
        usint baseRK;      // base for the refreshing key

        // for key distribution
        SECRET_KEY_DIST keyDist;
    };
    enum { PRIME = 0 };  // value for modKS if you want to use the intermediate prime for modulus for key switching
    const double STD_DEV = 3.19;
    // clang-format off
    const std::unordered_map<BINFHE_PARAMSET, BinFHEContextParams> paramsMap({
        //           numberBits|cyclOrder|latticeParam|  mod|   modKS|  stdDev| baseKS| gadgetBase| baseRK| keyDist
        //{ TOY,             { 27,     1024,          64,  512,   PRIME, STD_DEV,     25,    1 <<  9,  23,   GAUSSIAN} },
        { TOY,             { 27,     1024,          64,  512,   PRIME, STD_DEV,     25,    1 <<  9,  23,   UNIFORM_TERNARY} },
        { MEDIUM,          { 28,     2048,         422, 1024, 1 << 14, STD_DEV, 1 << 7,    1 << 10,  32,   UNIFORM_TERNARY} },
        { STD128_LMKCDEY,  { 28,     2048,         458, 1024, 1 << 14, STD_DEV, 1 << 7,    1 << 10,  32,   GAUSSIAN       } },
        { STD128_AP,       { 27,     2048,         512, 1024, 1 << 14, STD_DEV, 1 << 7,    1 <<  9,  32,   UNIFORM_TERNARY} },
        { STD128_APOPT,    { 27,     2048,         502, 1024, 1 << 14, STD_DEV, 1 << 7,    1 <<  9,  32,   UNIFORM_TERNARY} },
        { STD128,          { 27,     2048,         512, 1024, 1 << 14, STD_DEV, 1 << 7,    1 <<  7,  32,   UNIFORM_TERNARY} },
        { STD128_OPT,      { 27,     2048,         502, 1024, 1 << 14, STD_DEV, 1 << 7,    1 <<  7,  32,   UNIFORM_TERNARY} },
        { STD192,          { 37,     4096,        1024, 1024, 1 << 19, STD_DEV,     28,    1 << 13,  32,   UNIFORM_TERNARY} },
        { STD192_OPT,      { 37,     4096,         805, 1024, 1 << 15, STD_DEV,     32,    1 << 13,  32,   UNIFORM_TERNARY} },
        { STD256,          { 29,     4096,        1024, 2048, 1 << 14, STD_DEV, 1 << 7,    1 <<  8,  46,   UNIFORM_TERNARY} },
        { STD256_OPT,      { 29,     4096,         990, 2048, 1 << 14, STD_DEV, 1 << 7,    1 <<  8,  46,   UNIFORM_TERNARY} },
        { STD128Q,         { 50,     4096,        1024, 1024, 1 << 25, STD_DEV,     32,    1 << 25,  32,   UNIFORM_TERNARY} },
        { STD128Q_OPT,     { 50,     4096,         585, 1024, 1 << 15, STD_DEV,     32,    1 << 25,  32,   UNIFORM_TERNARY} },
        { STD192Q,         { 35,     4096,        1024, 1024, 1 << 17, STD_DEV,     64,    1 << 12,  32,   UNIFORM_TERNARY} },
        { STD192Q_OPT,     { 35,     4096,         875, 1024, 1 << 15, STD_DEV,     32,    1 << 12,  32,   UNIFORM_TERNARY} },
        { STD256Q,         { 27,     4096,        2048, 2048, 1 << 16, STD_DEV,     16,    1 <<  7,  46,   UNIFORM_TERNARY} },
        { STD256Q_OPT,     { 27,     4096,        1225, 1024, 1 << 16, STD_DEV,     16,    1 <<  7,  32,   UNIFORM_TERNARY} },
        { SIGNED_MOD_TEST, { 28,     2048,         512, 1024,   PRIME, STD_DEV,     25,    1 <<  7,  23,   UNIFORM_TERNARY} },
    });
    // clang-format on

    auto search = paramsMap.find(set);
    if (paramsMap.end() == search) {
        std::string errMsg("ERROR: Unknown parameter set [" + std::to_string(set) + "] for FHEW.");
        OPENFHE_THROW(config_error, errMsg);
    }

    BinFHEContextParams params = search->second;
    // intermediate prime
    NativeInteger Q(
        PreviousPrime<NativeInteger>(FirstPrime<NativeInteger>(params.numberBits, params.cyclOrder), params.cyclOrder));

    usint ringDim   = params.cyclOrder / 2;
    auto lweparams  = (PRIME == params.modKS) ?
                          std::make_shared<LWECryptoParams>(params.latticeParam, ringDim, params.mod, Q, Q,
                                                           params.stdDev, params.baseKS, params.keyDist) :
                          std::make_shared<LWECryptoParams>(params.latticeParam, ringDim, params.mod, Q, params.modKS,
                                                           params.stdDev, params.baseKS, params.keyDist);
    auto rgswparams = std::make_shared<RingGSWCryptoParams>(ringDim, Q, params.mod, params.gadgetBase, params.baseRK,
                                                            method, params.stdDev, params.keyDist);

    m_params       = std::make_shared<BinFHECryptoParams>(lweparams, rgswparams);
    m_binfhescheme = std::make_shared<BinFHEScheme>(method);
    m_binfhescheme->set_num_of_parties(num_of_parties);
}

LWEPrivateKey BinFHEContext::KeyGen() const {
    auto& LWEParams = m_params->GetLWEParams();
    if (LWEParams->GetKeyDist() == GAUSSIAN) {
        return m_LWEscheme->KeyGenGaussian(LWEParams->Getn(), LWEParams->GetqKS());
    }
    else {
        return m_LWEscheme->KeyGen(LWEParams->Getn(), LWEParams->GetqKS());
    }
}

LWEKeyPair BinFHEContext::MultipartyKeyGen(const std::vector<LWEPrivateKey>& privateKeyVec) {
    auto& LWEParams = m_params->GetLWEParams();
    return m_LWEscheme->MultipartyKeyGen(privateKeyVec, LWEParams);
}

void BinFHEContext::MultiPartyKeyGen(ConstLWEPrivateKey LWEsk, const NativePoly zN, const LWEPublicKey publicKey,
                                     LWESwitchingKey prevkskey, bool leadFlag) {
    auto& LWEParams  = m_params->GetLWEParams();
    auto& RGSWParams = m_params->GetRingGSWParams();

    auto temp = RGSWParams->GetBaseG();

    // if (m_BTKey_map.size() != 0) {
    //    m_BTKey = m_BTKey_map[temp];
    // }
    // else {
    m_BTKey           = m_binfhescheme->MultiPartyKeyGen(LWEParams, LWEsk, zN, publicKey, prevkskey, leadFlag);
    m_BTKey_map[temp] = m_BTKey;
    // }
}
LWEPublicKey BinFHEContext::MultipartyPubKeyGen(const LWEPrivateKey skN, const LWEPublicKey publicKey) {
    // auto& LWEParams = m_params->GetLWEParams();
    return m_LWEscheme->MultipartyPubKeyGen(skN, publicKey);
}

NativePoly BinFHEContext::RGSWKeygen() {
    return m_binfhescheme->RGSWKeyGen(m_params);
}

LWEPrivateKey BinFHEContext::KeyGenN() const {
    auto& LWEParams = m_params->GetLWEParams();
    if (LWEParams->GetKeyDist() == GAUSSIAN) {
        return m_LWEscheme->KeyGenGaussian(LWEParams->GetN(), LWEParams->GetQ());
    }
    else {
        return m_LWEscheme->KeyGen(LWEParams->GetN(), LWEParams->GetQ());
    }
}

LWEKeyPair BinFHEContext::KeyGenPair() const {
    auto& LWEParams = m_params->GetLWEParams();
    return m_LWEscheme->KeyGenPair(LWEParams);
}

LWEPublicKey BinFHEContext::PubKeyGen(ConstLWEPrivateKey sk) const {
    auto& LWEParams = m_params->GetLWEParams();
    return m_LWEscheme->PubKeyGen(LWEParams, sk);
}

LWECiphertext BinFHEContext::Encrypt(ConstLWEPrivateKey sk, LWEPlaintext m, BINFHE_OUTPUT output, LWEPlaintextModulus p,
                                     const NativeInteger& mod) const {
    const auto& LWEParams = m_params->GetLWEParams();

    LWECiphertext ct = (mod == 0) ? m_LWEscheme->Encrypt(LWEParams, sk, m, p, LWEParams->Getq()) :
                                    m_LWEscheme->Encrypt(LWEParams, sk, m, p, mod);

    // BINFHE_OUTPUT is kept as it is for backward compatibility but
    // this logic is obsolete now and commented out
    // if ((output != FRESH) && (p == 4)) {
    //    ct = m_binfhescheme->Bootstrap(m_params, m_BTKey, ct);
    //}

    return ct;
}

LWECiphertext BinFHEContext::Encrypt(ConstLWEPublicKey pk, LWEPlaintext m, BINFHE_OUTPUT output, LWEPlaintextModulus p,
                                     const NativeInteger& mod) const {
    const auto& LWEParams = m_params->GetLWEParams();

    LWECiphertext ct = (mod == 0) ? m_LWEscheme->EncryptN(LWEParams, pk, m, p, LWEParams->GetQ()) :
                                    m_LWEscheme->EncryptN(LWEParams, pk, m, p, mod);

    // Switch from ct of modulus Q and dimension N to smaller q and n
    // This is done by default while calling Encrypt but the output could
    // be set to LARGE_DIM to skip this switching
    if (output == SMALL_DIM) {
        LWECiphertext ct1 = SwitchCTtoqn(m_BTKey.KSkey, ct);
        return ct1;
    }
    return ct;
}

LWECiphertext BinFHEContext::SwitchCTtoqn(ConstLWESwitchingKey ksk, ConstLWECiphertext ct) const {
    const auto& LWEParams = m_params->GetLWEParams();
    auto Q                = LWEParams->GetQ();
    auto N                = LWEParams->GetN();

    if ((ct->GetLength() != N) && (ct->GetModulus() != Q)) {
        std::string errMsg("ERROR: Ciphertext dimension and modulus are not large N and Q");
        OPENFHE_THROW(config_error, errMsg);
    }

    LWECiphertext ct1 = m_LWEscheme->SwitchCTtoqn(LWEParams, ksk, ct);

    return ct1;
}

void BinFHEContext::Decrypt(ConstLWEPrivateKey sk, ConstLWECiphertext ct, LWEPlaintext* result,
                            LWEPlaintextModulus p) const {
    auto& LWEParams = m_params->GetLWEParams();
    m_LWEscheme->Decrypt(LWEParams, sk, ct, result, p);
}

LWECiphertext BinFHEContext::MultipartyDecryptLead(ConstLWEPrivateKey sk, ConstLWECiphertext ct,
                                                   const LWEPlaintextModulus& p) const {
    auto& LWEParams = m_params->GetLWEParams();
    return m_LWEscheme->MultipartyDecryptLead(LWEParams, sk, ct, p);
}
LWECiphertext BinFHEContext::MultipartyDecryptMain(ConstLWEPrivateKey sk, ConstLWECiphertext ct,
                                                   const LWEPlaintextModulus& p) const {
    auto& LWEParams = m_params->GetLWEParams();
    return m_LWEscheme->MultipartyDecryptMain(LWEParams, sk, ct, p);
}
void BinFHEContext::MultipartyDecryptFusion(const std::vector<LWECiphertext>& partialCiphertextVec,
                                            LWEPlaintext* plaintext, const LWEPlaintextModulus& p) const {
    m_LWEscheme->MultipartyDecryptFusion(partialCiphertextVec, plaintext);
}

LWESwitchingKey BinFHEContext::KeySwitchGen(ConstLWEPrivateKey sk, ConstLWEPrivateKey skN) const {
    return m_LWEscheme->KeySwitchGen(m_params->GetLWEParams(), sk, skN);
}

NativePoly BinFHEContext::Generateacrs() {
    auto& RGSWParams = m_params->GetRingGSWParams();
    return m_binfhescheme->Generateacrs(RGSWParams);
}

NativePoly BinFHEContext::RGSWKeyGen() const {
    return m_binfhescheme->RGSWKeyGen(m_params);
}
// RingGSWCiphertext
RingGSWEvalKey BinFHEContext::RGSWEncrypt(NativePoly acrs, const NativePoly& skNTT, const LWEPlaintext& m,
                                          bool leadFlag) const {
    auto& RGSWParams = m_params->GetRingGSWParams();
    return m_binfhescheme->RGSWEncrypt(RGSWParams, acrs, skNTT, m, leadFlag);
}

// RingGSWCiphertext
RingGSWEvalKey BinFHEContext::RGSWEvalAdd(RingGSWEvalKey a, RingGSWEvalKey b) {
    return m_binfhescheme->RGSWEvalAdd(a, b);
}

LWEPlaintext BinFHEContext::RGSWDecrypt(RingGSWEvalKey ct, const NativePoly& skNTT) const {
    auto& RGSWParams = m_params->GetRingGSWParams();
    return m_binfhescheme->RGSWDecrypt(RGSWParams, ct, skNTT);
}

void BinFHEContext::BTKeyGen(ConstLWEPrivateKey sk, KEYGEN_MODE keygenMode) {
    auto& RGSWParams = m_params->GetRingGSWParams();

    auto temp = RGSWParams->GetBaseG();

    if (m_timeOptimization) {
        auto gpowermap = RGSWParams->GetGPowerMap();
        for (std::map<uint32_t, std::vector<NativeInteger>>::iterator it = gpowermap.begin(); it != gpowermap.end();
             ++it) {
            RGSWParams->Change_BaseG(it->first);
            m_BTKey_map[it->first] = m_binfhescheme->KeyGen(m_params, sk, keygenMode);
        }
        RGSWParams->Change_BaseG(temp);
    }

    if (m_BTKey_map.size() != 0) {
        m_BTKey = m_BTKey_map[temp];
    }
    else {
        m_BTKey           = m_binfhescheme->KeyGen(m_params, sk, keygenMode);
        m_BTKey_map[temp] = m_BTKey;
    }
}

void BinFHEContext::BTKeyGenTest(ConstLWEPrivateKey sk, NativePoly skNPoly, NativePoly acrs, LWESwitchingKey kskey,
                                 KEYGEN_MODE keygenMode) {
    auto& RGSWParams = m_params->GetRingGSWParams();

    auto temp = RGSWParams->GetBaseG();

    m_BTKey           = m_binfhescheme->KeyGenTest(m_params, sk, skNPoly, acrs, kskey, keygenMode);
    m_BTKey_map[temp] = m_BTKey;
}

void BinFHEContext::MultipartyBTKeyGen(ConstLWEPrivateKey sk, RingGSWACCKey prevbtkey, NativePoly zkey,
                                       std::vector<std::vector<NativePoly>> acrsauto,
                                       std::vector<RingGSWEvalKey> rgswenc0, LWESwitchingKey prevkskey, bool leadFlag) {
    auto& RGSWParams = m_params->GetRingGSWParams();
    auto temp        = RGSWParams->GetBaseG();

    // if (m_BTKey_map.size() != 0) {
    //    m_BTKey = m_BTKey_map[temp];
    // }
    // else {
    m_BTKey           = m_binfhescheme->MultipartyBTKeyGen(m_params, sk, prevbtkey, zkey, acrsauto, rgswenc0, prevkskey,
                                                           m_binfhescheme->get_num_of_parties(), leadFlag);
    m_BTKey_map[temp] = m_BTKey;
    // }
    return;
}
void MultipartyAutoKeygen() {
    return;
}

LWECiphertext BinFHEContext::EvalBinGate(const BINGATE gate, ConstLWECiphertext ct1, ConstLWECiphertext ct2) const {
    return m_binfhescheme->EvalBinGate(m_params, gate, m_BTKey, ct1, ct2);
}

LWECiphertext BinFHEContext::Bootstrap(ConstLWECiphertext ct) const {
    return m_binfhescheme->Bootstrap(m_params, m_BTKey, ct);
}

LWECiphertext BinFHEContext::EvalNOT(ConstLWECiphertext ct) const {
    return m_binfhescheme->EvalNOT(m_params, ct);
}

LWECiphertext BinFHEContext::EvalConstant(bool value) const {
    return m_LWEscheme->NoiselessEmbedding(m_params->GetLWEParams(), value);
}

LWECiphertext BinFHEContext::EvalFunc(ConstLWECiphertext ct, const std::vector<NativeInteger>& LUT) const {
    NativeInteger beta = GetBeta();
    return m_binfhescheme->EvalFunc(m_params, m_BTKey, ct, LUT, beta);
}

LWECiphertext BinFHEContext::EvalFloor(ConstLWECiphertext ct, uint32_t roundbits) const {
    //    auto q = m_params->GetLWEParams()->Getq().ConvertToInt();
    //    if (roundbits != 0) {
    //        NativeInteger newp = this->GetMaxPlaintextSpace();
    //        SetQ(q / newp * (1 << roundbits));
    //    }
    //    SetQ(q);
    //    return res;
    return m_binfhescheme->EvalFloor(m_params, m_BTKey, ct, GetBeta(), roundbits);
}

LWECiphertext BinFHEContext::EvalSign(ConstLWECiphertext ct) {
    auto params        = std::make_shared<BinFHECryptoParams>(*m_params);
    NativeInteger beta = GetBeta();
    return m_binfhescheme->EvalSign(params, m_BTKey_map, ct, beta);
}

std::vector<LWECiphertext> BinFHEContext::EvalDecomp(ConstLWECiphertext ct) {
    NativeInteger beta = GetBeta();
    return m_binfhescheme->EvalDecomp(m_params, m_BTKey_map, ct, beta);
}

std::vector<NativeInteger> BinFHEContext::GenerateLUTviaFunction(NativeInteger (*f)(NativeInteger m, NativeInteger p),
                                                                 NativeInteger p) {
    if (ceil(log2(p.ConvertToInt())) != floor(log2(p.ConvertToInt()))) {
        std::string errMsg("ERROR: Only support plaintext space to be power-of-two.");
        OPENFHE_THROW(not_implemented_error, errMsg);
    }

    NativeInteger q        = GetParams()->GetLWEParams()->Getq();
    NativeInteger interval = q / p;
    NativeInteger outerval = interval;
    usint vecSize          = q.ConvertToInt();
    std::vector<NativeInteger> vec(vecSize);
    for (size_t i = 0; i < vecSize; ++i) {
        auto temp = f(NativeInteger(i) / interval, p);
        if (temp >= p) {
            std::string errMsg("ERROR: input function should output in Z_{p_output}.");
            OPENFHE_THROW(not_implemented_error, errMsg);
        }
        vec[i] = temp * outerval;
    }

    return vec;
}

}  // namespace lbcrypto
