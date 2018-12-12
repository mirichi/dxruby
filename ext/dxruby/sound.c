#define WINVER 0x0500                                  /* バージョン定義 Windows2000以上 */
#define DIRECTSOUND_VERSION 0x0900
#define _WIN32_WINNT WINVER

#include "ruby.h"
#ifndef RUBY_ST_H
#include "st.h"
#endif

#include <dmusici.h>

#define DXRUBY_EXTERN 1
#include "dxruby.h"
#include "sound.h"

#ifndef DS3DALG_DEFAULT
GUID DS3DALG_DEFAULT = {0};
#endif

#define WAVE_RECT 0
#define WAVE_SIN 1
#define WAVE_SAW 2
#define WAVE_TRI 3

static VALUE cSound;        /* サウンドクラス       */
static VALUE cSoundEffect;  /* 生成効果音クラス     */

static IDirectMusicPerformance8 *g_pDMPerformance = NULL;       /* DirectMusicPerformance8インターフェイス */
static IDirectMusicLoader8      *g_pDMLoader = NULL;            /* ローダー */
static LPDIRECTSOUND8           g_pDSound = NULL;               /* DirectSoundインターフェイス */
static int g_iRefDM = 0; /* DirectMusicパフォーマンスの参照カウント */
static int g_iRefDS = 0; /* DirectSoundの参照カウント */

/* Soundオブジェクト */
struct DXRubySound {
    IDirectMusicAudioPath8   *pDMDefAudioPath; /* デフォルトオーディオパス */
    IDirectMusicSegment8     *pDMSegment;        /* セグメント       */
    int start;
    int loopstart;
    int loopend;
    int loopcount;
    int midwavflag; /* midは0、wavは1 */
    VALUE vbuffer;
};

/* SoundEffectオブジェクト */
struct DXRubySoundEffect {
    LPDIRECTSOUNDBUFFER pDSBuffer;    /* バッファ         */
};



/*********************************************************************
 * Soundクラス
 *
 * DirectMusicを使用して音を鳴らす。
 * とりあえず音を出そうと頑張っている。
 *********************************************************************/

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
static void Sound_free( struct DXRubySound *sound )
{
    HRESULT hr;

    /* サウンドオブジェクトの開放 */
    /* バンド解放 */
    if ( sound->pDMSegment )
    {
        hr = sound->pDMSegment->lpVtbl->Unload( sound->pDMSegment, (IUnknown* )sound->pDMDefAudioPath );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Band release failed - Unload" );
        }
        /* セグメントを開放 */
        RELEASE( sound->pDMSegment );
    }

    /* デフォルトオーディオパスを開放 */
    RELEASE( sound->pDMDefAudioPath );

    g_iRefDM--;

    if( g_iRefDM <= 0 )
    {
        /* 演奏停止 */
        if ( g_pDMPerformance )
        {
            hr = g_pDMPerformance->lpVtbl->Stop( g_pDMPerformance, NULL, NULL, 0, 0 );
            if ( FAILED( hr ) )
            {
                rb_raise( eDXRubyError, "Stop performance failed - Stop" );
            }
            g_pDMPerformance->lpVtbl->CloseDown( g_pDMPerformance );
        }
        RELEASE(g_pDMPerformance);

        /* ローダを開放 */
        RELEASE(g_pDMLoader);
    }
}

static void Sound_mark( struct DXRubySound *sound )
{
    rb_gc_mark( sound->vbuffer );
}

