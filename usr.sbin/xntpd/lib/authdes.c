/*
 * authdes.c - an implementation of the DES cipher algorithm for NTP
 */
#include "ntp_stdlib.h"

/*
 * There are two entries in here.  auth_subkeys() called to
 * compute the encryption and decryption key schedules, while
 * auth_des() is called to do the actual encryption/decryption
 */

/*
 * Key setup.  Here we entirely permute a key, saving the results
 * for both the encryption and decryption.  Note that while the
 * decryption subkeys are simply the encryption keys reordered,
 * we save both so that a common cipher routine may be used.
 */

/*
 * Permuted choice 1 tables.  These are used to extract bits
 * from the left and right parts of the key to form Ci and Di.
 * The code that uses these tables knows which bits from which
 * part of each key are used to form Ci and Di.
 */
static U_LONG PC1_CL[8] = {
	0x00000000, 0x00000010, 0x00001000, 0x00001010,
	0x00100000, 0x00100010, 0x00101000, 0x00101010
};

static U_LONG PC1_DL[16] = {
	0x00000000, 0x00100000, 0x00001000, 0x00101000,
	0x00000010, 0x00100010, 0x00001010, 0x00101010,
	0x00000001, 0x00100001, 0x00001001, 0x00101001,
	0x00000011, 0x00100011, 0x00001011, 0x00101011
};

static U_LONG PC1_CR[16] = {
	0x00000000, 0x00000001, 0x00000100, 0x00000101,
	0x00010000, 0x00010001, 0x00010100, 0x00010101,
	0x01000000, 0x01000001, 0x01000100, 0x01000101,
	0x01010000, 0x01010001, 0x01010100, 0x01010101
};

static U_LONG PC1_DR[8] = {
	0x00000000, 0x01000000, 0x00010000, 0x01010000,
	0x00000100, 0x01000100, 0x00010100, 0x01010100
};


/*
 * At the start of some iterations of the key schedule we do
 * a circular left shift by one place, while for others we do a shift by
 * two places.  This has bits set for the iterations where we do 2 bit
 * shifts, starting at the low order bit.
 */
#define	TWO_BIT_SHIFTS	0x7efc

/*
 * Permuted choice 2 tables.  The first actually produces the low order
 * 24 bits of the subkey Ki from the 28 bit value of Ci.  The second produces
 * the high order 24 bits from Di.  The tables are indexed by six bit
 * segments of Ci and Di respectively.  The code is handcrafted to compute
 * the appropriate 6 bit chunks.
 *
 * Note that for ease of computation, the 24 bit values are produced with
 * six bits going into each byte.
 */
static U_LONG PC2_C[4][64] = {
      { 0x00000000, 0x00040000, 0x01000000, 0x01040000,
	0x00000400, 0x00040400, 0x01000400, 0x01040400,
	0x00200000, 0x00240000, 0x01200000, 0x01240000,
	0x00200400, 0x00240400, 0x01200400, 0x01240400,
	0x00000001, 0x00040001, 0x01000001, 0x01040001,
	0x00000401, 0x00040401, 0x01000401, 0x01040401,
	0x00200001, 0x00240001, 0x01200001, 0x01240001,
	0x00200401, 0x00240401, 0x01200401, 0x01240401,
	0x02000000, 0x02040000, 0x03000000, 0x03040000,
	0x02000400, 0x02040400, 0x03000400, 0x03040400,
	0x02200000, 0x02240000, 0x03200000, 0x03240000,
	0x02200400, 0x02240400, 0x03200400, 0x03240400,
	0x02000001, 0x02040001, 0x03000001, 0x03040001,
	0x02000401, 0x02040401, 0x03000401, 0x03040401,
	0x02200001, 0x02240001, 0x03200001, 0x03240001,
	0x02200401, 0x02240401, 0x03200401, 0x03240401 },

      { 0x00000000, 0x00000002, 0x00000800, 0x00000802,
	0x08000000, 0x08000002, 0x08000800, 0x08000802,
	0x00010000, 0x00010002, 0x00010800, 0x00010802,
	0x08010000, 0x08010002, 0x08010800, 0x08010802,
	0x00000100, 0x00000102, 0x00000900, 0x00000902,
	0x08000100, 0x08000102, 0x08000900, 0x08000902,
	0x00010100, 0x00010102, 0x00010900, 0x00010902,
	0x08010100, 0x08010102, 0x08010900, 0x08010902,
	0x00000010, 0x00000012, 0x00000810, 0x00000812,
	0x08000010, 0x08000012, 0x08000810, 0x08000812,
	0x00010010, 0x00010012, 0x00010810, 0x00010812,
	0x08010010, 0x08010012, 0x08010810, 0x08010812,
	0x00000110, 0x00000112, 0x00000910, 0x00000912,
	0x08000110, 0x08000112, 0x08000910, 0x08000912,
	0x00010110, 0x00010112, 0x00010910, 0x00010912,
	0x08010110, 0x08010112, 0x08010910, 0x08010912 },

      { 0x00000000, 0x04000000, 0x00002000, 0x04002000,
	0x10000000, 0x14000000, 0x10002000, 0x14002000,
	0x00000020, 0x04000020, 0x00002020, 0x04002020,
	0x10000020, 0x14000020, 0x10002020, 0x14002020,
	0x00080000, 0x04080000, 0x00082000, 0x04082000,
	0x10080000, 0x14080000, 0x10082000, 0x14082000,
	0x00080020, 0x04080020, 0x00082020, 0x04082020,
	0x10080020, 0x14080020, 0x10082020, 0x14082020,
	0x20000000, 0x24000000, 0x20002000, 0x24002000,
	0x30000000, 0x34000000, 0x30002000, 0x34002000,
	0x20000020, 0x24000020, 0x20002020, 0x24002020,
	0x30000020, 0x34000020, 0x30002020, 0x34002020,
	0x20080000, 0x24080000, 0x20082000, 0x24082000,
	0x30080000, 0x34080000, 0x30082000, 0x34082000,
	0x20080020, 0x24080020, 0x20082020, 0x24082020,
	0x30080020, 0x34080020, 0x30082020, 0x34082020 },

      { 0x00000000, 0x00100000, 0x00000008, 0x00100008,
	0x00000200, 0x00100200, 0x00000208, 0x00100208,
	0x00020000, 0x00120000, 0x00020008, 0x00120008,
	0x00020200, 0x00120200, 0x00020208, 0x00120208,
	0x00000004, 0x00100004, 0x0000000c, 0x0010000c,
	0x00000204, 0x00100204, 0x0000020c, 0x0010020c,
	0x00020004, 0x00120004, 0x0002000c, 0x0012000c,
	0x00020204, 0x00120204, 0x0002020c, 0x0012020c,
	0x00001000, 0x00101000, 0x00001008, 0x00101008,
	0x00001200, 0x00101200, 0x00001208, 0x00101208,
	0x00021000, 0x00121000, 0x00021008, 0x00121008,
	0x00021200, 0x00121200, 0x00021208, 0x00121208,
	0x00001004, 0x00101004, 0x0000100c, 0x0010100c,
	0x00001204, 0x00101204, 0x0000120c, 0x0010120c,
	0x00021004, 0x00121004, 0x0002100c, 0x0012100c,
	0x00021204, 0x00121204, 0x0002120c, 0x0012120c }
};

