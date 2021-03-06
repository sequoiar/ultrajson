/*
Copyright (c) 2011, Jonas Tarnstrom and ESN Social Software AB
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
1. Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.
2. Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.
3. All advertising materials mentioning features or use of this software
   must display the following acknowledgement:
   This product includes software developed by ESN Social Software AB (www.esn.me).
4. Neither the name of the ESN Social Software AB nor the
   names of its contributors may be used to endorse or promote products
   derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY ESN SOCIAL SOFTWARE AB ''AS IS'' AND ANY
EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL ESN SOCIAL SOFTWARE AB BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

Portions of code from:
MODP_ASCII - Ascii transformations (upper/lower, etc)
http://code.google.com/p/stringencoders/
Copyright (c) 2007  Nick Galbreath -- nickg [at] modp [dot] com. All rights reserved.

*/

#include "ultrajson.h"
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <float.h>

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

static const double g_pow10[] = {1, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000, 1000000000};
static const char g_hexChars[] = "0123456789abcdef";
static const char g_escapeChars[] = "0123456789\\b\\t\\n\\f\\r\\\"\\\\";


/*
FIXME: While this is fine dandy and working it's a magic value mess which probably only the author understands.
Needs a cleanup and more documentation */
static const JSUINT8 g_utf8LengthLookup[256] = 
{
/* 0x00 */ 0, 30, 30, 30, 30, 30, 30, 30, 10, 12, 14, 30, 16, 18, 30, 30, 
/* 0x10 */ 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30,
/* 0x20 */ 1, 1, 20, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
/* 0x30 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 0x40 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
/* 0x50 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 22, 1, 1, 1,
/* 0x60 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
/* 0x70 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 0x80 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
/* 0x90 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 0xa0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
/* 0xb0 */ 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
/* 0xc0 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
/* 0xd0 */ 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
/* 0xe0 */ 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
/* 0xf0 */ 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 1, 1
};

static void SetError (JSOBJ obj, JSONObjectEncoder *enc, const char *message)
{
	enc->errorMsg = message;
	enc->errorObj = obj;
}

/*
FIXME: Keep track of how big these get across several encoder calls and try to make an estimate
That way we won't run our head into the wall each call */
void Buffer_Realloc (JSONObjectEncoder *enc, size_t cbNeeded)
{
	size_t curSize = enc->end - enc->start;
	size_t newSize = curSize * 2;
	size_t offset = enc->offset - enc->start;

	while (newSize < curSize + cbNeeded)
	{
		newSize *= 2;
	}

	if (enc->heap)
	{
		enc->start = (char *) enc->realloc (enc->start, newSize);
	}
	else
	{
		char *oldStart = enc->start;
		enc->heap = 1;
		enc->start = (char *) enc->malloc (newSize);
		memcpy (enc->start, oldStart, offset);
	}
	enc->offset = enc->start + offset;
	enc->end = enc->start + newSize;
}

FASTCALL_ATTR INLINE_PREFIX void FASTCALL_MSVC Buffer_AppendShortHexUnchecked (char *outputOffset, unsigned short value)
{
	*(outputOffset++) = g_hexChars[(value & 0xf000) >> 12];
	*(outputOffset++) = g_hexChars[(value & 0x0f00) >> 8];
	*(outputOffset++) = g_hexChars[(value & 0x00f0) >> 4];
	*(outputOffset++) = g_hexChars[(value & 0x000f) >> 0];
}

/*
FIXME:
This code only works with Little and Big Endian

FIXME: The JSON spec says escape "/" but non of the others do and we don't 
want to be left alone doing it so we don't :)

FIXME: It should be faster to do SHIFT and then AND instead of AND and SHIFT.
Example:
(x & 0x3f00) >> 8) => Longer/more opcodes than below
(x >> 8) & 0x3f)   => Probably faster/smaller
Seems that atleast MSVC9 does this optimization by itself from time to time. Not sure really

*/

