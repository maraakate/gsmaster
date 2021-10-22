/*
	Copyright 2005,2006,2007,2008 Luigi Auriemma

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA

	http://www.gnu.org/licenses/gpl.txt

*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#endif

static const unsigned char enctype1_master_key[] = /* pre-built */
"\x01\xba\xfa\xb2\x51\x00\x54\x80\x75\x16\x8e\x8e\x02\x08\x36\xa5"
"\x2d\x05\x0d\x16\x52\x07\xb4\x22\x8c\xe9\x09\xd6\xb9\x26\x00\x04"
"\x06\x05\x00\x13\x18\xc4\x1e\x5b\x1d\x76\x74\xfc\x50\x51\x06\x16"
"\x00\x51\x28\x00\x04\x0a\x29\x78\x51\x00\x01\x11\x52\x16\x06\x4a"
"\x20\x84\x01\xa2\x1e\x16\x47\x16\x32\x51\x9a\xc4\x03\x2a\x73\xe1"
"\x2d\x4f\x18\x4b\x93\x4c\x0f\x39\x0a\x00\x04\xc0\x12\x0c\x9a\x5e"
"\x02\xb3\x18\xb8\x07\x0c\xcd\x21\x05\xc0\xa9\x41\x43\x04\x3c\x52"
"\x75\xec\x98\x80\x1d\x08\x02\x1d\x58\x84\x01\x4e\x3b\x6a\x53\x7a"
"\x55\x56\x57\x1e\x7f\xec\xb8\xad\x00\x70\x1f\x82\xd8\xfc\x97\x8b"
"\xf0\x83\xfe\x0e\x76\x03\xbe\x39\x29\x77\x30\xe0\x2b\xff\xb7\x9e"
"\x01\x04\xf8\x01\x0e\xe8\x53\xff\x94\x0c\xb2\x45\x9e\x0a\xc7\x06"
"\x18\x01\x64\xb0\x03\x98\x01\xeb\x02\xb0\x01\xb4\x12\x49\x07\x1f"
"\x5f\x5e\x5d\xa0\x4f\x5b\xa0\x5a\x59\x58\xcf\x52\x54\xd0\xb8\x34"
"\x02\xfc\x0e\x42\x29\xb8\xda\x00\xba\xb1\xf0\x12\xfd\x23\xae\xb6"
"\x45\xa9\xbb\x06\xb8\x88\x14\x24\xa9\x00\x14\xcb\x24\x12\xae\xcc"
"\x57\x56\xee\xfd\x08\x30\xd9\xfd\x8b\x3e\x0a\x84\x46\xfa\x77\xb8";

unsigned char  enc1key[261];

int write_enctype1_encrypted_data(char *output, const char *validate_key, unsigned char *scramble_data, int scrambled_len, char *encrypted_data, int encrypted_len, unsigned int encshare4_data);
void obfuscate_scramble_data(unsigned char *scramble_data, int scramble_len, const char *master_key, int master_key_len);
int find_master_key_offset(char key, const char *master_key, int master_key_len);

void encshare1(unsigned int *tbuff, unsigned char *datap, int len);
void encshare4(unsigned char *src, int size, unsigned int *dest);

unsigned char *enctype1_decoder(unsigned char *id, unsigned char *, int *);
void func2(unsigned char *, int, unsigned char *);
void func3(unsigned char *, int, unsigned char *);
void func6(unsigned char *, int, unsigned char *);
void func6e(unsigned char *, int, unsigned char *);
int  func7(int, unsigned char *);
int  func7e(char v, unsigned char *);
void func4(const unsigned char *, int, unsigned char *);
int  func5(int, const unsigned char *, int, int *, int *, unsigned char *);
void func8(unsigned char *, int, const unsigned char *);