static U_LONG PC2_D[4][64] = {
      { 0x00000000, 0x00000200, 0x00020000, 0x00020200,
	0x00000001, 0x00000201, 0x00020001, 0x00020201,
	0x08000000, 0x08000200, 0x08020000, 0x08020200,
	0x08000001, 0x08000201, 0x08020001, 0x08020201,
	0x00200000, 0x00200200, 0x00220000, 0x00220200,
	0x00200001, 0x00200201, 0x00220001, 0x00220201,
	0x08200000, 0x08200200, 0x08220000, 0x08220200,
	0x08200001, 0x08200201, 0x08220001, 0x08220201,
	0x00000002, 0x00000202, 0x00020002, 0x00020202,
	0x00000003, 0x00000203, 0x00020003, 0x00020203,
	0x08000002, 0x08000202, 0x08020002, 0x08020202,
	0x08000003, 0x08000203, 0x08020003, 0x08020203,
	0x00200002, 0x00200202, 0x00220002, 0x00220202,
	0x00200003, 0x00200203, 0x00220003, 0x00220203,
	0x08200002, 0x08200202, 0x08220002, 0x08220202,
	0x08200003, 0x08200203, 0x08220003, 0x08220203 },

      { 0x00000000, 0x00000010, 0x20000000, 0x20000010,
	0x00100000, 0x00100010, 0x20100000, 0x20100010,
	0x00000800, 0x00000810, 0x20000800, 0x20000810,
	0x00100800, 0x00100810, 0x20100800, 0x20100810,
	0x04000000, 0x04000010, 0x24000000, 0x24000010,
	0x04100000, 0x04100010, 0x24100000, 0x24100010,
	0x04000800, 0x04000810, 0x24000800, 0x24000810,
	0x04100800, 0x04100810, 0x24100800, 0x24100810,
	0x00000004, 0x00000014, 0x20000004, 0x20000014,
	0x00100004, 0x00100014, 0x20100004, 0x20100014,
	0x00000804, 0x00000814, 0x20000804, 0x20000814,
	0x00100804, 0x00100814, 0x20100804, 0x20100814,
	0x04000004, 0x04000014, 0x24000004, 0x24000014,
	0x04100004, 0x04100014, 0x24100004, 0x24100014,
	0x04000804, 0x04000814, 0x24000804, 0x24000814,
	0x04100804, 0x04100814, 0x24100804, 0x24100814 },

      { 0x00000000, 0x00001000, 0x00010000, 0x00011000,
	0x02000000, 0x02001000, 0x02010000, 0x02011000,
	0x00000020, 0x00001020, 0x00010020, 0x00011020,
	0x02000020, 0x02001020, 0x02010020, 0x02011020,
	0x00040000, 0x00041000, 0x00050000, 0x00051000,
	0x02040000, 0x02041000, 0x02050000, 0x02051000,
	0x00040020, 0x00041020, 0x00050020, 0x00051020,
	0x02040020, 0x02041020, 0x02050020, 0x02051020,
	0x00002000, 0x00003000, 0x00012000, 0x00013000,
	0x02002000, 0x02003000, 0x02012000, 0x02013000,
	0x00002020, 0x00003020, 0x00012020, 0x00013020,
	0x02002020, 0x02003020, 0x02012020, 0x02013020,
	0x00042000, 0x00043000, 0x00052000, 0x00053000,
	0x02042000, 0x02043000, 0x02052000, 0x02053000,
	0x00042020, 0x00043020, 0x00052020, 0x00053020,
	0x02042020, 0x02043020, 0x02052020, 0x02053020 },

      { 0x00000000, 0x00000400, 0x01000000, 0x01000400,
	0x00000100, 0x00000500, 0x01000100, 0x01000500,
	0x10000000, 0x10000400, 0x11000000, 0x11000400,
	0x10000100, 0x10000500, 0x11000100, 0x11000500,
	0x00080000, 0x00080400, 0x01080000, 0x01080400,
	0x00080100, 0x00080500, 0x01080100, 0x01080500,
	0x10080000, 0x10080400, 0x11080000, 0x11080400,
	0x10080100, 0x10080500, 0x11080100, 0x11080500,
	0x00000008, 0x00000408, 0x01000008, 0x01000408,
	0x00000108, 0x00000508, 0x01000108, 0x01000508,
	0x10000008, 0x10000408, 0x11000008, 0x11000408,
	0x10000108, 0x10000508, 0x11000108, 0x11000508,
	0x00080008, 0x00080408, 0x01080008, 0x01080408,
	0x00080108, 0x00080508, 0x01080108, 0x01080508,
	0x10080008, 0x10080408, 0x11080008, 0x11080408,
	0x10080108, 0x10080508, 0x11080108, 0x11080508 }
};



/*
 * Permute the key to give us our key schedule.
 */