static void Sound_release( struct DXRubySound *sound )
{
    if ( sound->pDMSegment )
    {
        Sound_free( sound );
    }
    free( sound );
    sound = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t Sound_data_type = {
    "Sound",
    {
    Sound_mark,
    Sound_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   Soundクラスのdispose。
 ---------------------------------------------------------------------*/
static VALUE Sound_dispose( VALUE self )
{
    struct DXRubySound *sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    Sound_free( sound );
    return self;
}

/*--------------------------------------------------------------------
   Soundクラスのdisposed?。
 ---------------------------------------------------------------------*/
static VALUE Sound_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( Sound, self )->pDMSegment == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
   Soundクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE Sound_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubySound *sound;

    /* DXRubyImageのメモリ取得＆Imageオブジェクト生成 */
    sound = malloc(sizeof(struct DXRubySound));
    if( sound == NULL ) rb_raise( eDXRubyError, "Out of memory - Sound_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &Sound_data_type, sound );
#else
    obj = Data_Wrap_Struct(klass, 0, Sound_release, sound);
#endif
    /* とりあえずサウンドオブジェクトはNULLにしておく */
    sound->pDMSegment = NULL;
    sound->vbuffer = Qnil;

    return obj;
}


/*--------------------------------------------------------------------
   Soundクラスのload_from_memory。ファイルをメモリからロードする。
 ---------------------------------------------------------------------*/
static VALUE Sound_load_from_memory( VALUE klass, VALUE vstr, VALUE vtype )
{
    HRESULT hr;
    WCHAR wstrFileName[MAX_PATH];
    VALUE obj;
    struct DXRubySound *sound;
    CHAR strPath[MAX_PATH];
    DWORD i;
    WCHAR wstrSearchPath[MAX_PATH];
    VALUE vsjisstr;

    g_iRefAll++;

    Check_Type( vstr, T_STRING );

    if( g_iRefDM == 0 )
    {
        /* パフォーマンスの作成 */
        hr = CoCreateInstance( &CLSID_DirectMusicPerformance, NULL,
                               CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8,
                               (void**)&g_pDMPerformance );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
        }

        /* パフォーマンスの初期化 */
        hr = g_pDMPerformance->lpVtbl->InitAudio( g_pDMPerformance,
                                                  NULL,                  /* IDirectMusicインターフェイスは不要 */
                                                  NULL,                  /* IDirectSoundインターフェイスは不要 */
                                                  g_hWnd,               /* ウィンドウのハンドル */
                                                  DMUS_APATH_SHARED_STEREOPLUSREVERB,  /* デフォルトのオーディオパス・タイプ */
                                                  64,                    /* パフォーマンス・チャンネルの数 */
                                                  DMUS_AUDIOF_ALL,       /* シンセサイザの機能 */
                                                  NULL );                /* オーディオ・パラメータにはデフォルトを使用 */
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectMusic initialize error - InitAudio" );
        }

        /* ローダーの作成 */
        hr = CoCreateInstance( &CLSID_DirectMusicLoader, NULL, 
                               CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8,
                               (void**)&g_pDMLoader );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
        }

        /* ローダーの初期化（検索パスをカレント・ディレクトリに設定） */
        i = GetCurrentDirectory( MAX_PATH, strPath );
        if ( i == 0 || MAX_PATH < i )
        {
            rb_raise( eDXRubyError, "Get current directory failed - GetCurrentDirectory" );
        }

        /* マルチ・バイト文字をUNICODEに変換 */
        MultiByteToWideChar( CP_ACP, 0, strPath, -1, wstrSearchPath, MAX_PATH );

        /* ローダーに検索パスを設定 */
        hr = g_pDMLoader->lpVtbl->SetSearchDirectory( g_pDMLoader, &GUID_DirectMusicAllTypes,
                                                      wstrSearchPath, FALSE );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set directory failed - SetSearchDirectory" );
        }
    }
    g_iRefDM++;

    /* サウンドオブジェクト取得 */
    obj = Sound_allocate( klass );
    sound = DXRUBY_GET_STRUCT( Sound, obj );
    if( sound->pDMSegment )
    {
        g_iRefDM++;
        Sound_free( sound );
        g_iRefDM--;
        g_iRefAll--;
    }

    /* オーディオ・パス作成 */
    hr = g_pDMPerformance->lpVtbl->CreateStandardAudioPath( g_pDMPerformance,
        DMUS_APATH_DYNAMIC_STEREO,      /* パスの種類。 */
        64,                             /* パフォーマンス チャンネルの数。 */
        TRUE,                           /* ここでアクティブになる。 */
        &sound->pDMDefAudioPath );      /* オーディオパスを受け取るポインタ。 */

    if ( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "AudioPath set error - CreateStandardAudioPath" );
    }

    sound->vbuffer = vstr;

    {
        DMUS_OBJECTDESC desc;

        ZeroMemory( &desc, sizeof(DMUS_OBJECTDESC) );
        desc.dwSize      = sizeof(DMUS_OBJECTDESC);
        desc.dwValidData = DMUS_OBJ_MEMORY | DMUS_OBJ_CLASS;
        desc.guidClass   = CLSID_DirectMusicSegment;
        desc.llMemLength = (LONGLONG)RSTRING_LEN(vstr);      // バッファのサイズ
        desc.pbMemData   = (LPBYTE)RSTRING_PTR(vstr);        // データの入っているバッファ

        hr = g_pDMLoader->lpVtbl->GetObject( g_pDMLoader, &desc, &IID_IDirectMusicSegment8, (void**)&sound->pDMSegment );
    }

    if( FAILED( hr ) )
    {
        sound->pDMSegment = NULL;
        rb_raise( eDXRubyError, "Load error - LoadObjectFromFile" );
    }

    sound->start = 0;
    sound->loopstart = 0;
    sound->loopend = 0;

    /* MIDIの場合 */
    if( NUM2INT( vtype ) == 0 )
    {
        hr = sound->pDMSegment->lpVtbl->SetParam( sound->pDMSegment, &GUID_StandardMIDIFile,
                                                  0xFFFFFFFF, 0, 0, NULL);
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Load error - SetParam" );
        }
        sound->loopcount = DMUS_SEG_REPEAT_INFINITE;
        sound->midwavflag = 0;
        /* ループ回数設定 */
        hr = sound->pDMSegment->lpVtbl->SetRepeats( sound->pDMSegment, sound->loopcount );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop count failed - SetRepeats" );
        }
    }
    else
    {
        sound->loopcount = 1;
        sound->midwavflag = 1;
    }

    /* バンドダウンロード */
    hr = sound->pDMSegment->lpVtbl->Download( sound->pDMSegment, (IUnknown* )sound->pDMDefAudioPath );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Band loading failed - Download" );
    }


    /* 音量設定 */
    hr = sound->pDMDefAudioPath->lpVtbl->SetVolume( sound->pDMDefAudioPath, 230 * 9600 / 255 - 9600 , 0 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set volume failed - SetVolume" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   Soundクラスのinitialize。ファイルをロードする。
 ---------------------------------------------------------------------*/
static VALUE Sound_initialize( VALUE obj, VALUE vfilename )
{
    HRESULT hr;
    WCHAR wstrFileName[MAX_PATH];
    struct DXRubySound *sound;
    CHAR strPath[MAX_PATH];
    DWORD i;
    WCHAR wstrSearchPath[MAX_PATH];
    VALUE vsjisstr;

    g_iRefAll++;

	Check_Type(vfilename, T_STRING);

    if( g_iRefDM == 0 )
	{
	    /* パフォーマンスの作成 */
	    hr = CoCreateInstance( &CLSID_DirectMusicPerformance, NULL,
	                           CLSCTX_INPROC_SERVER, &IID_IDirectMusicPerformance8,
	                           (void**)&g_pDMPerformance );
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
	    }

	    /* パフォーマンスの初期化 */
	    hr = g_pDMPerformance->lpVtbl->InitAudio( g_pDMPerformance,
	                                              NULL,                  /* IDirectMusicインターフェイスは不要 */
	                                              NULL,                  /* IDirectSoundインターフェイスは不要 */
	                                              g_hWnd,               /* ウィンドウのハンドル */
	                                              DMUS_APATH_SHARED_STEREOPLUSREVERB,  /* デフォルトのオーディオパス・タイプ */
	                                              64,                    /* パフォーマンス・チャンネルの数 */
	                                              DMUS_AUDIOF_ALL,       /* シンセサイザの機能 */
	                                              NULL );                /* オーディオ・パラメータにはデフォルトを使用 */
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "DirectMusic initialize error - InitAudio" );
	    }

	    /* ローダーの作成 */
	    hr = CoCreateInstance( &CLSID_DirectMusicLoader, NULL, 
	                           CLSCTX_INPROC_SERVER, &IID_IDirectMusicLoader8,
	                           (void**)&g_pDMLoader );
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "DirectMusic initialize error - CoCreateInstance" );
	    }

	    /* ローダーの初期化（検索パスをカレント・ディレクトリに設定） */
	    i = GetCurrentDirectory( MAX_PATH, strPath );
	    if ( i == 0 || MAX_PATH < i )
	    {
	        rb_raise( eDXRubyError, "Get current directory failed - GetCurrentDirectory" );
	    }

	    /* マルチ・バイト文字をUNICODEに変換 */
	    MultiByteToWideChar( CP_ACP, 0, strPath, -1, wstrSearchPath, MAX_PATH );

	    /* ローダーに検索パスを設定 */
	    hr = g_pDMLoader->lpVtbl->SetSearchDirectory( g_pDMLoader, &GUID_DirectMusicAllTypes,
	                                                  wstrSearchPath, FALSE );
	    if( FAILED( hr ) )
	    {
	        rb_raise( eDXRubyError, "Set directory failed - SetSearchDirectory" );
	    }
	}
    g_iRefDM++;

	/* サウンドオブジェクト取得 */
    sound = DXRUBY_GET_STRUCT( Sound, obj );
    if( sound->pDMSegment )
    {
        g_iRefDM++;
        Sound_free( sound );
        g_iRefDM--;
        g_iRefAll--;
    }

	/* オーディオ・パス作成 */
	hr = g_pDMPerformance->lpVtbl->CreateStandardAudioPath( g_pDMPerformance,
		DMUS_APATH_DYNAMIC_STEREO,      /* パスの種類。 */
		64,                             /* パフォーマンス チャンネルの数。 */
		TRUE,                           /* ここでアクティブになる。 */
		&sound->pDMDefAudioPath );      /* オーディオパスを受け取るポインタ。 */

	if ( FAILED( hr ) )
	{
        rb_raise( eDXRubyError, "AudioPath set error - CreateStandardAudioPath" );
	}

    /* ファイルロード */
    if( rb_enc_get_index( vfilename ) != 0 )
    {
        vsjisstr = rb_str_export_to_enc( vfilename, g_enc_sys );
    }
    else
    {
        vsjisstr = vfilename;
    }

    MultiByteToWideChar( CP_ACP, 0, RSTRING_PTR( vsjisstr ), -1, wstrFileName, MAX_PATH );
    hr = g_pDMLoader->lpVtbl->LoadObjectFromFile( g_pDMLoader, &CLSID_DirectMusicSegment,
                                                  &IID_IDirectMusicSegment8,
                                                  wstrFileName,
                                                  (LPVOID*)&sound->pDMSegment );
    if( FAILED( hr ) )
    {
        sound->pDMSegment = NULL;
        rb_raise( eDXRubyError, "Load error - LoadObjectFromFile" );
    }

    sound->start = 0;
    sound->loopstart = 0;
    sound->loopend = 0;

    /* MIDIの場合 */
    if( strstr( RSTRING_PTR( vsjisstr ), ".mid" ) != NULL )
    {
        hr = sound->pDMSegment->lpVtbl->SetParam( sound->pDMSegment, &GUID_StandardMIDIFile,
                                                  0xFFFFFFFF, 0, 0, NULL);
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Load error - SetParam" );
        }
        sound->loopcount = DMUS_SEG_REPEAT_INFINITE;
        sound->midwavflag = 0;
        /* ループ回数設定 */
        hr = sound->pDMSegment->lpVtbl->SetRepeats( sound->pDMSegment, sound->loopcount );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop count failed - SetRepeats" );
        }
    }
    else
    {
        sound->loopcount = 1;
        sound->midwavflag = 1;
    }

    /* バンドダウンロード */
    hr = sound->pDMSegment->lpVtbl->Download( sound->pDMSegment, (IUnknown* )sound->pDMDefAudioPath );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Band loading failed - Download" );
    }


    /* 音量設定 */
    hr = sound->pDMDefAudioPath->lpVtbl->SetVolume( sound->pDMDefAudioPath, 230 * 9600 / 255 - 9600 , 0 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set volume failed - SetVolume" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   開始位置を設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setStart( VALUE obj, VALUE vstart )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->start = NUM2INT( vstart );

    if( sound->midwavflag == 1 && sound->start > 0 )    /* wavの場合 */
    {
        hr = sound->pDMSegment->lpVtbl->SetLength( sound->pDMSegment, sound->start * DMUS_PPQ / 768 + 1 );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set start point failed - SetLength" );
        }
    }

    /* セグメント再生スタート位置設定 */
    hr = sound->pDMSegment->lpVtbl->SetStartPoint( sound->pDMSegment, sound->start * DMUS_PPQ / 768 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set start point failed - SetStartPoint" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   ループ開始位置を設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setLoopStart( VALUE obj, VALUE vloopstart )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->loopstart = NUM2INT( vloopstart );

    if( sound->midwavflag == 1 )
    {
        rb_raise( eDXRubyError, "Can not be set to Wav data - Sound_loopStart=" );
    }

    if( sound->loopstart <= sound->loopend )
    {
        /* ループ範囲設定 */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, sound->loopstart * DMUS_PPQ / 768
                                                                        , sound->loopend * DMUS_PPQ / 768 );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }
    else
    {
        /* ループ範囲設定 */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, 0, 0 );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }

    return obj;
}