void encshare2 (unsigned int *tbuff, unsigned int *tbuffp, int len)
{
	unsigned int    t1, t2, t3, t4, t5, *limit, *p;

	t2 = tbuff[304];
	t1 = tbuff[305];
	t3 = tbuff[306];
	t5 = tbuff[307];
	limit = tbuffp + len;
	while (tbuffp < limit)
	{
		p = tbuff + t2 + 272;
		while (t5 < 65536)
		{
			t1 += t5;
			p++;
			t3 += t1;
			t1 += t3;
			p[-17] = t1;
			p[-1] = t3;
			t4 = (t3 << 24) | (t3 >> 8);
			p[15] = t5;
			t5 <<= 1;
			t2++;
			t1 ^= tbuff[t1 & 0xff];
			t4 ^= tbuff[t4 & 0xff];
			t3 = (t4 << 24) | (t4 >> 8);
			t4 = (t1 >> 24) | (t1 << 8);
			t4 ^= tbuff[t4 & 0xff];
			t3 ^= tbuff[t3 & 0xff];
			t1 = (t4 >> 24) | (t4 << 8);
		}
		t3 ^= t1;
		*tbuffp++ = t3;
		t2--;
		t1 = tbuff[t2 + 256];
		t5 = tbuff[t2 + 272];
		t1 = ~t1;
		t3 = (t1 << 24) | (t1 >> 8);
		t3 ^= tbuff[t3 & 0xff];
		t5 ^= tbuff[t5 & 0xff];
		t1 = (t3 << 24) | (t3 >> 8);
		t4 = (t5 >> 24) | (t5 << 8);
		t1 ^= tbuff[t1 & 0xff];
		t4 ^= tbuff[t4 & 0xff];
		t3 = (t4 >> 24) | (t4 << 8);
		t5 = (tbuff[t2 + 288] << 1) + 1;
	}
	tbuff[304] = t2;
	tbuff[305] = t1;
	tbuff[306] = t3;
	tbuff[307] = t5;
}

void encshare1 (unsigned int *tbuff, unsigned char *datap, int len)
{
	unsigned char *p, *s;

	p = s = (unsigned char *)(tbuff + 309);
	encshare2(tbuff, (unsigned int *)p, 16);

	while (len--)
	{
		if ((p - s) == 63)
		{
			p = s;
			encshare2(tbuff, (unsigned int *)p, 16);
		}
		*datap ^= *p;
		datap++;
		p++;
	}
}

void encshare3 (unsigned int *data, int n1, int n2)
{
	unsigned int	t1, t2, t3, t4;
	int				i;

	t2 = n1;
	t1 = 0;
	t4 = 1;
	data[304] = 0;
	for (i = 32768; i; i >>= 1)
	{
		t2 += t4;
		t1 += t2;
		t2 += t1;
		if (n2 & i)
		{
			t2 = ~t2;
			t4 = (t4 << 1) + 1;
			t3 = (t2 << 24) | (t2 >> 8);
			t3 ^= data[t3 & 0xff];
			t1 ^= data[t1 & 0xff];
			t2 = (t3 << 24) | (t3 >> 8);
			t3 = (t1 >> 24) | (t1 << 8);
			t2 ^= data[t2 & 0xff];
			t3 ^= data[t3 & 0xff];
			t1 = (t3 >> 24) | (t3 << 8);
		}
		else
		{
			data[data[304] + 256] = t2;
			data[data[304] + 272] = t1;
			data[data[304] + 288] = t4;
			data[304]++;
			t3 = (t1 << 24) | (t1 >> 8);
			t2 ^= data[t2 & 0xff];
			t3 ^= data[t3 & 0xff];
			t1 = (t3 << 24) | (t3 >> 8);
			t3 = (t2 >> 24) | (t2 << 8);
			t3 ^= data[t3 & 0xff];
			t1 ^= data[t1 & 0xff];
			t2 = (t3 >> 24) | (t3 << 8);
			t4 <<= 1;
		}
	}
	data[305] = t2;
	data[306] = t1;
	data[307] = t4;
	data[308] = n1;
//    t1 ^= t2;
}