void
DESauth_subkeys(key, encryptkeys, decryptkeys)
	const U_LONG *key;
	u_char *encryptkeys;
	u_char *decryptkeys;
{
	register U_LONG tmp;
	register U_LONG c, d;
	register u_char *ek, *dk;
	register int two_bit_shifts;
	register int i;

	/*
	 * The first permutted choice gives us the 28 bits for C0 and
	 * 28 for D0.  C0 gets 12 bits from the left key and 16 from
	 * the right, while D0 gets 16 from the left and 12 from the
	 * right.  The code knows which bits go where.
	 */
	tmp = *key;	/* left part of key */
	c =  PC1_CL[(tmp >> 29) & 0x7]
	  | (PC1_CL[(tmp >> 21) & 0x7] << 1)
	  | (PC1_CL[(tmp >> 13) & 0x7] << 2)
	  | (PC1_CL[(tmp >>  5) & 0x7] << 3);
	d =  PC1_DL[(tmp >> 25) & 0xf]
	  | (PC1_DL[(tmp >> 17) & 0xf] << 1)
	  | (PC1_DL[(tmp >>  9) & 0xf] << 2)
	  | (PC1_DL[(tmp >>  1) & 0xf] << 3);
	
	tmp = *(key+1);	/* right part of key */
	c |= PC1_CR[(tmp >> 28) & 0xf]
	  | (PC1_CR[(tmp >> 20) & 0xf] << 1)
	  | (PC1_CR[(tmp >> 12) & 0xf] << 2)
	  | (PC1_CR[(tmp >>  4) & 0xf] << 3);
	d |= PC1_DR[(tmp >> 25) & 0x7]
	  | (PC1_DR[(tmp >> 17) & 0x7] << 1)
	  | (PC1_DR[(tmp >>  9) & 0x7] << 2)
	  | (PC1_DR[(tmp >>  1) & 0x7] << 3);

	/*
	 * Now iterate to compute the key schedule.  Note that we
	 * record the entire set of subkeys in 6 bit chunks since
	 * they are used that way.  At 6 bits/char, we need
	 * 48/6 char's/subkey * 16 subkeys/encryption == 128 chars.
	 * encryptkeys and decryptkeys must be this big.
	 */
	ek = encryptkeys;
	dk = decryptkeys + (8 * 15);
	two_bit_shifts = TWO_BIT_SHIFTS;
	for (i = 16; i > 0; i--) {
		/*
		 * Do the rotation.  One bit and two bit rotations
		 * are done separately.  Note C and D are 28 bits.
		 */
		if (two_bit_shifts & 0x1) {
			c = ((c << 2) & 0xffffffc) | (c >> 26);
			d = ((d << 2) & 0xffffffc) | (d >> 26);
		} else {
			c = ((c << 1) & 0xffffffe) | (c >> 27);
			d = ((d << 1) & 0xffffffe) | (d >> 27);
		}
		two_bit_shifts >>= 1;

		/*
		 * Apply permutted choice 2 to C to get the first
		 * 24 bits worth of keys.  Note that bits 9, 18, 22
		 * and 25 (using DES numbering) in C are unused.  The
		 * shift-mask stuff is done to delete these bits from
		 * the indices, since this cuts the table size in half.
		 */
		tmp = PC2_C[0][((c >> 22) & 0x3f)]
		    | PC2_C[1][((c >> 15) & 0xf) | ((c >> 16) & 0x30)]
		    | PC2_C[2][((c >>  4) & 0x3) | ((c >>  9) & 0x3c)]
		    | PC2_C[3][((c      ) & 0x7) | ((c >>  4) & 0x38)];
		*ek++ = *dk++ = (u_char)(tmp >> 24);
		*ek++ = *dk++ = (u_char)(tmp >> 16);
		*ek++ = *dk++ = (u_char)(tmp >>  8);
		*ek++ = *dk++ = (u_char)tmp;

		/*
		 * Apply permutted choice 2 to D to get the other half.
		 * Here, bits 7, 10, 15 and 26 go unused.  The sqeezing
		 * actually turns out to be cheaper here.
		 */
		tmp = PC2_D[0][((d >> 22) & 0x3f)]
		    | PC2_D[1][((d >> 14) & 0xf) | ((d >> 15) & 0x30)]
		    | PC2_D[2][((d >>  7) & 0x3f)]
		    | PC2_D[3][((d      ) & 0x3) | ((d >>  1) & 0x3c)];
		*ek++ = *dk++ = (u_char)(tmp >> 24);
		*ek++ = *dk++ = (u_char)(tmp >> 16);
		*ek++ = *dk++ = (u_char)(tmp >>  8);
		*ek++ = *dk++ = (u_char)tmp;

		/*
		 * We are filling in the decryption subkeys from the end.
		 * Space it back 16 elements to get to the start of the
		 * next set.
		 */
		dk -= 16;
	}
}

/*
 * The DES algorithm.  This is intended to be fairly speedy at the
 * expense of some memory.
 *
 * This uses all the standard hacks.  The S boxes and the P permutation
 * are precomputed into one table.  The E box never actually appears
 * explicitly since it is easy to apply this algorithmically.  The
 * initial permutation and final (inverse initial) permuation are
 * computed from tables designed to permute four bits at a time.  This
 * should run pretty fast on machines with 32 bit words and
 * bit field/multiple bit shift instructions which are fast.
 */

/*
 * The initial permutation array.  This is used to compute both the
 * left and the right halves of the initial permutation using bytes
 * from words made from the following operations:
 *
 * ((left & 0x55555555) << 1) | (right & 0x55555555)  for left half
 * (left & 0xaaaaaaaa) | ((right & 0xaaaaaaaa) >> 1)  for right half
 *
 * The scheme is that we index into the table using each byte.  The
 * result from the high order byte is or'd with the result from the
 * next byte shifted left once is or'd with the result from the next
 * byte shifted left twice if or'd with the result from the low order
 * byte shifted left by three.  Clear?
 */