/*--------------------------------------------------------------------
   ループ終了位置を設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setLoopEnd( VALUE obj, VALUE vloopend )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->loopend = NUM2INT( vloopend );

    if( sound->midwavflag == 1 )
    {
        rb_raise( eDXRubyError, "Can not be set to Wav data - Sound_loopEnd=" );
    }

    if( sound->loopstart <= sound->loopend )
    {
        /* ループ範囲設定 */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, sound->loopstart * DMUS_PPQ / 768
                                                                        , sound->loopend * DMUS_PPQ / 768 );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }
    else
    {
        /* ループ範囲設定 */
        hr = sound->pDMSegment->lpVtbl->SetLoopPoints( sound->pDMSegment, 0, 0 );

        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "Set loop points failed - SetLoopPoints" );
        }
    }


    return obj;
}


/*--------------------------------------------------------------------
   ループ回数を設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setLoopCount( VALUE obj, VALUE vloopcount )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );
    sound->loopcount = NUM2INT( vloopcount );

    /* ループ回数設定 */
    hr = sound->pDMSegment->lpVtbl->SetRepeats( sound->pDMSegment, sound->loopcount );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failed to set loop count - SetRepeats" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   音量を設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setVolume( int argc, VALUE *argv, VALUE obj )
{
    HRESULT hr;
    struct DXRubySound *sound;
    VALUE vvolume, vtime;
    int volume, time;

    rb_scan_args( argc, argv, "11", &vvolume, &vtime );

    time = vtime == Qnil ? 0 : NUM2INT( vtime );
    volume = NUM2INT( vvolume ) > 255 ? 255 : NUM2INT( vvolume );
    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    /* 音量設定 */
    hr = sound->pDMDefAudioPath->lpVtbl->SetVolume( sound->pDMDefAudioPath, volume * 9600 / 255 - 9600, time );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Set volume error - SetVolume" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   パンを取得する
 ---------------------------------------------------------------------*/
static VALUE Sound_getPan( VALUE self )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;
    long result;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - GetPan" );
    }

    /* パン取得 */
    hr = pDS3DBuffer->lpVtbl->GetPan( pDS3DBuffer, &result );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "pan get error - GetPan" );
    }

    RELEASE( pDS3DBuffer );

    return INT2NUM( result );
}