int Buffer_EscapeString (JSOBJ obj, JSONObjectEncoder *enc, const char *io, const char *end)
{
	JSUTF32 ucs;
	char *of = (char *) enc->offset;
	
	while (1)
	{

		//JSUINT8 chr = (unsigned char) *io;
		JSUINT8 utflen = g_utf8LengthLookup[(unsigned char) *io];

		switch (utflen)
		{
			case 0: 
			{
				enc->offset += (of - enc->offset); 
				return TRUE;
			}

			case 1:
			{
				*(of++)= (*io++); 
				continue;
			}

			case 2:
			{
				JSUTF32 in;

				if (io + 1 > end)
				{
					enc->offset += (of - enc->offset);
					SetError (obj, enc, "Unterminated UTF-8 sequence when encoding string");
					return FALSE;
				}

				in = *((JSUTF16 *) io);

#ifdef __LITTLE_ENDIAN__
				ucs = ((in & 0x1f) << 6) | ((in >> 8) & 0x3f);
#else
				ucs = ((in & 0x1f00) >> 2) | (in & 0x3f);
#endif

				if (ucs < 0x80)
				{
					enc->offset += (of - enc->offset);
					SetError (obj, enc, "Overlong 2 byte UTF-8 sequence detected when encoding string");
					return FALSE;
				}

				io += 2;
				break;
			}

			case 3:
			{
				JSUTF32 in;

				if (io + 2 > end)
				{
					enc->offset += (of - enc->offset);
					SetError (obj, enc, "Unterminated UTF-8 sequence when encoding string");
					return FALSE;
				}

#ifdef __LITTLE_ENDIAN__
				in = *((JSUTF16 *) io);
				in |= *((JSUINT8 *) io + 2) << 16;
				ucs = ((in & 0x0f) << 12) | ((in & 0x3f00) >> 2) | ((in & 0x3f0000) >> 16);
#else
				in = *((JSUTF16 *) io) << 8;
				in |= *((JSUINT8 *) io + 2);
				ucs = ((in & 0x0f0000) >> 4) | ((in & 0x3f00) >> 2) | (in & 0x3f);
#endif


				if (ucs < 0x800)
				{
					enc->offset += (of - enc->offset);
					SetError (obj, enc, "Overlong 3 byte UTF-8 sequence detected when encoding string");
					return FALSE;
				}

				io += 3;
				break;
			}
			case 4:
			{
				JSUTF32 in;
				
				if (io + 3 > end)
				{
					enc->offset += (of - enc->offset);
					SetError (obj, enc, "Unterminated UTF-8 sequence when encoding string");
					return FALSE;
				}

#ifdef __LITTLE_ENDIAN__
				in = *((JSUTF32 *) io);
				ucs = ((in & 0x07) << 18) | ((in & 0x3f00) << 4) | ((in & 0x3f0000) >> 10) | ((in & 0x3f000000) >> 24);
#else
				in = *((JSUTF32 *) io);
				ucs = ((in & 0x07000000) >> 6) | ((in & 0x3f0000) >> 4) | ((in & 0x3f00) >> 2) | (in & 0x3f);
#endif
				if (ucs < 0x10000)
				{
					enc->offset += (of - enc->offset);
					SetError (obj, enc, "Overlong 4 byte UTF-8 sequence detected when encoding string");
					return FALSE;
				}

				io += 4;
				break;
			}


			case 5:
			case 6:
				enc->offset += (of - enc->offset);
				SetError (obj, enc, "Unsupported UTF-8 sequence length when encoding string");
				return FALSE;

			case 30:
				// \uXXXX encode
				*(of++) = '\\';
				*(of++) = 'u';
				*(of++) = '0';
				*(of++) = '0';
				*(of++) = g_hexChars[ (unsigned char) (((*io) & 0xf0) >> 4)];
				*(of++) = g_hexChars[ (unsigned char) ((*io) & 0x0f)];
				io ++;
				continue;

			case 10:
			case 12:
			case 14:
			case 16:
			case 18:
			case 20:
			case 22:
				*(of++) = *( (char *) (g_escapeChars + utflen + 0));
				*(of++) = *( (char *) (g_escapeChars + utflen + 1));
				io ++;
				continue;
		}

		/*
		If the character is a UTF8 sequence of length > 1 we end up here */
		if (ucs > 0x10000)
		{
			ucs -= 0x10000;
			*(of++) = '\\';
			*(of++) = 'u';
			Buffer_AppendShortHexUnchecked(of, (ucs >> 10) + 0xd800);
			of += 4;

			*(of++) = '\\';
			*(of++) = 'u';
			Buffer_AppendShortHexUnchecked(of, (ucs & 0x3ff) + 0xdc00);
			of += 4;
		}
		else
		{
			*(of++) = '\\';
			*(of++) = 'u';
			Buffer_AppendShortHexUnchecked(of, ucs);
			of += 4;
		}
	}

	return FALSE;
}

