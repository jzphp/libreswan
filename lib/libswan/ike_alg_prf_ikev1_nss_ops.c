/*
 * Calculate IKEv1 prf and keying material, for libreswan
 *
 * Copyright (C) 2007 Michael C. Richardson <mcr@xelerance.com>
 * Copyright (C) 2010 Paul Wouters <paul@xelerance.com>
 * Copyright (C) 2013 D. Hugh Redelmeier <hugh@mimosa.com>
 * Copyright (C) 2015-2019 Andrew Cagney <cagney@gnu.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/gpl2.txt>.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 * for more details.
 */

#include "ike_alg.h"
#include "ike_alg_prf_ikev1_ops.h"
#include "crypt_symkey.h"
#include "lswlog.h"

/*
 * Compute: SKEYID = prf(Ni_b | Nr_b, g^xy)
 *
 * MUST BE THREAD-SAFE
 */
static PK11SymKey *signature_skeyid(const struct prf_desc *prf_desc,
				    const chunk_t Ni,
				    const chunk_t Nr,
				    /*const*/ PK11SymKey *dh_secret /* NSS doesn't do const */)
{
	CK_NSS_IKE_PRF_DERIVE_PARAMS ike_prf_params = {
		.prfMechanism = prf_desc->nss.mechanism,
		.bDataAsKey = CK_TRUE,
		.bRekey = CK_FALSE,
		.pNi = Ni.ptr,
		.ulNiLen = Ni.len,
		.pNr = Nr.ptr,
		.ulNrLen = Nr.len,
	};
	SECItem params = {
		.data = (unsigned char *)&ike_prf_params,
		.len = sizeof(ike_prf_params),
	};

        return crypt_derive(dh_secret, CKM_NSS_IKE_PRF_DERIVE, &params,
			    "skeyid", CKM_NSS_IKE1_PRF_DERIVE, CKA_DERIVE,
			    /*key,flags*/ 0, 0, HERE);
}

/*
 * Compute: SKEYID = prf(pre-shared-key, Ni_b | Nr_b)
 */
static PK11SymKey *pre_shared_key_skeyid(const struct prf_desc *prf_desc,
					 chunk_t pre_shared_key,
					 chunk_t Ni, chunk_t Nr)
{
	PK11SymKey *psk = prf_key_from_bytes("psk", prf_desc,
                                     pre_shared_key.ptr, pre_shared_key.len);
	PK11SymKey *skeyid;
	if (psk == NULL) {
		return NULL;
	}

	CK_NSS_IKE_PRF_DERIVE_PARAMS ike_prf_params = {
		.prfMechanism = prf_desc->nss.mechanism,
		.bDataAsKey = CK_FALSE,
		.bRekey = CK_FALSE,
		.pNi = Ni.ptr,
		.ulNiLen = Ni.len,
		.pNr = Nr.ptr,
		.ulNrLen = Nr.len,
	};
	SECItem params = {
		.data = (unsigned char *)&ike_prf_params,
		.len = sizeof(ike_prf_params),
	};

	skeyid = crypt_derive(psk, CKM_NSS_IKE_PRF_DERIVE, &params,
			      "skeyid", CKM_NSS_IKE1_PRF_DERIVE, CKA_DERIVE,
			      /*key_size*/0, /*flags*/0, HERE);
	release_symkey("SKEYID psk", "psk", &psk);
	return skeyid;
}

/*
 * SKEYID_d = prf(SKEYID, g^xy | CKY-I | CKY-R | 0)
 */
static PK11SymKey *skeyid_d(const struct prf_desc *prf_desc,
			    PK11SymKey *skeyid,
			    PK11SymKey *dh_secret,
			    chunk_t cky_i, chunk_t cky_r)
{
	CK_NSS_IKE1_PRF_DERIVE_PARAMS ike1_prf_params = {
		.prfMechanism = prf_desc->nss.mechanism,
		.bHasPrevKey = CK_FALSE,
		.hKeygxy = PK11_GetSymKeyHandle(dh_secret),
		.pCKYi = cky_i.ptr,
		.ulCKYiLen = cky_i.len,
		.pCKYr = cky_r.ptr,
		.ulCKYrLen = cky_r.len,
		.keyNumber = 0,
	};
	SECItem params = {
		.data = (unsigned char *)&ike1_prf_params,
		.len = sizeof(ike1_prf_params),
	};

	return crypt_derive(skeyid, CKM_NSS_IKE1_PRF_DERIVE, &params,
			    "skeyid_d", CKM_EXTRACT_KEY_FROM_KEY, CKA_DERIVE,
			    /*key-size*/0, /*flags*/0, HERE);
}

/*
 * SKEYID_a = prf(SKEYID, SKEYID_d | g^xy | CKY-I | CKY-R | 1)
 */