/*--------------------------------------------------------------------
   パンを設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setPan( VALUE self, VALUE vpan )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - SetPan" );
    }

    /* パン設定 */
    hr = pDS3DBuffer->lpVtbl->SetPan( pDS3DBuffer, NUM2INT( vpan ) );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "pan setting error - SetPan" );
    }

    RELEASE( pDS3DBuffer );

    return self;
}


/*--------------------------------------------------------------------
   周波数を取得する
 ---------------------------------------------------------------------*/
static VALUE Sound_getFrequency( VALUE self )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;
    DWORD result;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - GetPan" );
    }

    /* 周波数取得 */
    hr = pDS3DBuffer->lpVtbl->GetFrequency( pDS3DBuffer, &result );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "frequency get error - getFrequency" );
    }

    RELEASE( pDS3DBuffer );

    return UINT2NUM( result );
}


/*--------------------------------------------------------------------
   周波数を設定する
 ---------------------------------------------------------------------*/
static VALUE Sound_setFrequency( VALUE self, VALUE vfrequency )
{
    HRESULT hr;
    struct DXRubySound *sound;
    IDirectSoundBuffer8* pDS3DBuffer;

    sound = DXRUBY_GET_STRUCT( Sound, self );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    hr = sound->pDMDefAudioPath->lpVtbl->GetObjectInPath( sound->pDMDefAudioPath, DMUS_PCHANNEL_ALL, DMUS_PATH_BUFFER, 0, &GUID_NULL, 0, &IID_IDirectSoundBuffer8, (LPVOID*) &pDS3DBuffer);

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "internal error - SetPan" );
    }

    /* 周波数設定 */
    hr = pDS3DBuffer->lpVtbl->SetFrequency( pDS3DBuffer, NUM2UINT( vfrequency ) );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "frequency setting error - setFrequency" );
    }

    RELEASE( pDS3DBuffer );

    return self;
}


/*--------------------------------------------------------------------
   音を鳴らす
 ---------------------------------------------------------------------*/
