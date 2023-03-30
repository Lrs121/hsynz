//dict_decompress_plugin_demo.h
//  dict decompress plugin demo for hsync_demo
/*
 The MIT License (MIT)
 Copyright (c) 2022-2023 HouSisong
 
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
        hpatch_BOOL           dict_isInReset;
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
        self->dict_isInReset=hpatch_FALSE;
        _CacheBlockDict_init(&self->cache,((hpatch_byte*)self)+sizeof(*self),
                             cacheDictSize,dictSize,blockCount,blockSize);
        if (inflateInit2(&self->stream,-plugin->dict_bits)!=Z_OK){
            free(self);
            return 0;// error
        }
        return self;
    }
    static void _zlib_dictDecompressClose(struct hsync_TDictDecompress* decompressPlugin,
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
        if (self->dict_isInReset||_CacheBlockDict_isMustResetDict(&self->cache,blockIndex)){ //reset dict
            hpatch_byte* dict;
            size_t       dictSize;
            _CacheBlockDict_usedDict(&self->cache,blockIndex,&dict,&dictSize);
            assert(dictSize==(uInt)dictSize);
            if (inflateReset(&self->stream)!=Z_OK)
                return hpatch_FALSE;
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
        if (z_ret==Z_STREAM_END)
            self->dict_isInReset=hpatch_TRUE;
        else if (z_ret!=Z_OK)
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
        size_t                dictSize;
    } TDictDecompressPlugin_zstd;
    typedef struct{
        ZSTD_DCtx*            s;
        ZSTD_DDict*           d;
        _CacheBlockDict_t     cache;
    } _TDictDecompressPlugin_zstd_data;

    static hpatch_BOOL _zstd_dict_is_can_open(const char* compressType){
        return (0==strcmp(compressType,"zstdD"));
    }
    static hpatch_inline void __zstd_clear_dd(_TDictDecompressPlugin_zstd_data* self){
        ZSTD_DDict* d=self->d;
        self->d=0;
        if (d!=0){
            size_t ret=ZSTD_freeDDict(d);
            assert(ret==0);
        }
    }
    static void _zstd_dictDecompressClose(struct hsync_TDictDecompress* decompressPlugin,
                                          hsync_dictDecompressHandle dictHandle){
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)dictHandle;
        if (self!=0){
            ZSTD_DCtx*  s=self->s;
            __zstd_clear_dd(self);
            self->s=0;
            if (s!=0){
                size_t ret=ZSTD_freeDCtx(s);
                assert(ret==0);
            }
            free(self);
        }
    }
    
    static void _zstd_dictUncompress(hpatch_decompressHandle dictHandle,size_t blockIndex,size_t lastCompressedBlockIndex,
                                     const hpatch_byte* dataBegin,const hpatch_byte* dataEnd){
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)dictHandle;
        _CacheBlockDict_dictUncompress(&self->cache,blockIndex,lastCompressedBlockIndex,dataBegin,dataEnd);
    }

    #define _zstd_checkOpen(v)  do { if (ZSTD_isError(v)) goto _on_error; } while(0)

    static hsync_dictDecompressHandle _zstd_dictDecompressOpen(struct hsync_TDictDecompress* decompressPlugin,size_t blockCount,size_t blockSize,
                                                               const hpatch_byte* in_info,const hpatch_byte* in_infoEnd){
        const TDictDecompressPlugin_zstd*  plugin=(const TDictDecompressPlugin_zstd*)decompressPlugin;
        const size_t dictSize=plugin->dictSize;
        const size_t cacheDictSize=_getCacheBlockDictSize(dictSize,blockCount,blockSize);
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)malloc(sizeof(_TDictDecompressPlugin_zstd_data)+cacheDictSize);
        if (self==0) return 0;
        memset(self,0,sizeof(*self));
        assert(blockCount>0);
        assert(dictSize>0);
        _CacheBlockDict_init(&self->cache,((hpatch_byte*)self)+sizeof(*self),
                             cacheDictSize,dictSize,blockCount,blockSize);

        self->s=ZSTD_createDCtx();
        if (self->s==0) goto _on_error;
        _zstd_checkOpen(ZSTD_DCtx_setParameter(self->s,ZSTD_d_format,ZSTD_f_zstd1_magicless));
        _zstd_checkOpen(ZSTD_DCtx_setParameter(self->s,ZSTD_d_forceIgnoreChecksum,ZSTD_d_ignoreChecksum));
        #define _ZSTD_WINDOWLOG_MAX 30
        ZSTD_DCtx_setParameter(self->s,ZSTD_d_windowLogMax,_ZSTD_WINDOWLOG_MAX);
        return self;
    _on_error:
        _zstd_dictDecompressClose(decompressPlugin,self);
        return 0; //error
    }

    static void __zstd_resetDDict(_TDictDecompressPlugin_zstd_data* self){
        const size_t dictSize=self->cache.uncompressCur-self->cache.uncompress;
        if (self->d==0){
            self->d=ZSTD_createDDict_advanced(self->cache.uncompress,dictSize,ZSTD_dlm_byRef,ZSTD_dct_rawContent,ZSTD_defaultCMem); 
        }else{
            const ZSTD_DDict* refd=ZSTD_initStaticDDict(self->d,ZSTD_sizeof_DDict(self->d),self->cache.uncompress,dictSize,ZSTD_dlm_byRef,ZSTD_dct_rawContent);
            if (refd!=0){
                assert(self->d==refd);
                self->d=(ZSTD_DDict*)refd;
            }else{
                __zstd_clear_dd(self);
            }
        }
    }


    #define _zstd_checkDec(v)  do { if (ZSTD_isError(v)) return hpatch_FALSE; } while(0)

    static hpatch_BOOL _zstd_dictDecompress(hpatch_decompressHandle dictHandle,size_t blockIndex,
                                            const hpatch_byte* in_code,const hpatch_byte* in_codeEnd,
                                            hpatch_byte* out_dataBegin,hpatch_byte* out_dataEnd){
        _TDictDecompressPlugin_zstd_data* self=(_TDictDecompressPlugin_zstd_data*)dictHandle;
        ZSTD_DCtx* s=self->s;
        ZSTD_inBuffer       s_input;
        ZSTD_outBuffer      s_output;
        const size_t dataSize=(size_t)(out_dataEnd-out_dataBegin);
        if (_CacheBlockDict_isHaveDict(&self->cache)){ //reset dict
            __zstd_resetDDict(self);
            if (self->d==0) return hpatch_FALSE;
            _zstd_checkDec(ZSTD_DCtx_reset(s,ZSTD_reset_session_only));
            _zstd_checkDec(ZSTD_DCtx_refDDict(s,self->d));
        }
        s_input.src=in_code;
        s_input.size=in_codeEnd-in_code;
        s_input.pos=0;
        s_output.dst=out_dataBegin;
        s_output.size=dataSize;
        s_output.pos=0;
        _zstd_checkDec(ZSTD_decompressStream(s,&s_output,&s_input));
        assert(s_input.pos==s_input.size);
        assert(s_output.pos==dataSize);
        return (s_output.pos==dataSize);
    }
    
    static const TDictDecompressPlugin_zstd zstdDictDecompressPlugin={
        { _zstd_dict_is_can_open, _zstd_dictDecompressOpen,
          _zstd_dictDecompressClose, _zstd_dictDecompress, _zstd_dictUncompress },
        0 };
#endif//_CompressPlugin_zstd


#ifdef __cplusplus
}
#endif
#endif