static U_LONG IP[256] = {
	0x00000000, 0x00000010, 0x00000001, 0x00000011,
	0x00001000, 0x00001010, 0x00001001, 0x00001011,
	0x00000100, 0x00000110, 0x00000101, 0x00000111,
	0x00001100, 0x00001110, 0x00001101, 0x00001111,
	0x00100000, 0x00100010, 0x00100001, 0x00100011,
	0x00101000, 0x00101010, 0x00101001, 0x00101011,
	0x00100100, 0x00100110, 0x00100101, 0x00100111,
	0x00101100, 0x00101110, 0x00101101, 0x00101111,
	0x00010000, 0x00010010, 0x00010001, 0x00010011,
	0x00011000, 0x00011010, 0x00011001, 0x00011011,
	0x00010100, 0x00010110, 0x00010101, 0x00010111,
	0x00011100, 0x00011110, 0x00011101, 0x00011111,
	0x00110000, 0x00110010, 0x00110001, 0x00110011,
	0x00111000, 0x00111010, 0x00111001, 0x00111011,
	0x00110100, 0x00110110, 0x00110101, 0x00110111,
	0x00111100, 0x00111110, 0x00111101, 0x00111111,
	0x10000000, 0x10000010, 0x10000001, 0x10000011,
	0x10001000, 0x10001010, 0x10001001, 0x10001011,
	0x10000100, 0x10000110, 0x10000101, 0x10000111,
	0x10001100, 0x10001110, 0x10001101, 0x10001111,
	0x10100000, 0x10100010, 0x10100001, 0x10100011,
	0x10101000, 0x10101010, 0x10101001, 0x10101011,
	0x10100100, 0x10100110, 0x10100101, 0x10100111,
	0x10101100, 0x10101110, 0x10101101, 0x10101111,
	0x10010000, 0x10010010, 0x10010001, 0x10010011,
	0x10011000, 0x10011010, 0x10011001, 0x10011011,
	0x10010100, 0x10010110, 0x10010101, 0x10010111,
	0x10011100, 0x10011110, 0x10011101, 0x10011111,
	0x10110000, 0x10110010, 0x10110001, 0x10110011,
	0x10111000, 0x10111010, 0x10111001, 0x10111011,
	0x10110100, 0x10110110, 0x10110101, 0x10110111,
	0x10111100, 0x10111110, 0x10111101, 0x10111111,
	0x01000000, 0x01000010, 0x01000001, 0x01000011,
	0x01001000, 0x01001010, 0x01001001, 0x01001011,
	0x01000100, 0x01000110, 0x01000101, 0x01000111,
	0x01001100, 0x01001110, 0x01001101, 0x01001111,
	0x01100000, 0x01100010, 0x01100001, 0x01100011,
	0x01101000, 0x01101010, 0x01101001, 0x01101011,
	0x01100100, 0x01100110, 0x01100101, 0x01100111,
	0x01101100, 0x01101110, 0x01101101, 0x01101111,
	0x01010000, 0x01010010, 0x01010001, 0x01010011,
	0x01011000, 0x01011010, 0x01011001, 0x01011011,
	0x01010100, 0x01010110, 0x01010101, 0x01010111,
	0x01011100, 0x01011110, 0x01011101, 0x01011111,
	0x01110000, 0x01110010, 0x01110001, 0x01110011,
	0x01111000, 0x01111010, 0x01111001, 0x01111011,
	0x01110100, 0x01110110, 0x01110101, 0x01110111,
	0x01111100, 0x01111110, 0x01111101, 0x01111111,
	0x11000000, 0x11000010, 0x11000001, 0x11000011,
	0x11001000, 0x11001010, 0x11001001, 0x11001011,
	0x11000100, 0x11000110, 0x11000101, 0x11000111,
	0x11001100, 0x11001110, 0x11001101, 0x11001111,
	0x11100000, 0x11100010, 0x11100001, 0x11100011,
	0x11101000, 0x11101010, 0x11101001, 0x11101011,
	0x11100100, 0x11100110, 0x11100101, 0x11100111,
	0x11101100, 0x11101110, 0x11101101, 0x11101111,
	0x11010000, 0x11010010, 0x11010001, 0x11010011,
	0x11011000, 0x11011010, 0x11011001, 0x11011011,
	0x11010100, 0x11010110, 0x11010101, 0x11010111,
	0x11011100, 0x11011110, 0x11011101, 0x11011111,
	0x11110000, 0x11110010, 0x11110001, 0x11110011,
	0x11111000, 0x11111010, 0x11111001, 0x11111011,
	0x11110100, 0x11110110, 0x11110101, 0x11110111,
	0x11111100, 0x11111110, 0x11111101, 0x11111111
};

/*
 * The final permutation array.  Like the IP array, used
 * to compute both the left and right results from the nibbles
 * of words computed from:
 *
 * ((left & 0x0f0f0f0f) << 4) | (right & 0x0f0f0f0f)  for left result
 * (left & 0xf0f0f0f0) | ((right & 0xf0f0f0f0) >> 4)  for right result
 *
 * The result from the high order byte is shifted left 6 bits and
 * or'd with the result from the next byte shifted left 4 bits, which
 * is or'd with the result from the next byte shifted left 2 bits,
 * which is or'd with the result from the low byte.
 *
 * There is one of these for big end machines (the natural order for
 * DES) and a second for little end machines.  One is a byte swapped
 * version of the other.
 */
#ifndef XNTP_LITTLE_ENDIAN
	/*
	 * Big end version
	 */