static VALUE Sound_play( VALUE obj )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    /* 再生 */
    if( sound->midwavflag == 0 )
    {
        hr = g_pDMPerformance->lpVtbl->PlaySegmentEx( g_pDMPerformance, (IUnknown* )sound->pDMSegment, NULL, NULL,
                                                      DMUS_SEGF_CONTROL, 0, NULL, NULL, (IUnknown* )sound->pDMDefAudioPath );
    }
    else
    {
        hr = g_pDMPerformance->lpVtbl->PlaySegmentEx( g_pDMPerformance, (IUnknown* )sound->pDMSegment, NULL, NULL,
                                                      DMUS_SEGF_SECONDARY, 0, NULL, NULL, (IUnknown* )sound->pDMDefAudioPath );
    }
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - PlaySegmentEx" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   音を止める
 ---------------------------------------------------------------------*/
static VALUE Sound_stop( VALUE obj )
{
    HRESULT hr;
    struct DXRubySound *sound;

    sound = DXRUBY_GET_STRUCT( Sound, obj );
    DXRUBY_CHECK_DISPOSE( sound, pDMSegment );

    /* 再生 */
    hr = g_pDMPerformance->lpVtbl->StopEx( g_pDMPerformance, (IUnknown* )sound->pDMSegment, 0, 0 );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound stop failed - StopEx" );
    }

    return obj;

}


/*********************************************************************
 * SoundEffectクラス
 *
 * DirectSoundを使用して音を鳴らす。
 * とりあえず音を出そうと頑張っている。
 *********************************************************************/

/*--------------------------------------------------------------------
   参照されなくなったときにGCから呼ばれる関数
 ---------------------------------------------------------------------*/
static void SoundEffect_free( struct DXRubySoundEffect *soundeffect )
{
    /* サウンドバッファを開放 */
    RELEASE( soundeffect->pDSBuffer );

    g_iRefDS--;

    if( g_iRefDS == 0 )
    {
        RELEASE( g_pDSound );
    }

}

static void SoundEffect_release( struct DXRubySoundEffect *soundeffect )
{
    if( soundeffect->pDSBuffer )
    {
        SoundEffect_free( soundeffect );
    }
    free( soundeffect );
    soundeffect = NULL;

    g_iRefAll--;
    if( g_iRefAll == 0 )
    {
        CoUninitialize();
    }
}

#ifdef DXRUBY_USE_TYPEDDATA
const rb_data_type_t SoundEffect_data_type = {
    "SoundEffect",
    {
    0,
    SoundEffect_release,
    0,
    },
    NULL, NULL
};
#endif

/*--------------------------------------------------------------------
   SoundEffectクラスのdispose。
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_dispose( VALUE self )
{
    struct DXRubySoundEffect *soundeffect = DXRUBY_GET_STRUCT( SoundEffect, self );
    DXRUBY_CHECK_DISPOSE( soundeffect, pDSBuffer );
    SoundEffect_free( soundeffect );
    return self;
}

/*--------------------------------------------------------------------
   SoundEffectクラスのdisposed?。
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_check_disposed( VALUE self )
{
    if( DXRUBY_GET_STRUCT( SoundEffect, self )->pDSBuffer == NULL )
    {
        return Qtrue;
    }

    return Qfalse;
}

/*--------------------------------------------------------------------
   SoundEffectクラスのallocate。メモリを確保する為にinitialize前に呼ばれる。
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_allocate( VALUE klass )
{
    VALUE obj;
    struct DXRubySoundEffect *soundeffect;

    /* DXRubySoundEffectのメモリ取得＆SoundEffectオブジェクト生成 */
    soundeffect = malloc(sizeof(struct DXRubySoundEffect));
    if( soundeffect == NULL ) rb_raise( eDXRubyError, "Out of memory - SoundEffect_allocate" );
#ifdef DXRUBY_USE_TYPEDDATA
    obj = TypedData_Wrap_Struct( klass, &SoundEffect_data_type, soundeffect );
#else
    obj = Data_Wrap_Struct(klass, 0, SoundEffect_release, soundeffect);
#endif

    /* とりあえずサウンドオブジェクトはNULLにしておく */
    soundeffect->pDSBuffer = NULL;

    return obj;
}


static short calcwave(int type, double vol, double count, double p, double duty)
{
	switch( type )
	{
	case 1: /* サイン波 */
		return (short)((sin( (3.141592653589793115997963468544185161590576171875f * 2) * (double)count / (double)p )) * (double)vol * 128);
		break;
	case 2: /* ノコギリ波 */
		return (short)(((double)count / (double)p - 0.5) * (double)vol * 256);
		break;
	case 3: /* 三角波 */
		if( count < p / 4 )			/* 1/4 */
		{
			return (short)((double)count / ((double)p / 4) * (double)vol * 128);
		}
		else if( count < p / 2 )		/* 2/4 */
		{
			return (short)(((double)p / 2 - (double)count) / ((double)p / 4) * (double)vol * 128);
		}
		else if( count < p * 3 / 4 )	/* 3/4 */
		{
			return (short)(-((double)count - (double)p / 2)/ ((double)p / 4) * (double)vol * 128);
		}
		else											/* 最後 */
		{
			return (short)(-((double)p - (double)count) / ((double)p / 4) * (double)vol * 128);
		}
		break;
	case 0: /* 矩形波 */
	default: /* デフォルト */
		if( count < p * duty )	/* 前半 */
		{
			return (short)(vol * 128);
		}
		else									/* 後半 */
		{
		    return (short)(-vol * 128);
		}
		break;
	}
}