#define Buffer_Reserve(__enc, __len) \
	if ((__enc)->offset + (__len) > (__enc)->end)	\
	{	\
		Buffer_Realloc((__enc), (__len));\
	}	\


#define Buffer_AppendCharUnchecked(__enc, __chr) \
				*((__enc)->offset++) = __chr; \

FASTCALL_ATTR INLINE_PREFIX void FASTCALL_MSVC strreverse(char* begin, char* end)
{
	char aux;
	while (end > begin)
	aux = *end, *end-- = *begin, *begin++ = aux;
}

void Buffer_AppendIntUnchecked(JSONObjectEncoder *enc, JSINT32 value)
{
	char* wstr;
	JSUINT32 uvalue = (value < 0) ? -value : value;

	wstr = enc->offset;
	// Conversion. Number is reversed.
	
	do *wstr++ = (char)(48 + (uvalue % 10)); while(uvalue /= 10);
	if (value < 0) *wstr++ = '-';

	// Reverse string
	strreverse(enc->offset,wstr - 1);
	enc->offset += (wstr - (enc->offset));
}

void Buffer_AppendLongUnchecked(JSONObjectEncoder *enc, JSINT64 value)
{
	char* wstr;
	JSUINT64 uvalue = (value < 0) ? -value : value;

	wstr = enc->offset;
	// Conversion. Number is reversed.
	
	do *wstr++ = (char)(48 + (uvalue % 10ULL)); while(uvalue /= 10ULL);
	if (value < 0) *wstr++ = '-';

	// Reverse string
	strreverse(enc->offset,wstr - 1);
	enc->offset += (wstr - (enc->offset));
}

int Buffer_AppendDoubleUnchecked(JSOBJ obj, JSONObjectEncoder *enc, double value)
{
	/* if input is larger than thres_max, revert to exponential */
	const double thres_max = (double)(0x7FFFFFFF);
	int count;
	double diff = 0.0;
	char* str = enc->offset;
	char* wstr = str;
	int whole;
	double tmp;
	uint32_t frac;
	int neg;

	if (value == HUGE_VAL || value == -HUGE_VAL)
	{
		SetError (obj, enc, "Invalid Inf value when encoding double");
		return FALSE;
	}
	if (! (value == value)) 
	{
		SetError (obj, enc, "Invalid Nan value when encoding double");
		return FALSE;
	}


	/* we'll work in positive values and deal with the
	negative sign issue later */
	neg = 0;
	if (value < 0) 
	{
		neg = 1;
		value = -value;
	}

	//FIXME: Two lookups of same value in g_pow10 
	whole = (int) value;
	tmp = (value - whole) * g_pow10[enc->doublePrecision];
	frac = (uint32_t)(tmp);
	diff = tmp - frac;

	if (diff > 0.5) 
	{
		++frac;
		/* handle rollover, e.g.  case 0.99 with prec 1 is 1.0  */
		if (frac >= g_pow10[enc->doublePrecision]) 
		{
			frac = 0;
			++whole;
		}
	} 
	else 
	if (diff == 0.5 && ((frac == 0) || (frac & 1))) 
	{
		/* if halfway, round up if odd, OR
		if last digit is 0.  That last part is strange */
		++frac;
	}

	/* for very large numbers switch back to native sprintf for exponentials.
	anyone want to write code to replace this? */
	/*
	normal printf behavior is to print EVERY whole number digit
	which can be 100s of characters overflowing your buffers == bad
	*/
	if (value > thres_max) 
	{
		enc->offset += sprintf(str, "%e", neg ? -value : value);
		return TRUE;
	}

	if (enc->doublePrecision == 0) 
	{
		diff = value - whole;

		if (diff > 0.5) 
		{
		/* greater than 0.5, round up, e.g. 1.6 -> 2 */
		++whole;
		}
		else 
		if (diff == 0.5 && (whole & 1)) 
		{
			/* exactly 0.5 and ODD, then round up */
			/* 1.5 -> 2, but 2.5 -> 2 */
			++whole;
		}

			//vvvvvvvvvvvvvvvvvvv  Diff from modp_dto2
	} 
	else 
	if (frac) 
	{ 
		count = enc->doublePrecision;
		// now do fractional part, as an unsigned number
		// we know it is not 0 but we can have leading zeros, these
		// should be removed
		while (!(frac % 10))
		{
		--count;
		frac /= 10;
		}
		//^^^^^^^^^^^^^^^^^^^  Diff from modp_dto2

		// now do fractional part, as an unsigned number
		do 
		{
			--count;
			*wstr++ = (char)(48 + (frac % 10));
		} while (frac /= 10);
		// add extra 0s
		while (count-- > 0)
		{
			*wstr++ = '0';
		}
		// add decimal
		*wstr++ = '.';
	}

	// do whole part
	// Take care of sign
	// Conversion. Number is reversed.
	do *wstr++ = (char)(48 + (whole % 10)); while (whole /= 10);
	
	if (neg) 
	{
		*wstr++ = '-';
	}
	strreverse(str, wstr-1);
	enc->offset += (wstr - (enc->offset));

	return TRUE;
}






