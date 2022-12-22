//dict_decompress_plugin_demo.h
//  dict decompress plugin demo for hsync_demo
/*
 The MIT License (MIT)
 Copyright (c) 2022 HouSisong
 
 Permission is hereby granted, free of charge, to any person
 obtaining a copy of this software and associated documentation
 files (the "Software"), to deal in the Software without
 restriction, including without limitation the rights to use,
 copy, modify, merge, publish, distribute, sublicense, and/or sell
 copies of the Software, and to permit persons to whom the
 Software is furnished to do so, subject to the following
 conditions:
 
 The above copyright notice and this permission notice shall be
 included in all copies of the Software.
 
 THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef dict_decompress_plugin_demo_h
#define dict_decompress_plugin_demo_h
//dict decompress plugin demo:
//  zlibDictDecompressPlugin

#include "HDiffPatch/libhsync/sync_client/dict_decompress_plugin.h"
#ifndef HPatch_decompress_plugin_demo_h
#ifndef _IsNeedIncludeDefaultCompressHead
#   define _IsNeedIncludeDefaultCompressHead 1
#endif
#endif
#ifdef __cplusplus
extern "C" {
#endif


#ifdef  _CompressPlugin_zlib
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zlib.h" // http://zlib.net/  https://github.com/madler/zlib
#endif
    //zlibDictDecompressPlugin
    typedef struct{
        hsync_TDictDecompress base;
        hpatch_byte           dict_bits;
    } TDictDecompressPlugin_zlib;
    typedef struct{
        z_stream              stream;
        _CacheBlockDict_t     cache;
    } _TDictDecompressPlugin_zlib_data;
    
    static hpatch_BOOL _zlib_dict_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"zlibD"))||(0==strcmp(compressType,"gzipD"));
    }
    static hsync_dictDecompressHandle _zlib_dictDecompressOpen(struct hsync_TDictDecompress* decompressPlugin,size_t blockCount,size_t blockSize,
                                                               const hpatch_byte* in_info,const hpatch_byte* in_infoEnd){
        const TDictDecompressPlugin_zlib*  plugin=(const TDictDecompressPlugin_zlib*)decompressPlugin;
        const size_t dictSize=((size_t)1<<plugin->dict_bits);
        const size_t cacheDictSize=_getCacheBlockDictSize(dictSize,blockCount,blockSize);
        _TDictDecompressPlugin_zlib_data* self=(_TDictDecompressPlugin_zlib_data*)malloc(sizeof(_TDictDecompressPlugin_zlib_data)+cacheDictSize);
        if (self==0) return 0;
        memset(self,0,sizeof(*self));
        assert(in_info==in_infoEnd);
        assert(blockCount>0);
        _CacheBlockDict_init(&self->cache,((hpatch_byte*)self)+sizeof(*self),
                             cacheDictSize,dictSize,blockCount,blockSize);
        if (inflateInit2(&self->stream,-plugin->dict_bits)!=Z_OK){
            free(self);
            return 0;// error
        }
        return self;
    }
    static void _zlib_dictDecompressClose(struct hsync_TDictDecompress* dictDecompressPlugin,
                                          hsync_dictDecompressHandle dictHandle){
        _TDictDecompressPlugin_zlib_data* self=(_TDictDecompressPlugin_zlib_data*)dictHandle;
        if (self!=0){
            if (self->stream.state!=0){
                int ret=inflateEnd(&self->stream);
                self->stream.state=0;
                assert(Z_OK==ret);
            }
            free(self);
        }
    }
    
    static void _zlib_dictUncompress(hpatch_decompressHandle dictHandle,size_t blockIndex,size_t lastCompressedBlockIndex,
                                            const hpatch_byte* dataBegin,const hpatch_byte* dataEnd){
        _TDictDecompressPlugin_zlib_data* self=(_TDictDecompressPlugin_zlib_data*)dictHandle;
        _CacheBlockDict_dictUncompress(&self->cache,blockIndex,lastCompressedBlockIndex,dataBegin,dataEnd);
    }

    static hpatch_BOOL _zlib_dictDecompress(hpatch_decompressHandle dictHandle,size_t blockIndex,
                                            const hpatch_byte* in_code,const hpatch_byte* in_codeEnd,
                                            hpatch_byte* out_dataBegin,hpatch_byte* out_dataEnd){
        _TDictDecompressPlugin_zlib_data* self=(_TDictDecompressPlugin_zlib_data*)dictHandle;
        int z_ret;
        const size_t dataSize=out_dataEnd-out_dataBegin;
        if (_CacheBlockDict_isNeedResetDict(&self->cache,blockIndex)){ //reset dict
            hpatch_byte* dict;
            size_t       dictSize;
            if (inflateReset(&self->stream)!=Z_OK)
                return hpatch_FALSE;
            _CacheBlockDict_usedDict(&self->cache,blockIndex,&dict,&dictSize);
            assert(dictSize==(uInt)dictSize);
            if (inflateSetDictionary(&self->stream,dict,(uInt)dictSize)!=Z_OK)
                return hpatch_FALSE; //error
        }
        self->stream.next_in =(Bytef*)in_code;
        self->stream.avail_in=(uInt)(in_codeEnd-in_code);
        assert(self->stream.avail_in==(size_t)(in_codeEnd-in_code));
        self->stream.next_out =out_dataBegin;
        self->stream.avail_out=(uInt)dataSize;
        assert(self->stream.avail_out==dataSize);
        z_ret=inflate(&self->stream,Z_PARTIAL_FLUSH);
        if ((z_ret!=Z_STREAM_END)&&(z_ret!=Z_OK))
            return hpatch_FALSE;//error
        assert(self->stream.avail_in==0);
        if (self->stream.total_out!=dataSize)
            return hpatch_FALSE;//error
        self->stream.total_out=0;
        return hpatch_TRUE;
    }
    
    static const TDictDecompressPlugin_zlib zlibDictDecompressPlugin={
        { _zlib_dict_is_can_open, _zlib_dictDecompressOpen,
          _zlib_dictDecompressClose, _zlib_dictDecompress,_zlib_dictUncompress },
        MAX_WBITS };
    
#endif//_CompressPlugin_zlib


#ifdef  _CompressPlugin_zstd
#define ZSTD_STATIC_LINKING_ONLY
#if (_IsNeedIncludeDefaultCompressHead)
#   include "zstd.h" // "zstd/lib/zstd.h" https://github.com/facebook/zstd
#endif
    //zstdDictDecompressPlugin
    typedef struct{
        hsync_TDictDecompress base;
        hpatch_byte           dict_bits;
    } TDictDecompressPlugin_zstd;
    typedef struct{
        ZSTD_DCtx*          s;
    } _TDictDecompressPlugin_zstd_data;

    static hpatch_BOOL _zstd_dict_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"zstdD"));
    }
    static void _zstd_dictDecompressClose(struct hsync_TDictDecompress* dictDecompressPlugin,
                                          hsync_dictDecompressHandle dictHandle){
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)dictHandle;
        if (self!=0){
            if (self->s!=0){
                size_t ret=ZSTD_freeDCtx(self->s);
                assert(ret==0);
            }
            free(self);
        }
    }
    static hsync_dictDecompressHandle _zstd_dictDecompressOpen(struct hsync_TDictDecompress* dictDecompressPlugin){
        const TDictDecompressPlugin_zstd*  plugin=(const TDictDecompressPlugin_zstd*)dictDecompressPlugin;
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)malloc(sizeof(_TDictDecompressPlugin_zstd_data));
        size_t ret;
        if (self==0) return 0;
        memset(self,0,sizeof(*self));
        self->s=ZSTD_createDCtx();
        if (self->s==0) goto _on_error;
        ret=ZSTD_DCtx_setParameter(self->s,ZSTD_d_format,ZSTD_f_zstd1_magicless);
        if (ZSTD_isError(ret)) goto _on_error;
        ret=ZSTD_DCtx_setParameter(self->s,ZSTD_d_forceIgnoreChecksum,ZSTD_d_ignoreChecksum);
        if (ZSTD_isError(ret)) goto _on_error;
        return self;
    _on_error:
        _zstd_dictDecompressClose(dictDecompressPlugin,self);
        return 0; //error
    }

    #define _zstd_checkDec(v)  do { if (ZSTD_isError(v)) return hpatch_FALSE; } while(0)

    static hpatch_BOOL _zstd_dictDecompress(hsync_dictDecompressHandle dictHandle,
                                            const unsigned char* in_code,const unsigned char* in_codeEnd,
                                            const unsigned char* in_dict,unsigned char* in_dictEnd_and_out_dataBegin,
                                            unsigned char* out_dataEnd,hpatch_BOOL dict_isReset,hpatch_BOOL out_isEnd){
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)dictHandle;
        ZSTD_DCtx* s=self->s;
        ZSTD_inBuffer       s_input;
        ZSTD_outBuffer      s_output;
        size_t dictSize=in_dictEnd_and_out_dataBegin-in_dict;
        const size_t dataSize=(size_t)(out_dataEnd-in_dictEnd_and_out_dataBegin);
        if (dictSize>0){ //reset dict
            _zstd_checkDec(ZSTD_DCtx_reset(s,ZSTD_reset_session_only));
            _zstd_checkDec(ZSTD_DCtx_refPrefix(s,in_dict,dictSize));
        }
        s_input.src=in_code;
        s_input.size=in_codeEnd-in_code;
        s_input.pos=0;
        s_output.dst=in_dictEnd_and_out_dataBegin;
        s_output.size=dataSize;
        s_output.pos=0;
        _zstd_checkDec(ZSTD_decompressStream(s,&s_output,&s_input));
        assert(s_input.pos==s_input.size);
        assert(s_output.pos==dataSize);
        return (s_output.pos==dataSize);
    }
    
    static const TDictDecompressPlugin_zstd zstdDictDecompressPlugin={
        { _zstd_dict_is_can_open, _zstd_dictDecompressOpen,
          _zstd_dictDecompressClose, 0, _zstd_dictDecompress },
        20 };
#endif//_CompressPlugin_zstd


#ifdef __cplusplus
}
#endif
#endif