/*--------------------------------------------------------------------
   SoundEffectクラスのinitialize。波形を生成する。
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_initialize( int argc, VALUE *argv, VALUE obj )
{
    HRESULT hr;
    struct DXRubySoundEffect *soundeffect;
    DSBUFFERDESC desc;
    WAVEFORMATEX pcmwf;
    int i;
    short *pointer, *pointer2;
    DWORD size, size2;
    VALUE vf;
    double count, duty = 0.5;
    double vol;
    double f;
    VALUE vsize, vtype, vresolution;
    int type, resolution;

    g_iRefAll++;

    rb_scan_args( argc, argv, "12", &vsize, &vtype, &vresolution );

    type       = vtype       == Qnil ? 0    : NUM2INT( vtype );
    resolution = vresolution == Qnil ? 1000 : (NUM2INT( vresolution ) > 44100 ? 44100 : NUM2INT( vresolution ));

    /* DirectSoundオブジェクトの作成 */
    if( g_iRefDS == 0 )
    {
        hr = DirectSoundCreate8( &DSDEVID_DefaultPlayback, &g_pDSound, NULL );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectSound initialize error - DirectSoundCreate8" );
        }

        hr = g_pDSound->lpVtbl->SetCooperativeLevel( g_pDSound, g_hWnd, DSSCL_PRIORITY );
        if( FAILED( hr ) )
        {
            rb_raise( eDXRubyError, "DirectSound initialize error - SetCooperativeLevel" );
        }
    }
    g_iRefDS++;

    /* サウンドオブジェクト作成 */
    soundeffect = DXRUBY_GET_STRUCT( SoundEffect, obj );
    if( soundeffect->pDSBuffer )
    {
        g_iRefDS++;
        SoundEffect_free( soundeffect );
        g_iRefDS--;
        g_iRefAll++;
    }

    /* サウンドバッファ作成 */
    pcmwf.wFormatTag = WAVE_FORMAT_PCM;
    pcmwf.nChannels = 1;
    pcmwf.nSamplesPerSec = 44100;
    pcmwf.wBitsPerSample = 16;
    pcmwf.nBlockAlign = pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
    pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
    pcmwf.cbSize = 0;

    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_GLOBALFOCUS;
#ifdef DXRUBY15
    if( TYPE( vsize ) == T_ARRAY )
    {
        desc.dwBufferBytes = RARRAY_LEN( vsize ) * (pcmwf.nChannels * pcmwf.wBitsPerSample / 8);
    }
    else
    {
#endif
        desc.dwBufferBytes = pcmwf.nAvgBytesPerSec / 100 * NUM2INT(vsize) / 10;
#ifdef DXRUBY15
    }
#endif
    desc.dwReserved = 0;
    desc.lpwfxFormat = &pcmwf;
    desc.guid3DAlgorithm = DS3DALG_DEFAULT;

    hr = g_pDSound->lpVtbl->CreateSoundBuffer( g_pDSound, &desc, &soundeffect->pDSBuffer, NULL );

    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to create the SoundBuffer - CreateSoundBuffer" );
    }

    /* ロック */
    hr = soundeffect->pDSBuffer->lpVtbl->Lock( soundeffect->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    if( TYPE( vsize ) == T_ARRAY )
    {
        /* 音バッファ生成 */
        for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
        {
            double tmp = NUM2DBL( RARRAY_AREF( vsize, i ) );
            if( tmp < 0.0 )
            {
                if( tmp < -1.0 ) tmp = -1.0;
                pointer[i] = (short)(tmp * 32768.0);
            }
            else
            {
                if( tmp > 1.0 ) tmp = 1.0;
                pointer[i] = (short)(tmp * 32767.0);
            }
        }
    }
    else
    {
        /* 音バッファ初期化 */
        for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
        {
            pointer[i] = 0;
        }

        count = 0;

        /* 波形生成 */
        for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
        {
            /* 指定時間単位でブロックを呼び出す */
            if ( i % (pcmwf.nSamplesPerSec / resolution) == 0 )
            {
                vf = rb_yield( obj );
                if( TYPE( vf ) != T_ARRAY )
                {
                    soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
                    rb_raise(rb_eTypeError, "invalid value - SoundEffect_initialize");
                    break;
                }
                f = NUM2DBL( rb_ary_entry(vf, 0) );
                vol = NUM2DBL(rb_ary_entry(vf, 1));
                if( RARRAY_LEN( vf ) > 2 )
                {
                    duty = NUM2DBL(rb_ary_entry(vf, 2));
                }
                /* 最大/最低周波数と最大ボリュームの制限 */
                f = f > pcmwf.nSamplesPerSec / 2.0f ? pcmwf.nSamplesPerSec / 2.0f : f;
                f = f < 20 ? 20 : f;
                vol = vol > 255 ? 255 : vol;
                vol = vol < 0 ? 0 : vol;
            }
            count = count + f;
            if( count >= pcmwf.nSamplesPerSec )
            {
                count = count - pcmwf.nSamplesPerSec;
            }
            pointer[i] = calcwave(type, vol, count, pcmwf.nSamplesPerSec, duty);
        }
    }

    /* アンロック */
    hr = soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return obj;

}