static U_LONG FP[256] = {
	0x00000000, 0x02000000, 0x00020000, 0x02020000,
	0x00000200, 0x02000200, 0x00020200, 0x02020200,
	0x00000002, 0x02000002, 0x00020002, 0x02020002,
	0x00000202, 0x02000202, 0x00020202, 0x02020202,
	0x01000000, 0x03000000, 0x01020000, 0x03020000,
	0x01000200, 0x03000200, 0x01020200, 0x03020200,
	0x01000002, 0x03000002, 0x01020002, 0x03020002,
	0x01000202, 0x03000202, 0x01020202, 0x03020202,
	0x00010000, 0x02010000, 0x00030000, 0x02030000,
	0x00010200, 0x02010200, 0x00030200, 0x02030200,
	0x00010002, 0x02010002, 0x00030002, 0x02030002,
	0x00010202, 0x02010202, 0x00030202, 0x02030202,
	0x01010000, 0x03010000, 0x01030000, 0x03030000,
	0x01010200, 0x03010200, 0x01030200, 0x03030200,
	0x01010002, 0x03010002, 0x01030002, 0x03030002,
	0x01010202, 0x03010202, 0x01030202, 0x03030202,
	0x00000100, 0x02000100, 0x00020100, 0x02020100,
	0x00000300, 0x02000300, 0x00020300, 0x02020300,
	0x00000102, 0x02000102, 0x00020102, 0x02020102,
	0x00000302, 0x02000302, 0x00020302, 0x02020302,
	0x01000100, 0x03000100, 0x01020100, 0x03020100,
	0x01000300, 0x03000300, 0x01020300, 0x03020300,
	0x01000102, 0x03000102, 0x01020102, 0x03020102,
	0x01000302, 0x03000302, 0x01020302, 0x03020302,
	0x00010100, 0x02010100, 0x00030100, 0x02030100,
	0x00010300, 0x02010300, 0x00030300, 0x02030300,
	0x00010102, 0x02010102, 0x00030102, 0x02030102,
	0x00010302, 0x02010302, 0x00030302, 0x02030302,
	0x01010100, 0x03010100, 0x01030100, 0x03030100,
	0x01010300, 0x03010300, 0x01030300, 0x03030300,
	0x01010102, 0x03010102, 0x01030102, 0x03030102,
	0x01010302, 0x03010302, 0x01030302, 0x03030302,
	0x00000001, 0x02000001, 0x00020001, 0x02020001,
	0x00000201, 0x02000201, 0x00020201, 0x02020201,
	0x00000003, 0x02000003, 0x00020003, 0x02020003,
	0x00000203, 0x02000203, 0x00020203, 0x02020203,
	0x01000001, 0x03000001, 0x01020001, 0x03020001,
	0x01000201, 0x03000201, 0x01020201, 0x03020201,
	0x01000003, 0x03000003, 0x01020003, 0x03020003,
	0x01000203, 0x03000203, 0x01020203, 0x03020203,
	0x00010001, 0x02010001, 0x00030001, 0x02030001,
	0x00010201, 0x02010201, 0x00030201, 0x02030201,
	0x00010003, 0x02010003, 0x00030003, 0x02030003,
	0x00010203, 0x02010203, 0x00030203, 0x02030203,
	0x01010001, 0x03010001, 0x01030001, 0x03030001,
	0x01010201, 0x03010201, 0x01030201, 0x03030201,
	0x01010003, 0x03010003, 0x01030003, 0x03030003,
	0x01010203, 0x03010203, 0x01030203, 0x03030203,
	0x00000101, 0x02000101, 0x00020101, 0x02020101,
	0x00000301, 0x02000301, 0x00020301, 0x02020301,
	0x00000103, 0x02000103, 0x00020103, 0x02020103,
	0x00000303, 0x02000303, 0x00020303, 0x02020303,
	0x01000101, 0x03000101, 0x01020101, 0x03020101,
	0x01000301, 0x03000301, 0x01020301, 0x03020301,
	0x01000103, 0x03000103, 0x01020103, 0x03020103,
	0x01000303, 0x03000303, 0x01020303, 0x03020303,
	0x00010101, 0x02010101, 0x00030101, 0x02030101,
	0x00010301, 0x02010301, 0x00030301, 0x02030301,
	0x00010103, 0x02010103, 0x00030103, 0x02030103,
	0x00010303, 0x02010303, 0x00030303, 0x02030303,
	0x01010101, 0x03010101, 0x01030101, 0x03030101,
	0x01010301, 0x03010301, 0x01030301, 0x03030301,
	0x01010103, 0x03010103, 0x01030103, 0x03030103,
	0x01010303, 0x03010303, 0x01030303, 0x03030303
};
#else
	/*
	 * Byte swapped for little end machines.
	 */