static PK11SymKey *skeyid_a(const struct prf_desc *prf_desc,
			    PK11SymKey *skeyid,
			    PK11SymKey *skeyid_d, PK11SymKey *dh_secret,
			    chunk_t cky_i, chunk_t cky_r)
{
	CK_NSS_IKE1_PRF_DERIVE_PARAMS ike1_prf_params = {
		.prfMechanism = prf_desc->nss.mechanism,
		.bHasPrevKey = CK_TRUE,
		.hKeygxy = PK11_GetSymKeyHandle(dh_secret),
		.hPrevKey = PK11_GetSymKeyHandle(skeyid_d),
		.pCKYi = cky_i.ptr,
		.ulCKYiLen = cky_i.len,
		.pCKYr = cky_r.ptr,
		.ulCKYrLen = cky_r.len,
		.keyNumber = 1,
	};
	SECItem params = {
		.data = (unsigned char *)&ike1_prf_params,
		.len = sizeof(ike1_prf_params),
	};

	return crypt_derive(skeyid, CKM_NSS_IKE1_PRF_DERIVE, &params,
			    "skeyid_a", CKM_EXTRACT_KEY_FROM_KEY, CKA_DERIVE,
			    /*key-size*/0, /*flags*/0, HERE);
}

/*
 * SKEYID_e = prf(SKEYID, SKEYID_a | g^xy | CKY-I | CKY-R | 2)
 */
static PK11SymKey *skeyid_e(const struct prf_desc *prf_desc,
			    PK11SymKey *skeyid,
			    PK11SymKey *skeyid_a, PK11SymKey *dh_secret,
			    chunk_t cky_i, chunk_t cky_r)
{
	CK_NSS_IKE1_PRF_DERIVE_PARAMS ike1_prf_params = {
		.prfMechanism = prf_desc->nss.mechanism,
		.bHasPrevKey = CK_TRUE,
		.hKeygxy = PK11_GetSymKeyHandle(dh_secret),
		.hPrevKey = PK11_GetSymKeyHandle(skeyid_a),
		.pCKYi = cky_i.ptr,
		.ulCKYiLen = cky_i.len,
		.pCKYr = cky_r.ptr,
		.ulCKYrLen = cky_r.len,
		.keyNumber = 2,
	};
	SECItem params = {
		.data = (unsigned char *)&ike1_prf_params,
		.len = sizeof(ike1_prf_params),
	};

	return crypt_derive(skeyid, CKM_NSS_IKE1_PRF_DERIVE, &params,
			    "skeyid_e", CKM_EXTRACT_KEY_FROM_KEY, CKA_DERIVE,
			    /*key-size*/0, /*flags*/0, HERE);
}

static PK11SymKey *appendix_b_keymat_e(const struct prf_desc *prf,
				       const struct encrypt_desc *encrypter,
				       PK11SymKey *skeyid_e,
				       unsigned required_keymat)
{
#ifdef LATEST_NSS
	CK_MECHANISM_TYPE mechanism = prf->nss.mechanism;
	CK_MECHANISM_TYPE target = encrypter->nss.mechanism;
	SECItem params = {
		.data = (unsigned char *)&mechanism,
		.len = sizeof(mechanism),
	};
	/* for when ENCRYPTER isn't NSS */
	if (target == 0) target = CKM_EXTRACT_KEY_FROM_KEY;

	return crypt_derive(skeyid_e, CKM_NSS_IKE1_APP_B_PRF_DERIVE,
			    &params, "keymat_e", target, CKA_ENCRYPT,
			    /*key-size*/required_keymat, /*flags*/CKF_DECRYPT|CKF_ENCRYPT, HERE);
#else
	DBG_log("using MAC ops for %s", __func__);
	return ike_alg_prf_ikev1_mac_ops.appendix_b_keymat_e(prf, encrypter, skeyid_e,
							     required_keymat);
#endif
}

static chunk_t section_5_keymat(const struct prf_desc *prf,
				PK11SymKey *SKEYID_d,
				PK11SymKey *g_xy,
				uint8_t protocol,
				shunk_t SPI,
				chunk_t Ni_b, chunk_t Nr_b,
				unsigned required_keymat)
{
#ifdef LATEST_NSS
#else
	DBG_log("using MAC ops for %s", __func__);
	return ike_alg_prf_ikev1_mac_ops.section_5_keymat(prf, SKEYID_d, g_xy,
							  protocol, SPI, Ni_b, Nr_b,
							  required_keymat);
#endif
}

const struct prf_ikev1_ops ike_alg_prf_ikev1_nss_ops = {
#ifdef LATEST_NSS
	.backend = "NSS",
#else
	.backend = "NSS+native",
#endif
	.signature_skeyid = signature_skeyid,
	.pre_shared_key_skeyid = pre_shared_key_skeyid,
	.skeyid_d = skeyid_d,
	.skeyid_a = skeyid_a,
	.skeyid_e = skeyid_e,
	.appendix_b_keymat_e = appendix_b_keymat_e,
	.section_5_keymat = section_5_keymat,
};