/*--------------------------------------------------------------------
   波形を合成する。
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_add( int argc, VALUE *argv, VALUE obj )
{
    HRESULT hr;
    struct DXRubySoundEffect *soundeffect;
	DSBUFFERDESC desc;
	WAVEFORMATEX pcmwf;
	int i;
	short *pointer, *pointer2;
	DWORD size, size2;
	VALUE vf, vtype, vresolution;
	double count, duty = 0.5;
	double vol;
    double f;
	int type, resolution;
	int data;

	rb_scan_args( argc, argv, "02", &vtype, &vresolution );

	type       = vtype       == Qnil ? 0    : NUM2INT( vtype );
	resolution = vresolution == Qnil ? 1000 : (NUM2INT( vresolution ) > 44100 ? 44100 : NUM2INT( vresolution ));

    /* サウンドオブジェクト取得 */
    soundeffect = DXRUBY_GET_STRUCT( SoundEffect, obj );
    DXRUBY_CHECK_DISPOSE( soundeffect, pDSBuffer );

	/* ロック */
	hr = soundeffect->pDSBuffer->lpVtbl->Lock( soundeffect->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

	pcmwf.nChannels = 1;
	pcmwf.nSamplesPerSec = 44100;
	pcmwf.wBitsPerSample = 16;
	pcmwf.nBlockAlign = pcmwf.nChannels * pcmwf.wBitsPerSample / 8;
	pcmwf.nAvgBytesPerSec = pcmwf.nSamplesPerSec * pcmwf.nBlockAlign;
	desc.dwBufferBytes = size;

	count = 0;

	/* 波形生成 */
	for( i = 0; i < desc.dwBufferBytes / (pcmwf.wBitsPerSample / 8); i++ )
	{
		/* 指定時間単位でブロックを呼び出す */
		if ( i % (pcmwf.nSamplesPerSec / resolution) == 0 )
		{
			vf = rb_yield( obj );
			if( TYPE( vf ) != T_ARRAY )
			{
				soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
			    rb_raise(rb_eTypeError, "invalid value - SoundEffect_add");
				break;
			}
			f = NUM2DBL( rb_ary_entry(vf, 0) );
			vol = NUM2DBL(rb_ary_entry(vf, 1));
            if( RARRAY_LEN( vf ) > 2 )
            {
                duty = NUM2DBL(rb_ary_entry(vf, 2));
            }
			/* 最大/最低周波数と最大ボリュームの制限 */
			f = f > pcmwf.nSamplesPerSec / 2.0f ? pcmwf.nSamplesPerSec / 2.0f : f;
			f = f < 20 ? 20 : f;
			vol = vol > 255 ? 255 : vol;
			vol = vol < 0 ? 0 : vol;
		}
		count = count + f;
		if( count >= pcmwf.nSamplesPerSec )
		{
			count = count - pcmwf.nSamplesPerSec;
		}

		data = calcwave(type, vol, count, pcmwf.nSamplesPerSec, duty);

		if( data + (int)pointer[i] > 32767 )
		{
			pointer[i] = 32767;
		}
		else if( data + (int)pointer[i] < -32768 )
		{
			pointer[i] = -32768;
		}
		else
		{
			pointer[i] += data;
		}
	}

	/* アンロック */
	hr = soundeffect->pDSBuffer->lpVtbl->Unlock( soundeffect->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return obj;

}


/*--------------------------------------------------------------------
   音を鳴らす
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_play( int argc, VALUE *argv, VALUE self )
{
    HRESULT hr;
    struct DXRubySoundEffect *se = DXRUBY_GET_STRUCT( SoundEffect, self );
    VALUE vflag;
    DXRUBY_CHECK_DISPOSE( se, pDSBuffer );

    rb_scan_args( argc, argv, "01", &vflag );

    /* とめる */
    hr = se->pDSBuffer->lpVtbl->Stop( se->pDSBuffer );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - SoundEffect_play" );
    }

    hr = se->pDSBuffer->lpVtbl->SetCurrentPosition( se->pDSBuffer, 0 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - SoundEffect_play" );
    }

    /* 再生 */
    hr = se->pDSBuffer->lpVtbl->Play( se->pDSBuffer, 0, 0, RTEST(vflag) ? DSBPLAY_LOOPING : 0 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Sound play failed - SoundEffect_play" );
    }
    return self;
}


/*--------------------------------------------------------------------
   音を止める
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_stop( VALUE obj )
{
    HRESULT hr;
    struct DXRubySoundEffect *soundeffect;

    soundeffect = DXRUBY_GET_STRUCT( SoundEffect, obj );
    DXRUBY_CHECK_DISPOSE( soundeffect, pDSBuffer );

    /* とめる */
    hr = soundeffect->pDSBuffer->lpVtbl->Stop( soundeffect->pDSBuffer );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "音の停止に失敗しました - Stop" );
    }

    return obj;
}