static U_LONG FP[256] = {
	0x00000000, 0x00000002, 0x00000200, 0x00000202,
	0x00020000, 0x00020002, 0x00020200, 0x00020202,
	0x02000000, 0x02000002, 0x02000200, 0x02000202,
	0x02020000, 0x02020002, 0x02020200, 0x02020202,
	0x00000001, 0x00000003, 0x00000201, 0x00000203,
	0x00020001, 0x00020003, 0x00020201, 0x00020203,
	0x02000001, 0x02000003, 0x02000201, 0x02000203,
	0x02020001, 0x02020003, 0x02020201, 0x02020203,
	0x00000100, 0x00000102, 0x00000300, 0x00000302,
	0x00020100, 0x00020102, 0x00020300, 0x00020302,
	0x02000100, 0x02000102, 0x02000300, 0x02000302,
	0x02020100, 0x02020102, 0x02020300, 0x02020302,
	0x00000101, 0x00000103, 0x00000301, 0x00000303,
	0x00020101, 0x00020103, 0x00020301, 0x00020303,
	0x02000101, 0x02000103, 0x02000301, 0x02000303,
	0x02020101, 0x02020103, 0x02020301, 0x02020303,
	0x00010000, 0x00010002, 0x00010200, 0x00010202,
	0x00030000, 0x00030002, 0x00030200, 0x00030202,
	0x02010000, 0x02010002, 0x02010200, 0x02010202,
	0x02030000, 0x02030002, 0x02030200, 0x02030202,
	0x00010001, 0x00010003, 0x00010201, 0x00010203,
	0x00030001, 0x00030003, 0x00030201, 0x00030203,
	0x02010001, 0x02010003, 0x02010201, 0x02010203,
	0x02030001, 0x02030003, 0x02030201, 0x02030203,
	0x00010100, 0x00010102, 0x00010300, 0x00010302,
	0x00030100, 0x00030102, 0x00030300, 0x00030302,
	0x02010100, 0x02010102, 0x02010300, 0x02010302,
	0x02030100, 0x02030102, 0x02030300, 0x02030302,
	0x00010101, 0x00010103, 0x00010301, 0x00010303,
	0x00030101, 0x00030103, 0x00030301, 0x00030303,
	0x02010101, 0x02010103, 0x02010301, 0x02010303,
	0x02030101, 0x02030103, 0x02030301, 0x02030303,
	0x01000000, 0x01000002, 0x01000200, 0x01000202,
	0x01020000, 0x01020002, 0x01020200, 0x01020202,
	0x03000000, 0x03000002, 0x03000200, 0x03000202,
	0x03020000, 0x03020002, 0x03020200, 0x03020202,
	0x01000001, 0x01000003, 0x01000201, 0x01000203,
	0x01020001, 0x01020003, 0x01020201, 0x01020203,
	0x03000001, 0x03000003, 0x03000201, 0x03000203,
	0x03020001, 0x03020003, 0x03020201, 0x03020203,
	0x01000100, 0x01000102, 0x01000300, 0x01000302,
	0x01020100, 0x01020102, 0x01020300, 0x01020302,
	0x03000100, 0x03000102, 0x03000300, 0x03000302,
	0x03020100, 0x03020102, 0x03020300, 0x03020302,
	0x01000101, 0x01000103, 0x01000301, 0x01000303,
	0x01020101, 0x01020103, 0x01020301, 0x01020303,
	0x03000101, 0x03000103, 0x03000301, 0x03000303,
	0x03020101, 0x03020103, 0x03020301, 0x03020303,
	0x01010000, 0x01010002, 0x01010200, 0x01010202,
	0x01030000, 0x01030002, 0x01030200, 0x01030202,
	0x03010000, 0x03010002, 0x03010200, 0x03010202,
	0x03030000, 0x03030002, 0x03030200, 0x03030202,
	0x01010001, 0x01010003, 0x01010201, 0x01010203,
	0x01030001, 0x01030003, 0x01030201, 0x01030203,
	0x03010001, 0x03010003, 0x03010201, 0x03010203,
	0x03030001, 0x03030003, 0x03030201, 0x03030203,
	0x01010100, 0x01010102, 0x01010300, 0x01010302,
	0x01030100, 0x01030102, 0x01030300, 0x01030302,
	0x03010100, 0x03010102, 0x03010300, 0x03010302,
	0x03030100, 0x03030102, 0x03030300, 0x03030302,
	0x01010101, 0x01010103, 0x01010301, 0x01010303,
	0x01030101, 0x01030103, 0x01030301, 0x01030303,
	0x03010101, 0x03010103, 0x03010301, 0x03010303,
	0x03030101, 0x03030103, 0x03030301, 0x03030303
};
#endif


/*
 * The SP table is actually the S boxes and the P permutation
 * table combined.
 */