/*
FIXME:
Handle integration functions returning NULL here */

/*
FIXME:
Perhaps implement recursion detection */

void encode(JSOBJ obj, JSONObjectEncoder *enc, const char *name, size_t cbName)
{
	JSONTypeContext tc;
	size_t szlen;

	if (enc->level > enc->recursionMax)
	{
		SetError (obj, enc, "Maximum recursion level reached");
		return;
	}

	/*
	This reservation must hold 

	length of _name as encoded worst case +
	maxLength of double to string OR maxLength of JSLONG to string

	Since input is assumed to be UTF-8 the worst character length is:

	4 bytes (of UTF-8) => "\uXXXX\uXXXX" (12 bytes)
	*/

	Buffer_Reserve(enc, 256 + (((cbName / 4) + 1) * 12));

	if (name)
	{
		Buffer_AppendCharUnchecked(enc, '\"');
		if (!Buffer_EscapeString(obj, enc, name, name + cbName))
		{
			return;
		}
		Buffer_AppendCharUnchecked(enc, '\"');

		Buffer_AppendCharUnchecked (enc, ':');
#ifndef JSON_NO_EXTRA_WHITESPACE
		Buffer_AppendCharUnchecked (enc, ' ');
#endif
	}

	enc->beginTypeContext(obj, &tc);

	switch (tc.type)
	{
		case JT_INVALID:
			SetError(obj, enc, "Could not encode object");
			return;

		case JT_ARRAY:
		{
			int count = 0;
			JSOBJ iterObj;
			enc->iterBegin(obj, &tc);

			Buffer_AppendCharUnchecked (enc, '[');

			while (enc->iterNext(obj, &tc))
			{
				if (count > 0)
				{
					Buffer_AppendCharUnchecked (enc, ',');
#ifndef JSON_NO_EXTRA_WHITESPACE
					Buffer_AppendCharUnchecked (buffer, ' ');
#endif
				}

				iterObj = enc->iterGetValue(obj, &tc);

				enc->level ++;
				encode (iterObj, enc, NULL, 0);			
				count ++;
			}

			enc->iterEnd(obj, &tc);
			Buffer_AppendCharUnchecked (enc, ']');
			break;
		}

		case JT_OBJECT:
		{
			int count = 0;
			JSOBJ iterObj;
			char *objName;

			enc->iterBegin(obj, &tc);

			Buffer_AppendCharUnchecked (enc, '{');

			while (enc->iterNext(obj, &tc))
			{
				if (count > 0)
				{
					Buffer_AppendCharUnchecked (enc, ',');
#ifndef JSON_NO_EXTRA_WHITESPACE
					Buffer_AppendCharUnchecked (enc, ' ');
#endif
				}

				iterObj = enc->iterGetValue(obj, &tc);
				objName = enc->iterGetName(obj, &tc, &szlen);

				enc->level ++;
				encode (iterObj, enc, objName, szlen);			
				count ++;
			}

			enc->iterEnd(obj, &tc);
			Buffer_AppendCharUnchecked (enc, '}');
			break;
		}

		case JT_LONG:
		{
			Buffer_AppendLongUnchecked (enc, enc->getLongValue(obj, &tc));
			break;
		}

		case JT_INT:
		{
			Buffer_AppendIntUnchecked (enc, enc->getIntValue(obj, &tc));
			break;
		}

		case JT_TRUE:
		{
			Buffer_AppendCharUnchecked (enc, 't');
			Buffer_AppendCharUnchecked (enc, 'r');
			Buffer_AppendCharUnchecked (enc, 'u');
			Buffer_AppendCharUnchecked (enc, 'e');
			break;
		}

		case JT_FALSE:
		{
			Buffer_AppendCharUnchecked (enc, 'f');
			Buffer_AppendCharUnchecked (enc, 'a');
			Buffer_AppendCharUnchecked (enc, 'l');
			Buffer_AppendCharUnchecked (enc, 's');
			Buffer_AppendCharUnchecked (enc, 'e');
			break;
		}


		case JT_NULL: 
		{
			Buffer_AppendCharUnchecked (enc, 'n');
			Buffer_AppendCharUnchecked (enc, 'u');
			Buffer_AppendCharUnchecked (enc, 'l');
			Buffer_AppendCharUnchecked (enc, 'l');
			break;
		}

		case JT_DOUBLE:
		{
			if (!Buffer_AppendDoubleUnchecked (obj, enc, enc->getDoubleValue(obj, &tc)))
			{
				enc->endTypeContext(obj, &tc);
				enc->level --;
				return;
			}
			break;
		}

		case JT_UTF8:
		{
			const char *value = enc->getStringValue(obj, &tc, &szlen);
			Buffer_Reserve(enc, ((szlen / 4) + 1) * 12);
			Buffer_AppendCharUnchecked (enc, '\"');

			if (!Buffer_EscapeString(obj, enc, value, value + szlen))
			{
				enc->endTypeContext(obj, &tc);
				enc->level --;
				return;
			}

			Buffer_AppendCharUnchecked (enc, '\"');
			break;
		}
	}

	enc->endTypeContext(obj, &tc);
	enc->level --;

}