/*--------------------------------------------------------------------
   wavファイル出力
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_save( VALUE self, VALUE vfilename )
{
    HRESULT hr;
    HANDLE hfile;
    short *pointer, *pointer2;
    DWORD size, size2;
    DWORD tmpl;
    WORD tmps;
    DWORD wsize;
    struct DXRubySoundEffect *se = DXRUBY_GET_STRUCT( SoundEffect, self );

    DXRUBY_CHECK_DISPOSE( se, pDSBuffer );

    /* ロック */
    hr = se->pDSBuffer->lpVtbl->Lock( se->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    hfile = CreateFile( RSTRING_PTR( vfilename ), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if( hfile == INVALID_HANDLE_VALUE )
    {
        rb_raise( eDXRubyError, "Write failure - open" );
    }

    if( !WriteFile( hfile, "RIFF", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = size + 44 - 8;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    if( !WriteFile( hfile, "WAVE", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    if( !WriteFile( hfile, "fmt ", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = 16;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 1;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 1;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = 44100;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = 44100*2;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 2;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmps = 16;
    if( !WriteFile( hfile, &tmps, 2, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );

    if( !WriteFile( hfile, "data", 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    tmpl = size;
    if( !WriteFile( hfile, &tmpl, 4, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );
    if( !WriteFile( hfile, pointer, size, &wsize, NULL ) ) rb_raise( eDXRubyError, "Write failure - write" );

    CloseHandle( hfile );

    /* アンロック */
    hr = se->pDSBuffer->lpVtbl->Unlock( se->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return self;
}


/*--------------------------------------------------------------------
   配列化
 ---------------------------------------------------------------------*/
static VALUE SoundEffect_to_a( VALUE self )
{
    HRESULT hr;
    short *pointer, *pointer2;
    DWORD size, size2;
    struct DXRubySoundEffect *se = DXRUBY_GET_STRUCT( SoundEffect, self );
    VALUE ary;
    int i;

    DXRUBY_CHECK_DISPOSE( se, pDSBuffer );

    /* ロック */
    hr = se->pDSBuffer->lpVtbl->Lock( se->pDSBuffer, 0, 0, &pointer, &size, &pointer2, &size2, DSBLOCK_ENTIREBUFFER );
    if( FAILED( hr ) || size2 != 0 )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    ary = rb_ary_new2( size / 2 );
    for( i = 0; i < size / 2; i++ )
    {
        double tmp;
        if( pointer[i] < 0 )
        {
            tmp = pointer[i] / 32768.0;
        }
        else
        {
            tmp = pointer[i] / 32767.0;
        }
        rb_ary_push( ary, rb_float_new( tmp ) );
    }

    /* アンロック */
    hr = se->pDSBuffer->lpVtbl->Unlock( se->pDSBuffer, pointer, size, pointer2, size2 );
    if( FAILED( hr ) )
    {
        rb_raise( eDXRubyError, "Failure to lock the SoundBuffer - Lock" );
    }

    return ary;
}


void Init_dxruby_Sound( void )
{
    /* Soundクラス定義 */
    cSound = rb_define_class_under( mDXRuby, "Sound", rb_cObject );
    rb_define_singleton_method( cSound, "load_from_memory", Sound_load_from_memory, 2 );
    rb_define_singleton_method( cSound, "loadFromMemory", Sound_load_from_memory, 2 );

    /* Soundクラスにメソッド登録*/
    rb_define_private_method( cSound, "initialize"   , Sound_initialize   , 1 );
    rb_define_method( cSound, "dispose"      , Sound_dispose   , 0 );
    rb_define_method( cSound, "disposed?"    , Sound_check_disposed, 0 );
    rb_define_method( cSound, "play"         , Sound_play      , 0 );
    rb_define_method( cSound, "stop"         , Sound_stop         , 0 );
    rb_define_method( cSound, "set_volume"   , Sound_setVolume    , -1 );
    rb_define_method( cSound, "setVolume"    , Sound_setVolume    , -1 );
    rb_define_method( cSound, "pan"          , Sound_getPan       , 0 );
    rb_define_method( cSound, "pan="         , Sound_setPan       , 1 );
    rb_define_method( cSound, "frequency"    , Sound_getFrequency , 0 );
    rb_define_method( cSound, "frequency="   , Sound_setFrequency , 1 );
    rb_define_method( cSound, "start="       , Sound_setStart     , 1 );
    rb_define_method( cSound, "loop_start="  , Sound_setLoopStart , 1 );
    rb_define_method( cSound, "loopStart="   , Sound_setLoopStart , 1 );
    rb_define_method( cSound, "loop_end="    , Sound_setLoopEnd   , 1 );
    rb_define_method( cSound, "loopEnd="     , Sound_setLoopEnd   , 1 );
    rb_define_method( cSound, "loop_count="  , Sound_setLoopCount , 1 );
    rb_define_method( cSound, "loopCount="   , Sound_setLoopCount , 1 );

    /* Soundオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cSound, Sound_allocate );


    /* SoundEffectクラス定義 */
    cSoundEffect = rb_define_class_under( mDXRuby, "SoundEffect", rb_cObject );

    /* SoundEffectクラスにメソッド登録*/
    rb_define_private_method( cSoundEffect, "initialize", SoundEffect_initialize, -1 );
    rb_define_method( cSoundEffect, "dispose"   , SoundEffect_dispose   , 0 );
    rb_define_method( cSoundEffect, "disposed?" , SoundEffect_check_disposed, 0 );
    rb_define_method( cSoundEffect, "play"      , SoundEffect_play      , -1 );
    rb_define_method( cSoundEffect, "stop"      , SoundEffect_stop      , 0 );
    rb_define_method( cSoundEffect, "add"       , SoundEffect_add       , -1 );
    rb_define_method( cSoundEffect, "save"      , SoundEffect_save      , 1 );
#ifdef DXRUBY15
    rb_define_method( cSoundEffect, "to_a"      , SoundEffect_to_a      , 0 );
#endif
    /* SoundEffectオブジェクトを生成した時にinitializeの前に呼ばれるメモリ割り当て関数登録 */
    rb_define_alloc_func( cSoundEffect, SoundEffect_allocate );

    rb_define_const( mDXRuby, "WAVE_SIN"     , INT2FIX(WAVE_SIN) );
    rb_define_const( mDXRuby, "WAVE_SAW"     , INT2FIX(WAVE_SAW) );
    rb_define_const( mDXRuby, "WAVE_TRI"     , INT2FIX(WAVE_TRI) );
    rb_define_const( mDXRuby, "WAVE_RECT"    , INT2FIX(WAVE_RECT) );

    rb_define_const( mDXRuby, "TYPE_MIDI"    , INT2FIX(0) );
    rb_define_const( mDXRuby, "TYPE_WAV"     , INT2FIX(1) );
}