void encshare4 (unsigned char *src, int size, unsigned int *dest)
{
	unsigned int    tmp;
	int             i;
	unsigned char   pos, x, y;

	for (i = 0; i < 256; i++)
		dest[i] = 0;

	for (y = 0; y < 4; y++)
	{
		for (i = 0; i < 256; i++)
		{
			dest[i] = (dest[i] << 8) + i;
		}

		for (pos = y, x = 0; x < 2; x++)
		{
			for (i = 0; i < 256; i++)
			{
				tmp = dest[i];
				pos += tmp + src[i % size];
				dest[i] = dest[pos];
				dest[pos] = tmp;
			}
		}
	}

	for (i = 0; i < 256; i++)
		dest[i] ^= i;

	encshare3(dest, 0, 0);
}

// L00429D10
unsigned char *enctype1_decoder (unsigned char *id, unsigned char *data, int *datalen)
{
	unsigned int    tbuff[326];
	int             i, len, tmplen;
	unsigned char   tbuff2[258];
	unsigned char *datap;

	len = (data[0] << 24) | (data[1] << 16) | (data[2] << 8) | (data[3]);
	if ((len < 0) || (len > *datalen))
	{
		*datalen = 0;
		return(data);
	}

	data[4] = (data[4] ^ 62) - 20;
	data[5] = (data[5] ^ 205) - 5;
	func8(data + 19, 16, enctype1_master_key);

	len -= data[4] + data[5] + 40;
	datap = data + data[5] + 40;

	tmplen = (len >> 2) - 5;
	if (tmplen >= 0)
	{
		func4(id, strlen(id), enc1key);
		func6(datap, tmplen, enc1key);
		memset(enc1key, 0, sizeof(enc1key));
	}

	/* added by me */
	for (i = 256; i < 326; i++)
		tbuff[i] = 0;

	tmplen = (len >> 1) - 17;
	if (tmplen >= 0)
	{
		encshare4(data + 36, 4, tbuff);
		encshare1(tbuff, datap, tmplen);
	}

	memset(tbuff2, 0, sizeof(tbuff2));
	func3(data + 19, 16, tbuff2);
	func2(datap, len, tbuff2);

	*datalen = len;
	return(datap);
}

int create_enctype1_buffer (const char *validate_key, char *input, int inputLen, char *output)
{
	char *encrypt_data = input;
	int encrypt_len = inputLen;
	int encshare4_data = rand();
	int tmplen = (encrypt_len >> 1) - 17;
	unsigned int    tbuff[326];
	unsigned char  enc1key[261];
	char scramble_data[16];
	char encryption_key[258];
	int i;

	memset(&tbuff, 0, sizeof(tbuff));

	if (tmplen >= 0)
	{
		encshare4((char *)&encshare4_data, sizeof(encshare4_data), tbuff);
		encshare1(tbuff, encrypt_data, tmplen);
	}

	for (i = 0; i < sizeof(scramble_data); i++)
	{
		unsigned char key = rand();
		scramble_data[i] = enctype1_master_key[key];
	}

	func3(scramble_data, sizeof(scramble_data), encryption_key);
	func2((char *)encrypt_data, encrypt_len, (char *)&encryption_key); //encrypt actual data

	tmplen = (encrypt_len >> 2) - 5;
	if (tmplen >= 0)
	{
		func4(validate_key, strlen(validate_key), enc1key);
		func6e(encrypt_data, tmplen, enc1key);
	}

	obfuscate_scramble_data(scramble_data, sizeof(scramble_data), enctype1_master_key, sizeof(enctype1_master_key));

	return write_enctype1_encrypted_data(output, validate_key, scramble_data, sizeof(scramble_data), (char *)encrypt_data, encrypt_len, encshare4_data);
}

int find_master_key_offset (char key, const char *master_key, int master_key_len)
{
	int i;

	for (i = 0; i < master_key_len; i++)
	{
		if (master_key[i] == key)
		{
			return i;
		}
	}

	return -1;
}