char *JSON_EncodeObject(JSOBJ obj, JSONObjectEncoder *enc, char *_buffer, size_t _cbBuffer)
{
	enc->malloc = enc->malloc ? enc->malloc : malloc;
	enc->free =  enc->free ? enc->free : free;
	enc->realloc = enc->realloc ? enc->realloc : realloc;
	enc->errorMsg = NULL;
	enc->errorObj = NULL;
	enc->level = 0;

	if (enc->recursionMax < 1)
	{
		enc->recursionMax = JSON_MAX_RECURSION_DEPTH;
	}

	if (enc->doublePrecision < 0 ||
			enc->doublePrecision > JSON_DOUBLE_MAX_DECIMALS)
	{
		enc->doublePrecision = JSON_DOUBLE_MAX_DECIMALS;
	}

	if (_buffer == NULL)
	{
		_cbBuffer = 32768;
		enc->start = (char *) enc->malloc (_cbBuffer);
		enc->heap = 1;
	}
	else
	{
		enc->start = _buffer;
		enc->heap = 0;
	}

	enc->end = enc->start + _cbBuffer;
	enc->offset = enc->start;


	encode (obj, enc, NULL, 0);
	
	Buffer_Reserve(enc, 1);
	Buffer_AppendCharUnchecked(enc, '\0');

	return enc->start;
}