static U_LONG SP[8][64] = {
      { 0x00808200, 0x00000000, 0x00008000, 0x00808202,
	0x00808002, 0x00008202, 0x00000002, 0x00008000,
	0x00000200, 0x00808200, 0x00808202, 0x00000200,
	0x00800202, 0x00808002, 0x00800000, 0x00000002,
	0x00000202, 0x00800200, 0x00800200, 0x00008200,
	0x00008200, 0x00808000, 0x00808000, 0x00800202,
	0x00008002, 0x00800002, 0x00800002, 0x00008002,
	0x00000000, 0x00000202, 0x00008202, 0x00800000,
	0x00008000, 0x00808202, 0x00000002, 0x00808000,
	0x00808200, 0x00800000, 0x00800000, 0x00000200,
	0x00808002, 0x00008000, 0x00008200, 0x00800002,
	0x00000200, 0x00000002, 0x00800202, 0x00008202,
	0x00808202, 0x00008002, 0x00808000, 0x00800202,
	0x00800002, 0x00000202, 0x00008202, 0x00808200,
	0x00000202, 0x00800200, 0x00800200, 0x00000000,
	0x00008002, 0x00008200, 0x00000000, 0x00808002 },

      { 0x40084010, 0x40004000, 0x00004000, 0x00084010,
	0x00080000, 0x00000010, 0x40080010, 0x40004010,
	0x40000010, 0x40084010, 0x40084000, 0x40000000,
	0x40004000, 0x00080000, 0x00000010, 0x40080010,
	0x00084000, 0x00080010, 0x40004010, 0x00000000,
	0x40000000, 0x00004000, 0x00084010, 0x40080000,
	0x00080010, 0x40000010, 0x00000000, 0x00084000,
	0x00004010, 0x40084000, 0x40080000, 0x00004010,
	0x00000000, 0x00084010, 0x40080010, 0x00080000,
	0x40004010, 0x40080000, 0x40084000, 0x00004000,
	0x40080000, 0x40004000, 0x00000010, 0x40084010,
	0x00084010, 0x00000010, 0x00004000, 0x40000000,
	0x00004010, 0x40084000, 0x00080000, 0x40000010,
	0x00080010, 0x40004010, 0x40000010, 0x00080010,
	0x00084000, 0x00000000, 0x40004000, 0x00004010,
	0x40000000, 0x40080010, 0x40084010, 0x00084000 },

      { 0x00000104, 0x04010100, 0x00000000, 0x04010004,
	0x04000100, 0x00000000, 0x00010104, 0x04000100,
	0x00010004, 0x04000004, 0x04000004, 0x00010000,
	0x04010104, 0x00010004, 0x04010000, 0x00000104,
	0x04000000, 0x00000004, 0x04010100, 0x00000100,
	0x00010100, 0x04010000, 0x04010004, 0x00010104,
	0x04000104, 0x00010100, 0x00010000, 0x04000104,
	0x00000004, 0x04010104, 0x00000100, 0x04000000,
	0x04010100, 0x04000000, 0x00010004, 0x00000104,
	0x00010000, 0x04010100, 0x04000100, 0x00000000,
	0x00000100, 0x00010004, 0x04010104, 0x04000100,
	0x04000004, 0x00000100, 0x00000000, 0x04010004,
	0x04000104, 0x00010000, 0x04000000, 0x04010104,
	0x00000004, 0x00010104, 0x00010100, 0x04000004,
	0x04010000, 0x04000104, 0x00000104, 0x04010000,
	0x00010104, 0x00000004, 0x04010004, 0x00010100 },

      { 0x80401000, 0x80001040, 0x80001040, 0x00000040,
	0x00401040, 0x80400040, 0x80400000, 0x80001000,
	0x00000000, 0x00401000, 0x00401000, 0x80401040,
	0x80000040, 0x00000000, 0x00400040, 0x80400000,
	0x80000000, 0x00001000, 0x00400000, 0x80401000,
	0x00000040, 0x00400000, 0x80001000, 0x00001040,
	0x80400040, 0x80000000, 0x00001040, 0x00400040,
	0x00001000, 0x00401040, 0x80401040, 0x80000040,
	0x00400040, 0x80400000, 0x00401000, 0x80401040,
	0x80000040, 0x00000000, 0x00000000, 0x00401000,
	0x00001040, 0x00400040, 0x80400040, 0x80000000,
	0x80401000, 0x80001040, 0x80001040, 0x00000040,
	0x80401040, 0x80000040, 0x80000000, 0x00001000,
	0x80400000, 0x80001000, 0x00401040, 0x80400040,
	0x80001000, 0x00001040, 0x00400000, 0x80401000,
	0x00000040, 0x00400000, 0x00001000, 0x00401040 },

      { 0x00000080, 0x01040080, 0x01040000, 0x21000080,
	0x00040000, 0x00000080, 0x20000000, 0x01040000,
	0x20040080, 0x00040000, 0x01000080, 0x20040080,
	0x21000080, 0x21040000, 0x00040080, 0x20000000,
	0x01000000, 0x20040000, 0x20040000, 0x00000000,
	0x20000080, 0x21040080, 0x21040080, 0x01000080,
	0x21040000, 0x20000080, 0x00000000, 0x21000000,
	0x01040080, 0x01000000, 0x21000000, 0x00040080,
	0x00040000, 0x21000080, 0x00000080, 0x01000000,
	0x20000000, 0x01040000, 0x21000080, 0x20040080,
	0x01000080, 0x20000000, 0x21040000, 0x01040080,
	0x20040080, 0x00000080, 0x01000000, 0x21040000,
	0x21040080, 0x00040080, 0x21000000, 0x21040080,
	0x01040000, 0x00000000, 0x20040000, 0x21000000,
	0x00040080, 0x01000080, 0x20000080, 0x00040000,
	0x00000000, 0x20040000, 0x01040080, 0x20000080 },

      { 0x10000008, 0x10200000, 0x00002000, 0x10202008,
	0x10200000, 0x00000008, 0x10202008, 0x00200000,
	0x10002000, 0x00202008, 0x00200000, 0x10000008,
	0x00200008, 0x10002000, 0x10000000, 0x00002008,
	0x00000000, 0x00200008, 0x10002008, 0x00002000,
	0x00202000, 0x10002008, 0x00000008, 0x10200008,
	0x10200008, 0x00000000, 0x00202008, 0x10202000,
	0x00002008, 0x00202000, 0x10202000, 0x10000000,
	0x10002000, 0x00000008, 0x10200008, 0x00202000,
	0x10202008, 0x00200000, 0x00002008, 0x10000008,
	0x00200000, 0x10002000, 0x10000000, 0x00002008,
	0x10000008, 0x10202008, 0x00202000, 0x10200000,
	0x00202008, 0x10202000, 0x00000000, 0x10200008,
	0x00000008, 0x00002000, 0x10200000, 0x00202008,
	0x00002000, 0x00200008, 0x10002008, 0x00000000,
	0x10202000, 0x10000000, 0x00200008, 0x10002008 },

      { 0x00100000, 0x02100001, 0x02000401, 0x00000000,
	0x00000400, 0x02000401, 0x00100401, 0x02100400,
	0x02100401, 0x00100000, 0x00000000, 0x02000001,
	0x00000001, 0x02000000, 0x02100001, 0x00000401,
	0x02000400, 0x00100401, 0x00100001, 0x02000400,
	0x02000001, 0x02100000, 0x02100400, 0x00100001,
	0x02100000, 0x00000400, 0x00000401, 0x02100401,
	0x00100400, 0x00000001, 0x02000000, 0x00100400,
	0x02000000, 0x00100400, 0x00100000, 0x02000401,
	0x02000401, 0x02100001, 0x02100001, 0x00000001,
	0x00100001, 0x02000000, 0x02000400, 0x00100000,
	0x02100400, 0x00000401, 0x00100401, 0x02100400,
	0x00000401, 0x02000001, 0x02100401, 0x02100000,
	0x00100400, 0x00000000, 0x00000001, 0x02100401,
	0x00000000, 0x00100401, 0x02100000, 0x00000400,
	0x02000001, 0x02000400, 0x00000400, 0x00100001 },

      { 0x08000820, 0x00000800, 0x00020000, 0x08020820,
	0x08000000, 0x08000820, 0x00000020, 0x08000000,
	0x00020020, 0x08020000, 0x08020820, 0x00020800,
	0x08020800, 0x00020820, 0x00000800, 0x00000020,
	0x08020000, 0x08000020, 0x08000800, 0x00000820,
	0x00020800, 0x00020020, 0x08020020, 0x08020800,
	0x00000820, 0x00000000, 0x00000000, 0x08020020,
	0x08000020, 0x08000800, 0x00020820, 0x00020000,
	0x00020820, 0x00020000, 0x08020800, 0x00000800,
	0x00000020, 0x08020020, 0x00000800, 0x00020820,
	0x08000800, 0x00000020, 0x08000020, 0x08020000,
	0x08020020, 0x08000000, 0x00020000, 0x08000820,
	0x00000000, 0x08020820, 0x00020020, 0x08000020,
	0x08020000, 0x08000800, 0x08000820, 0x00000000,
	0x08020820, 0x00020800, 0x00020800, 0x00000820,
	0x00000820, 0x00020020, 0x08000000, 0x08020800 }
};