/*
This routine will only return the first instance in the master key... so it is not really secure
*/
void obfuscate_scramble_data (unsigned char *scramble_data, int scramble_len, const char *master_key, int master_key_len)
{
	int i;
	for (i = 0; i < scramble_len; i++)
	{
		int offset = find_master_key_offset(scramble_data[i], master_key, master_key_len);
		if (offset != -1)
		{
			scramble_data[i] = offset;
		}
	}
}

int write_enctype1_encrypted_data (char *output, const char *validate_key, unsigned char *scramble_data, int scrambled_len, char *encrypted_data, int encrypted_len, unsigned int encshare4_data)
{
	char *head = output;
	int outputLen = 0;
	int i;
	int total_len;
	int *len_address;
	int out1 = 0;
	static const unsigned char out2 = 42;
	static const unsigned char out3 = 218;
	unsigned char randout = 0;

	memcpy(output + outputLen, &out1, sizeof(out1));
	outputLen += sizeof(out1);

	memcpy(output + outputLen, &out2, sizeof(out2));
	outputLen += sizeof(out2);

	memcpy(output + outputLen, &out3, sizeof(out3));
	outputLen += sizeof(out3);

	for (i = 0; i < 13; i++)
	{ //unused data...?
		randout = rand();
		memcpy(output + outputLen, &randout, sizeof(randout));
		outputLen += sizeof(randout);
	}

	memcpy(output + outputLen, scramble_data, scrambled_len);
	outputLen += scrambled_len;

	// unknown data
	randout = rand();
	memcpy(output + outputLen, &randout, sizeof(randout));
	outputLen += sizeof(randout);

	memcpy(output + outputLen, &encshare4_data, sizeof(encshare4_data));
	outputLen += sizeof(encshare4_data);

	for (i = 0; i < 18; i++)
	{ //unused data...?
		randout = rand();
		memcpy(output + outputLen, &randout, sizeof(randout));
		outputLen += sizeof(randout);
	}

	memcpy(output + outputLen, encrypted_data, encrypted_len);
	outputLen += encrypted_len;

	total_len = outputLen;
	len_address = (int *)head;
	*len_address = htonl(total_len);

	return total_len;
}

void func2 (unsigned char *data, int size, unsigned char *crypt)
{
	unsigned char   n1, n2, t;

	n1 = crypt[256];
	n2 = crypt[257];
	while (size--)
	{
		t = crypt[++n1];
		n2 += t;
		crypt[n1] = crypt[n2];
		crypt[n2] = t;
		t += crypt[n1];
		*data++ ^= crypt[t];
	}
	crypt[256] = n1;
	crypt[257] = n2;
}

void func3 (unsigned char *data, int len, unsigned char *buff)
{
	int             i;
	unsigned char   pos = 0, tmp, rev = 0xff;

	for (i = 0; i < 256; i++)
	{
		buff[i] = rev--;
	}

	buff[256] = 0;
	buff[257] = 0;
	for (i = 0; i < 256; i++)
	{
		tmp = buff[i];
		pos += data[i % len] + tmp;
		buff[i] = buff[pos];
		buff[pos] = tmp;
	}
}

void func4 (const unsigned char *id, int idlen, unsigned char *enc1key)
{
	int             i, n1 = 0, n2 = 0;
	unsigned char   t1, t2;

	if (idlen < 1)
		return;

	for (i = 0; i < 256; i++)
		enc1key[i] = i;

	for (i = 255; i >= 0; i--)
	{
		t1 = func5(i, id, idlen, &n1, &n2, enc1key);
		t2 = enc1key[i];
		enc1key[i] = enc1key[t1];
		enc1key[t1] = t2;
	}

	enc1key[256] = enc1key[1];
	enc1key[257] = enc1key[3];
	enc1key[258] = enc1key[5];
	enc1key[259] = enc1key[7];
	enc1key[260] = enc1key[n1 & 0xff];
}

