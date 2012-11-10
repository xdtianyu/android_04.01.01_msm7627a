/** Log wrapper for Android.
 * @{
 * @file
 *
 * Maps LOG_*() macros to __android_log_print() if LOG_ANDROID is defined.
 * Adds some extra info to log output like LOG_TAG, file name and line number.
 *
 * <!-- Copyright Giesecke & Devrient GmbH 2010 - 2011 -->
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#ifndef TLCWRAPPERANDROIDLOG_H_
#define TLCWRAPPERANDROIDLOG_H_

#include <unistd.h>
#include <stdio.h>
#include <android/log.h>


#define EOL "\n"
#define DUMMY_FUNCTION()    do{}while(0)


#ifdef LOG_ANDROID

#ifdef NDEBUG
    #define LOG_I(fmt, args...) DUMMY_FUNCTION()
    #define LOG_W(fmt, args...) DUMMY_FUNCTION()
#else
    #define LOG_I(fmt, args...) LOG_i("%d : "fmt , __LINE__ , ## args)
    #define LOG_W(fmt, args...) LOG_w("%d : "fmt , __LINE__ , ## args)
#endif
    #define _LOG_E(fmt, args...) LOG_e("%d : "fmt , __LINE__ , ## args)

    #define LOG_i(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
    #define LOG_w(...) __android_log_print(ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
    #define LOG_e(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#else //!defined(LOG_ANDROID)

    // #level / #LOG_TAG ( process_id): __VA_ARGS__
    // Example:
    // I/McDrvBasicTest_0_1( 4075): setUp
    #define _LOG_x(_x_,...) \
                do \
                { \
                    printf("%s/%s(%d): ",_x_,LOG_TAG,getpid()); \
                    printf(__VA_ARGS__); \
                    printf(EOL); \
                } while(1!=1)


#ifdef NDEBUG
    #define LOG_I(fmt, args...) DUMMY_FUNCTION()
    #define LOG_W(fmt, args...) DUMMY_FUNCTION()
#else
    #define LOG_I(...)  _LOG_x("I",__VA_ARGS__)
    #define LOG_W(...)  _LOG_x("W",__VA_ARGS__)
#endif
    #define _LOG_E(...)  _LOG_x("E",__VA_ARGS__)

#endif //defined(LOG_ANDROID)


/** LOG_E() needs to be more prominent:
 * Display "*********** ERROR ***********" before actual error message.
 */
#define LOG_E(...) \
            do \
            { \
                _LOG_E("*****************************"); \
                _LOG_E("*********   ERROR   *********"); \
                _LOG_E(__VA_ARGS__); \
            } while(1!=1)


#define LOG_I_BUF   LOG_I_Buf

__attribute__ ((unused))
static void LOG_I_Buf(
	const char *  szDescriptor,
	const void *  blob,
	size_t        sizeOfBlob
) {

	#define CPL         0x10  // chars per line
	#define OVERHEAD    20

	char buffer[CPL * 4 + OVERHEAD];

	uint32_t index = 0;

	uint32_t moreThanOneLine = (sizeOfBlob > CPL);
	uint32_t blockLen = CPL;
	uint32_t addr = 0;
	uint32_t i = 0;

	if (NULL != szDescriptor)
	{
		index += sprintf(&buffer[index], "%s", szDescriptor);
	}

	if (moreThanOneLine)
	{
		if (NULL == szDescriptor)
		{
			index += sprintf(&buffer[index], "memory dump");
		}
		index += sprintf(&buffer[index], " (0x%08x, %d bytes)", (uint32_t)blob,sizeOfBlob);
		LOG_I("%s", buffer);
		index = 0;
	}
	else if (NULL == szDescriptor)
	{
		index += sprintf(&buffer[index], "Data at 0x%08x: ", (uint32_t)blob);
	}

	if(sizeOfBlob == 0) {
		LOG_I("%s", buffer);
	}
	else
	{
		while (sizeOfBlob > 0)
		{
			if (sizeOfBlob < blockLen)
			{
				blockLen = sizeOfBlob;
			}

			// address
			if (moreThanOneLine)
			{
				index += sprintf(&buffer[index], "0x%08X | ",addr);
				addr += CPL;
			}
			// bytes as hex
			for (i=0; i<blockLen; ++i)
			{
				index += sprintf(&buffer[index], "%02x ", ((const char *)blob)[i] );
			}
			// spaces if necessary
			if ((blockLen < CPL) && (moreThanOneLine))
			{
				// add spaces
				for (i=0; i<(3*(CPL-blockLen)); ++i) {
				index += sprintf(&buffer[index], " ");
				}
			}
			// bytes as ASCII
			index += sprintf(&buffer[index], "| ");
			for (i=0; i<blockLen; ++i)
			{
				char c = ((const char *)blob)[i];
				index += sprintf(&buffer[index], "%c",(c>32)?c:'.');
			}

			blob = &(((const char *)blob)[blockLen]);
			sizeOfBlob -= blockLen;

			// print line to logcat / stdout
			LOG_I("%s", buffer);
			index = 0;
		}
	}
}

#endif /** TLCWRAPPERANDROIDLOG_H_ */

/** @} */