/*
 * DESauth_des - perform an in place DES encryption on 64 bits
 *
 * Note that the `data' argument is always in big-end-first
 * byte order, i.e. *(char *)data is the high order byte of
 * the 8 byte data word.  We modify the initial and final
 * permutation computations for little-end-first machines to
 * swap bytes into the natural host order at the beginning and
 * back to big-end order at the end.  This is unclean but avoids
 * a byte swapping performance penalty on Vaxes (which are slow already).
 */
void
DESauth_des(data, subkeys)
	U_LONG *data;
	u_char *subkeys;
{
	register U_LONG left, right;
	register U_LONG temp;
	register u_char *kp;
	register int i;

	/*
	 * Do the initial permutation.  The first operation gets
	 * all the bits which are used to form the left half of the
	 * permutted result in one word, which is then used to
	 * index the appropriate table a byte at a time.
	 */
	temp = ((*data & 0x55555555) << 1) | (*(data+1) & 0x55555555);
#ifdef XNTP_LITTLE_ENDIAN
	/*
	 * Modify the computation to use the opposite set of bytes.
	 */
	left = (IP[(temp >> 24) & 0xff] << 3)
	     | (IP[(temp >> 16) & 0xff] << 2)
	     | (IP[(temp >>  8) & 0xff] << 1)
	     | IP[temp & 0xff];
#else
	left = IP[(temp >> 24) & 0xff]
	     | (IP[(temp >> 16) & 0xff] << 1)
	     | (IP[(temp >>  8) & 0xff] << 2)
	     | (IP[temp & 0xff] << 3);
#endif
	
	/*
	 * Same thing again except for the right half.
	 */
	temp = (*data & 0xaaaaaaaa) | ((*(data+1) & 0xaaaaaaaa) >> 1);
#ifdef XNTP_LITTLE_ENDIAN
	right = (IP[(temp >> 24) & 0xff] << 3)
	      | (IP[(temp >> 16) & 0xff] << 2)
	      | (IP[(temp >>  8) & 0xff] << 1)
	      | IP[temp & 0xff];
#else
	right = IP[(temp >> 24) & 0xff]
	      | (IP[(temp >> 16) & 0xff] << 1)
	      | (IP[(temp >>  8) & 0xff] << 2)
	      | (IP[temp & 0xff] << 3);
#endif
	
	/*
	 * Do the 16 rounds through the cipher function.  We actually
	 * do two at a time, one on the left half and one on the right
	 * half.
	 */
	kp = subkeys;
	for (i = 0; i < 8; i++) {
		/*
		 * The E expansion is easy to compute algorithmically.
		 * Take a look at its form and compare it to
		 * everything involving temp below.  Note that
		 * since SP[0-7] don't have any bits in common set
		 * it is okay to do the successive xor's.
		 */
		temp = (right >> 1) | ((right & 1) ? 0x80000000 : 0);
		left ^= SP[0][((temp >> 26) & 0x3f) ^ *kp++];
		left ^= SP[1][((temp >> 22) & 0x3f) ^ *kp++];
		left ^= SP[2][((temp >> 18) & 0x3f) ^ *kp++];
		left ^= SP[3][((temp >> 14) & 0x3f) ^ *kp++];
		left ^= SP[4][((temp >> 10) & 0x3f) ^ *kp++];
		left ^= SP[5][((temp >>  6) & 0x3f) ^ *kp++];
		left ^= SP[6][((temp >>  2) & 0x3f) ^ *kp++];
		left ^= SP[7][(((right << 1) | ((right & 0x80000000)?1:0))
				& 0x3f) ^ *kp++];
		
		/*
		 * Careful here.  Right now `right' is actually the
		 * left side and `left' is the right side.  Do the
		 * same thing again, except swap `left' and `right'
		 */
		temp = (left >> 1) | ((left & 1) ? 0x80000000 : 0);
		right ^= SP[0][((temp >> 26) & 0x3f) ^ *kp++];
		right ^= SP[1][((temp >> 22) & 0x3f) ^ *kp++];
		right ^= SP[2][((temp >> 18) & 0x3f) ^ *kp++];
		right ^= SP[3][((temp >> 14) & 0x3f) ^ *kp++];
		right ^= SP[4][((temp >> 10) & 0x3f) ^ *kp++];
		right ^= SP[5][((temp >>  6) & 0x3f) ^ *kp++];
		right ^= SP[6][((temp >>  2) & 0x3f) ^ *kp++];
		right ^= SP[7][(((left << 1) | ((left & 0x80000000)?1:0))
				& 0x3f) ^ *kp++];

		/*
		 * By the time we get here, all is straightened out
		 * again.  `left' is left and `right' is right.
		 */
	}

	/*
	 * Now the final permutation.  Note this is like the IP above
	 * except that the data is computed from
	 *
 	 * ((left & 0x0f0f0f0f) << 4) | (right & 0x0f0f0f0f)  for left result
 	 * (left & 0xf0f0f0f0) | ((right & 0xf0f0f0f0) >> 4)  for right result
	 *
	 * Just to confuse things more, we're supposed to swap the right
	 * and the left halves before doing this.  Instead, we'll just
	 * switch which goes where when computing the temporary.
	 *
	 * This operation also byte swaps stuff back into big end byte
	 * order.  This is accomplished by modifying the FP table for
	 * little end machines, however, so we don't have to worry about
	 * it here.
	 */
	temp = ((right & 0x0f0f0f0f) << 4) | (left & 0x0f0f0f0f);
	*data = (FP[(temp >> 24) & 0xff] << 6)
	      | (FP[(temp >> 16) & 0xff] << 4)
	      | (FP[(temp >>  8) & 0xff] << 2)
	      |  FP[temp & 0xff];

	temp = (right & 0xf0f0f0f0) | ((left & 0xf0f0f0f0) >> 4);
	*(data+1) = (FP[(temp >> 24) & 0xff] << 6)
		  | (FP[(temp >> 16) & 0xff] << 4)
		  | (FP[(temp >>  8) & 0xff] << 2)
		  |  FP[temp & 0xff];
}