int func5 (int cnt, const unsigned char *id, int idlen, int *n1, int *n2, unsigned char *enc1key)
{
	int     i, tmp, mask = 1;

	if (!cnt)
		return(0);
	if (cnt > 1)
	{
		do
		{
			mask = (mask << 1) + 1;
		} while (mask < cnt);
	}

	i = 0;
	do
	{
		*n1 = enc1key[*n1 & 0xff] + id[*n2];
		(*n2)++;
		if (*n2 >= idlen)
		{
			*n2 = 0;
			*n1 += idlen;
		}
		tmp = *n1 & mask;
		if (++i > 11) tmp %= cnt;
	} while (tmp > cnt);

	return(tmp);
}

void func6 (unsigned char *data, int len, unsigned char *enc1key)
{
	while (len--)
	{
		*data = func7(*data, enc1key);
		data++;
	}
}

void func6e (unsigned char *data, int len, unsigned char *enc1key)
{
	while (len--)
	{
		*data = func7e(*data, enc1key);
		data++;
	}
}

int func7 (int len, unsigned char *enc1key)
{
	unsigned char   a, b, c;

	a = enc1key[256];
	b = enc1key[257];
	c = enc1key[a];
	enc1key[256] = a + 1;
	enc1key[257] = b + c;
	a = enc1key[260];
	b = enc1key[257];
	b = enc1key[b];
	c = enc1key[a];
	enc1key[a] = b;
	a = enc1key[259];
	b = enc1key[257];
	a = enc1key[a];
	enc1key[b] = a;
	a = enc1key[256];
	b = enc1key[259];
	a = enc1key[a];
	enc1key[b] = a;
	a = enc1key[256];
	enc1key[a] = c;
	b = enc1key[258];
	a = enc1key[c];
	c = enc1key[259];
	b = b + a;
	enc1key[258] = b;
	a = b;
	c = enc1key[c];
	b = enc1key[257];
	b = enc1key[b];
	a = enc1key[a];
	c += b;
	b = enc1key[260];
	b = enc1key[b];
	c += b;
	b = enc1key[c];
	c = enc1key[256];
	c = enc1key[c];
	a += c;
	c = enc1key[b];
	b = enc1key[a];
	a = len;
	c ^= b;
	enc1key[260] = a;
	c ^= a;
	enc1key[259] = c;
	return(c);
}

int func7e (char v, unsigned char *enc1key)
{
	unsigned char   a, b, c;

	a = enc1key[256];
	b = enc1key[257];
	c = enc1key[a];
	enc1key[256] = a + 1;
	enc1key[257] = b + c;
	a = enc1key[260];
	b = enc1key[257];
	b = enc1key[b];
	c = enc1key[a];
	enc1key[a] = b;
	a = enc1key[259];
	b = enc1key[257];
	a = enc1key[a];
	enc1key[b] = a;
	a = enc1key[256];
	b = enc1key[259];
	a = enc1key[a];
	enc1key[b] = a;
	a = enc1key[256];
	enc1key[a] = c;
	b = enc1key[258];
	a = enc1key[c];
	c = enc1key[259];
	b = b + a;
	enc1key[258] = b;
	a = b;

	c = enc1key[c];
	b = enc1key[257];
	b = enc1key[b];
	a = enc1key[a];
	c += b;
	b = enc1key[260];
	b = enc1key[b];
	c += b;

	b = enc1key[c];
	c = enc1key[256];
	c = enc1key[c];
	a += c;

	c = enc1key[b];
	b = enc1key[a];

	c ^= b;
	c ^= v;

	enc1key[260] = c;
	enc1key[259] = v;
	return c;
}

void func8 (unsigned char *data, int len, const unsigned char *enctype1_data)
{
	while (len--)
	{
		*data = enctype1_data[*data];
		data++;
	}
}

int enctype1_wrapper (unsigned char *key, unsigned char *data, int size)
{
	unsigned char *p;

	p = enctype1_decoder(key, data, &size);
	memmove(data, p, size);
	return(size);
}
