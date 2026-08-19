#include "harness/audio.h"
#include "harness/config.h"
#include "harness/os.h"
#include "harness/trace.h"
#include "common/globvars.h"
#define WARPUP
#ifdef WARPUP
#pragma pack(push,2)
#endif
#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>
#include <proto/ahi.h>
#include <devices/ahi.h>

//cdplayer.library
#include <libraries/cdplayer.h>
#include <proto/cdplayer.h>
#include <stdbool.h>

#ifndef IOERR_SUCCESS
#define IOERR_SUCCESS 0

struct IOStdReq *CD_Request;
struct MsgPort *CD_Port;
struct Library *CDPlayerBase;
struct CD_TOC table;
struct CD_Time cd_time;	/* Time table */
struct CD_Volume vol;	/* Volume */
struct CD_Info info;	/* Device information */
bool CD_Active = FALSE;

STRPTR device = "scsi.device"; // Nazwa urządzenia
ULONG unit = 0;
//cdplayer.library

#endif
#ifdef WARPUP
#pragma pack(pop)
#endif

#include <clib/debug_protos.h>

#ifndef WARPUP
#include <SDI_compiler.h>
#include <SDI_hook.h>
#endif

#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

bool MusicFileIsPlaying = TRUE;
int music_channel = 0;

typedef struct tMiniaudio_sample {
    int init_volume;
    int init_pan;
    int init_new_rate;
    void *data;
    int channel;
    int sample_size;
    int channels;
} tMiniaudio_sample;

typedef struct tMiniaudio_stream {
    int sample_rate;
    int type;
    int position;
    char *buffer;
    int length;
    int channel;
} tMiniaudio_stream;

static tMiniaudio_sample* music_sample = NULL;

static bool UseCarmageddonMusic(void) {
    return harness_game_info.mode == eGame_splatpack
        || harness_game_info.mode == eGame_splatpack_demo
        || harness_game_info.mode == eGame_splatpack_xmas_demo;
}

static void MakeMusicTrackPath(char* path, int track) {
    /* AmigaDOS uses a leading slash to address the parent directory.
     * Splat Pack is launched from CARSPLAT, next to CARMA. */
    sprintf(path, UseCarmageddonMusic()
        ? "/CARMA/MUSIC/Track%02d.pcm"
        : "MUSIC/Track%02d.pcm", track);
}

#define MAX_CHANNELS (32)
enum {CHANNEL_STOPPED, CHANNEL_STARTED, CHANNEL_PLAYING, CHANNEL_LOOPING};

static WORD ChannelPlaying[MAX_CHANNELS];
static int channel_count = 0;
#ifdef WARPUP
static ULONG ChannelPlayCount[MAX_CHANNELS] = {0};
#endif

struct Library *AHIBase = NULL;

static struct MsgPort *AHImp = NULL;
static struct AHIRequest *AHIio = NULL;
static struct AHIAudioCtrl *actrl = NULL;
static BYTE AHIDevice = -1;
static ULONG audioID = AHI_DEFAULT_ID;

#ifdef WARPUP
UWORD SoundFunc[] =
{
    /*
        ULONG *channelPlayCount = actrl->ahiac_UserData;
        channelPlayCount[smsg->ahism_Channel]++;
    */
    0x4280,           //    clr.l d0
    0x3011,           //    move.w (a1),d0
    0xe588,           //    lsl.l #2,d0
    0xd092,           //    add.l (a2),d0
    0x2040,           //    movea.l d0,a0
    0x5290,           //    addq.l #1,(a0)
    //0x4280,           //  clr.l d0
    0x4e75,           //    rts
};
struct Hook SoundHook = {{NULL, NULL}, (HOOKFUNC)SoundFunc, NULL, NULL};
#else
HOOKPROTO(SoundFunc, ULONG, struct AHIAudioCtrl *actrl, struct AHISoundMessage *smsg)
{
    int channel = smsg->ahism_Channel;
    UWORD status = ChannelPlaying[channel];
    if (status == CHANNEL_STARTED) {
        ChannelPlaying[channel] = CHANNEL_PLAYING;
    } else if (status == CHANNEL_PLAYING) {
        ChannelPlaying[channel] = CHANNEL_STOPPED;
    }
    LOG_WARN(" %s:%ld ChannelPlaying[%ld] = %ld\n", __FUNCTION__, __LINE__, channel, ChannelPlaying[channel]);
    return 0;
}
MakeHook(SoundHook, SoundFunc);
#endif

tAudioBackend_error_code AudioBackend_Init(void) {
        // Resetuj channel_count
    channel_count = 0;

    // Resetuj tablicę ChannelPlaying
    for (int i = 0; i < MAX_CHANNELS; i++) {
        ChannelPlaying[i] = CHANNEL_STOPPED;
    }
    if ((AHImp = CreateMsgPort())) {
        if ((AHIio = (struct AHIRequest *)CreateIORequest(AHImp, sizeof(struct AHIRequest)))) {
            AHIio->ahir_Version = 4;
            if (!(AHIDevice = OpenDevice((STRPTR)AHINAME, AHI_NO_UNIT, (struct IORequest *)AHIio, 0))) {

                AHIBase = (struct Library *)AHIio->ahir_Std.io_Device;

                // query the Music Unit mode
                ULONG realtime, stereo, volume, panning, bits, channels;
                struct TagItem queryTags[] =
                {
                    {AHIDB_AudioID, (ULONG)&audioID},
                    {AHIDB_Realtime, (ULONG)&realtime},
                    {AHIDB_Stereo, (ULONG)&stereo},
                    {AHIDB_Volume, (ULONG)&volume},
                    {AHIDB_Panning, (ULONG)&panning},
                    {AHIDB_Bits, (ULONG)&bits},
                    {AHIDB_MaxChannels, (ULONG)&channels},
                    {TAG_DONE, 0}
                };
                AHI_GetAudioAttrsA(AHI_DEFAULT_ID, NULL, queryTags);

                struct TagItem filterTags[] =
                {
                    {AHIDB_AudioID, audioID},
                    {AHIDB_Realtime, TRUE},
                    {AHIDB_Stereo, TRUE},
                    {AHIDB_Volume, TRUE},
                    {AHIDB_Panning, TRUE},
                    {AHIDB_HiFi, FALSE},//TRUE},
                    {AHIDB_Bits, 8},
                    {AHIDB_MaxChannels, MAX_CHANNELS},
                    {AHIB_Dizzy, (ULONG)&filterTags[1]}, // skips AHIDB_AudioID
                    {TAG_DONE, 0}
                };

                if (!realtime || (stereo && !panning) || !volume || bits < 8 || channels < MAX_CHANNELS) {
                    audioID = AHI_BestAudioIDA(filterTags);
                    LOG_INFO(" %s best id %08lx\n", __FUNCTION__, audioID);
                }

                char namebuf[64];
                AHI_GetAudioAttrs(audioID, NULL,
                    AHIDB_BufferLen, sizeof(namebuf),
                    AHIDB_Name, (ULONG)namebuf,
                    TAG_END);

                LOG_INFO(" using AHI mode: %s\n", namebuf);
                LOG_INFO(" AHI Audio Configuration: ID=%ld, Stereo=%ld, Bits=%ld, Channels=%ld\n", audioID, stereo, bits, channels);
                actrl = AHI_AllocAudio(
                    AHIA_AudioID, audioID,
                    //AHIA_MixFreq, SOUND_SAMPLERATE,
                    AHIA_Channels, MAX_CHANNELS,
                    AHIA_Sounds, MAX_CHANNELS,
#ifdef WARPUP
                    AHIA_UserData, (ULONG)ChannelPlayCount,
#endif
                    AHIA_SoundFunc, (ULONG)&SoundHook,
                    TAG_DONE);

                if (actrl) {
                    ULONG r = (int)pow(2, (int)log2(MAX_CHANNELS));
                    struct AHIEffMasterVolume vol = {
                        AHIET_MASTERVOLUME,
                        r * 0x10000
                    };
                    AHI_SetEffect(&vol, actrl);

                    AHI_ControlAudio(actrl, AHIC_Play, TRUE, TAG_END);

                    return eAB_success;
                } else {
                     actrl = NULL;
                }

                CloseDevice((struct IORequest *)AHIio);
                AHIDevice = -1;
                AHIBase = NULL;
            }
            DeleteIORequest((struct IORequest *)AHIio);
            AHIio = NULL;
        }
        DeleteMsgPort(AHImp);
        AHImp = NULL;
    }
    return eAB_error;
}

void AudioBackend_UnInit(void) {
    if (actrl) {
        AHI_ControlAudio(actrl, AHIC_Play, FALSE, TAG_END);
        struct AHIEffMasterVolume vol = {
            AHIET_MASTERVOLUME | AHIET_CANCEL,
            0x10000
        };
        AHI_SetEffect(&vol, actrl);
        AHI_FreeAudio(actrl);
        actrl = NULL;
    }

    if (AHIDevice) {
        CloseDevice((struct IORequest *)AHIio);
        AHIDevice = -1;
        AHIBase = NULL;
    }

    if (AHIio) {
        DeleteIORequest((struct IORequest *)AHIio);
        AHIio = NULL;
    }

    if (AHImp) {
        DeleteMsgPort(AHImp);
        AHImp = NULL;
    }
}

void* AudioBackend_AllocateSampleTypeStruct(void) {
    // Check if AHI is initialized
    if (!actrl) {
        LOG_WARN("Cannot allocate sample - AHI not initialized");
        return NULL;
    }
    int channel = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (ChannelPlaying[i] == CHANNEL_STOPPED) {
            channel = i;
            break;
        }
    }
    if (channel == -1) {
        LOG_WARN("No free channels available for sample");
        return NULL;
    }

    // Oznacz kanał jako zajęty
    ChannelPlaying[channel] = CHANNEL_STARTED;

    tMiniaudio_sample* sample_struct;
    sample_struct = malloc(sizeof(tMiniaudio_sample));
    if (sample_struct == NULL) {
        // Jeśli alokacja pamięci się nie powiodła, zresetuj status kanału
        ChannelPlaying[channel] = CHANNEL_STOPPED;
        return NULL;
    }
    memset(sample_struct, 0, sizeof(tMiniaudio_sample));
    sample_struct->init_volume = 0x10000;
    sample_struct->init_pan = 0x8000;
    sample_struct->init_new_rate = 0;
    sample_struct->data = NULL;
    sample_struct->channel = channel;
    sample_struct->sample_size = 8;
    sample_struct->channels = 1;
    return (void*)sample_struct;
}


static tAudioBackend_error_code AudioBackend_PlaySampleWithSize(void* type_struct_sample, int channels, void* data, int size, int rate, int loop, int sample_size) {
    tMiniaudio_sample* miniaudio = (tMiniaudio_sample*)type_struct_sample;
    int channel = (int)miniaudio->channel;
    LOG_TRACE(" %s channel %d channels %d data %p size %d rate %d loop %d sample_size %d\n", __FUNCTION__, channel, channels, data, size, rate, loop, sample_size);

    // TODO: this assumes that there's one dedicated sound per channel
    if (miniaudio->data != data) {
        //LOG_WARN(" %s channel %ld - data %lx -> %lx\n", __FUNCTION__, channel, miniaudio->data, data);
        miniaudio->data = data;

        struct AHISampleInfo sample;
        sample.ahisi_Address = data;

        if (channels == 2) {
            if (sample_size == 16) {
                sample.ahisi_Type = AHIST_S16S;
            } else {
                sample.ahisi_Type = AHIST_S8S;
            }
        } else {
            if (sample_size == 16) {
                sample.ahisi_Type = AHIST_M16S;
            } else {
                sample.ahisi_Type = AHIST_M8S;
            }
        }

        sample.ahisi_Length = size / AHI_SampleFrameSize(sample.ahisi_Type);

        AHI_LoadSound(channel, AHIST_SAMPLE, &sample, actrl);
    }

    miniaudio->channels = channels;
    miniaudio->sample_size = sample_size;

    ChannelPlaying[channel] = loop ? CHANNEL_LOOPING : CHANNEL_STARTED;
    LOG_TRACE(" %s:%ld ChannelPlaying[%ld] = %ld\n", __FUNCTION__, __LINE__, channel, ChannelPlaying[channel]);

#ifdef WARPUP
    ChannelPlayCount[channel] = 0;
    AHI_Play(actrl,
        AHIP_BeginChannel, channel,
        AHIP_Freq, rate,//miniaudio->init_new_rate,
        AHIP_Vol, miniaudio->init_volume,
        AHIP_Pan, miniaudio->init_pan,
        AHIP_Sound, channel,
        !loop ? AHIP_LoopSound : TAG_IGNORE, AHI_NOSOUND,
        AHIP_EndChannel, 0,
        TAG_END);
#else
    AHI_SetFreq(channel, rate, actrl, AHISF_IMM);
    AHI_SetVol(channel, miniaudio->init_volume, miniaudio->init_pan, actrl, AHISF_IMM);
    AHI_SetSound(channel, channel, 0, 0, actrl, AHISF_IMM);
    if (loop == 0) {
        AHI_SetSound(channel, AHI_NOSOUND, 0, 0, actrl, AHISF_NONE);
    }
#endif

    return eAB_success;
}

tAudioBackend_error_code AudioBackend_PlaySample(void* type_struct_sample, int channels, void* data, int size, int rate, int loop) {
    return AudioBackend_PlaySampleWithSize(type_struct_sample, channels, data, size, rate, loop, 8);
}

#ifdef WARPUP
static inline void AudioBackend_UpdateChannelPlaying(int channel) {
    if (ChannelPlaying[channel] != CHANNEL_LOOPING) {
        ULONG count = ChannelPlayCount[channel];
        if (count > 1) {
            ChannelPlaying[channel] = CHANNEL_STOPPED;
            LOG_TRACE(" Stopped channel %ld\n", channel);
        } else if (count > 0) {
            ChannelPlaying[channel] = CHANNEL_PLAYING;
            //LOG_TRACE3(" Playing channel %ld\n", channel);
        }
    }
}
#endif

int AudioBackend_SoundIsPlaying(void* type_struct_sample) {
    tMiniaudio_sample* miniaudio = (tMiniaudio_sample*)type_struct_sample;
    int channel = (int)miniaudio->channel;
    //LOG_TRACE3(" channel %ld\n", channel);
#ifdef WARPUP
    AudioBackend_UpdateChannelPlaying(channel);
#endif
    return ChannelPlaying[channel];
}

tAudioBackend_error_code AudioBackend_SetVolume(void* type_struct_sample, int volume) {
    tMiniaudio_sample* miniaudio = (tMiniaudio_sample*)type_struct_sample;
    int channel = (int)miniaudio->channel;
    // Konwersja volume (zakładamy, że jest w zakresie 0-100) na zakres 0-65535
    int new_volume = volume << 7;
    //LOG_WARN(" %s channel %d volume %d -> %d\n", __FUNCTION__, channel, volume, miniaudio->init_volume);
#ifdef WARPUP
    AudioBackend_UpdateChannelPlaying(channel);
#endif
    if (ChannelPlaying[channel] && miniaudio->init_volume != new_volume) {
        AHI_SetVol(channel, new_volume, miniaudio->init_pan, actrl, AHISF_IMM);
    }
    miniaudio->init_volume = new_volume;
    return eAB_success;
}

tAudioBackend_error_code AudioBackend_SetPan(void* type_struct_sample, int pan) {
    tMiniaudio_sample* miniaudio = (tMiniaudio_sample*)type_struct_sample;
    int channel = (int)miniaudio->channel;
    // Skalowanie pan (zakres -10000 do 10000) na zakres 0 - 65535
    int new_pan = (pan + 10000) * 3; // 0 - 20000 -> 0 - 60000

#ifdef WARPUP
    AudioBackend_UpdateChannelPlaying(channel);
#endif
    if (ChannelPlaying[channel] && miniaudio->init_pan != new_pan) {
        AHI_SetVol(channel, miniaudio->init_volume, new_pan, actrl, AHISF_IMM);
    }
    miniaudio->init_pan = new_pan;
    return eAB_success;
}

tAudioBackend_error_code AudioBackend_SetFrequency(void* type_struct_sample, int original_rate, int new_rate) {
    tMiniaudio_sample* miniaudio = (tMiniaudio_sample*)type_struct_sample;
    int channel = (int)miniaudio->channel;
#ifdef WARPUP
    AudioBackend_UpdateChannelPlaying(channel);
#endif
    if (ChannelPlaying[channel] && miniaudio->init_new_rate != new_rate) {
        AHI_SetFreq(channel, new_rate, actrl, AHISF_IMM);
    }
    miniaudio->init_new_rate = new_rate;
    return eAB_success;
}

tAudioBackend_error_code AudioBackend_SetVolumeSeparate(void* type_struct_sample, int left_volume, int right_volume) {
    return eAB_error;
}

tAudioBackend_error_code AudioBackend_StopSample(void* type_struct_sample) {
    tMiniaudio_sample* miniaudio = (tMiniaudio_sample*)type_struct_sample;
    int channel = (int)miniaudio->channel;
#ifdef WARPUP
    AudioBackend_UpdateChannelPlaying(channel);
#endif
    if (ChannelPlaying[channel]) {
        LOG_TRACE(" Playing channel %ld\n", channel);
        AHI_SetSound(channel, AHI_NOSOUND, 0, 0, actrl, AHISF_IMM);
    }
#ifdef WARPUP
    ChannelPlayCount[channel] = 0;
#endif
    ChannelPlaying[channel] = CHANNEL_STOPPED;
    LOG_TRACE(" Stopped channel %ld\n", channel);
    return eAB_success;
}

tAudioBackend_error_code AudioBackend_InitCDA(void) {
    char trackname[256];
    BPTR music_file;

    /* A file-based install does not need the optional physical-CD helper.
     * Avoid making AmigaDOS search every LIBS: assign component for a library
     * which will not be used anyway. */
    MakeMusicTrackPath(trackname, 2);
    music_file = Open((STRPTR)trackname, MODE_OLDFILE);
    if (music_file) {
        Close(music_file);
        CD_Active = FALSE;
        return eAB_success;
    }

    // Jednostka urządzenia
    // Open cdplayer.library
    if ((CDPlayerBase = OpenLibrary(CDPLAYERNAME, CDPLAYERVERSION)) == NULL) {
        LOG_WARN(" Can't open cdplayer.library using music files instead.");
        return eAB_success;
    }

    // Create message port
    if ((CD_Port = CreateMsgPort()) == NULL) {
        LOG_WARN(" Can't create message port.\n");
        CloseLibrary(CDPlayerBase);
        return eAB_success;
    }

    // Create IORequest
    if ((CD_Request = (struct IOStdReq *)CreateIORequest(CD_Port, sizeof(struct IOStdReq))) == NULL) {
        LOG_WARN(" Can't create IORequest.\n");
        DeleteMsgPort(CD_Port);
        CloseLibrary(CDPlayerBase);
        return eAB_success;
    }

    // Open device
    if (OpenDevice((STRPTR)device, unit, (struct IORequest *)CD_Request, 0) != IOERR_SUCCESS) {
        LOG_WARN(" Can't open device %s unit %ld.\n", device, unit);
        DeleteIORequest((struct IORequest *)CD_Request);
        DeleteMsgPort(CD_Port);
        CloseLibrary(CDPlayerBase);
        return eAB_success;
    }

    // Read TOC (Table of Contents)
    if (CDReadTOC(&table, CD_Request) == IOERR_SUCCESS) {
        LOG_WARN(" Found %ld tracks on the CD.\n", table.cdc_NumTracks);
        CD_Active = TRUE;
        return eAB_success;
    } else {
        LOG_WARN(" Can't find CD audio. Using music files instead.\n");
        return eAB_success;
    }
}

void AudioBackend_UnInitCDA(void) {
    if (!CD_Active) {
        if (music_sample) {
            AudioBackend_StopSample(music_sample);
            FreeVec(music_sample->data);
            music_sample = NULL;
        }
        return;
    }

    CloseDevice((struct IORequest *)CD_Request);
    DeleteIORequest((struct IORequest *)CD_Request);
    DeleteMsgPort(CD_Port);
    CloseLibrary(CDPlayerBase);
}

tAudioBackend_error_code AudioBackend_StopCDA(void) {
    if (!CD_Active) {
        if (music_sample) {
            AudioBackend_StopSample(music_sample);
            FreeVec(music_sample->data);
            music_sample = NULL;

            LOG_WARN(" Stopped music file.\n");
            MusicFileIsPlaying = FALSE;
        }
        return eAB_success;
    }
    CDStop(CD_Request);
    LOG_WARN(" Stopped CD audio.\n");
    return eAB_success;
}


unsigned char* audio_data;
tAudioBackend_error_code AudioBackend_PlayCDA(int track) {
    if (!CD_Active) {
        // Załaduj plik surowy PCM
        char trackname[256];
        MakeMusicTrackPath(trackname, track);

        // Otwórz plik
        BPTR file = Open(trackname, MODE_OLDFILE);
        if (!file) {
            LOG_WARN("Failed to open %s\n", trackname);
            return eAB_error;
        }

        // Pobierz rozmiar pliku
        Seek(file, 0, OFFSET_END);
        LONG file_size = Seek(file, 0, OFFSET_BEGINNING);

        // Alokuj pamięć na dane pliku
        audio_data = AllocVec(file_size, MEMF_FAST/*MEMF_PUBLIC*/ | MEMF_CLEAR);
        if (!audio_data) {
            Close(file);
            LOG_WARN("Failed to allocate memory for %s\n", trackname);
            return eAB_error;
        }

        // Odczytaj plik do pamięci
        LONG bytes_read = Read(file, audio_data, file_size);
        Close(file);
        if (bytes_read != file_size) {
            FreeVec(audio_data);
            LOG_WARN(" Failed to read %s\n", trackname);
            return eAB_error;
        }

        // Jeśli dane są w formacie Little Endian, wykonaj swap bajtów
        //SwapBytes16(audio_data, file_size);

        // Ustaw parametry audio
        int channels = 2; // Stereo
        int sample_size = 16; // 16-bit
        int sample_rate = 32000;//44100; // 44100 Hz

        // Utwórz strukturę tMiniaudio_sample
        if (music_sample) {
            // Zatrzymaj i zwolnij poprzedni sample
            AudioBackend_StopSample(music_sample);
            free(music_sample);
            music_sample = NULL;
        }

        music_sample = AudioBackend_AllocateSampleTypeStruct();
        if (!music_sample) {
            LOG_WARN(" Failed to allocate music sample structure\n");
            FreeVec(audio_data);
            return eAB_error;
        }
        LOG_TRACE(" Playing music file %s\n", trackname);
        // Odtwórz sample
        if (AudioBackend_PlaySampleWithSize(music_sample, channels, audio_data, file_size, sample_rate, 2, sample_size) != eAB_success) {
            LOG_WARN(" Failed to play music sample\n");
            FreeVec(audio_data);
            free(music_sample);
            music_sample = NULL;
            return eAB_error;
        }

        MusicFileIsPlaying = TRUE;
        return eAB_success;
        }
    /* Play CD track */
    if (CDPlay(track, table.cdc_NumTracks, CD_Request) == IOERR_SUCCESS) {
        LOG_WARN(" Playing CD track %d...\n", track);
        return eAB_success;
    } else {
        LOG_WARN(" Can't play CD track %d.\n", track);
        return eAB_error;
    }
}

int AudioBackend_CDAIsPlaying(void) {
    if (!CD_Active) {
        if (music_sample) {
            return AudioBackend_SoundIsPlaying(music_sample);
        }
        return 0;
    }
    return CDActive(CD_Request);
}

tAudioBackend_error_code AudioBackend_SetCDAVolume(int volume) {
    if (!CD_Active) {
        if (music_sample) {
            return AudioBackend_SetVolume(music_sample, volume);
        }
        return eAB_error;
    }

    // Pobierz aktualne ustawienia głośności
    if (CDGetVolume(&vol, CD_Request) != IOERR_SUCCESS) {
        LOG_WARN(" Cannot get volume information.\n");
        return eAB_error;
    }

    // Ustaw głośność na wszystkich kanałach
    vol.cdv_Chan0 = volume;
    vol.cdv_Chan1 = volume;
    vol.cdv_Chan2 = volume;
    vol.cdv_Chan3 = volume;

    // Zastosuj nowe ustawienia głośności
    if (CDSetVolume(&vol, CD_Request) != IOERR_SUCCESS) {
        LOG_WARN(" Cannot set volume.\n");
        return eAB_error;
    }

    LOG_WARN(" Volume set to %d on all channels.\n", volume);
    return eAB_success;
}

tAudioBackend_stream* AudioBackend_StreamOpen(int bit_depth, int channels, unsigned int sample_rate) {
    tMiniaudio_stream* new;

    if (!actrl) {
        return NULL;
    }

    // Wyszukaj wolny kanał
    int channel = -1;
    for (int i = 0; i < MAX_CHANNELS; i++) {
        if (ChannelPlaying[i] == CHANNEL_STOPPED) {
            channel = i;
            break;
        }
    }
    if (channel == -1) {
        LOG_WARN("Ran out of channels\n");
        return NULL;
    }

    // Oznacz kanał jako zajęty
    ChannelPlaying[channel] = CHANNEL_STARTED;

    new = malloc(sizeof(tMiniaudio_stream));
    if (!new) {
        return NULL;
    }
    new->sample_rate = sample_rate;
    new->channel = channel;
    new->position = 0;
    if (channels == 2) {
        if (bit_depth == 16) {
            new->type = AHIST_S16S;
        } else {
            new->type = AHIST_S8S;
        }
    } else {
        if (bit_depth == 16) {
            new->type = AHIST_M16S;
        } else {
            new->type = AHIST_M8S;
        }
    }
    new->buffer = NULL;
    new->length = 0;
    return new;
}

tAudioBackend_error_code AudioBackend_StreamWrite(tAudioBackend_stream* stream_handle, const unsigned char* data, unsigned long size) {

    if (!actrl) {
        return eAB_error;
    }
    tMiniaudio_stream* stream = stream_handle;
    if (!stream->buffer) {
        // first call, initialize the buffer
        stream->length = size + 3000;/// + 2000; // TODO is this enough slack?
        stream->buffer = calloc(1, stream->length);
        if (!stream->buffer) {
            LOG_WARN("out of memory, can't allocate %d bytes", stream->length);
            return eAB_error;
        }
        stream->position = size;

        int channel = (int)stream->channel;

        struct AHISampleInfo sample;
        sample.ahisi_Address = stream->buffer;
        sample.ahisi_Type = stream->type;
        sample.ahisi_Length = stream->length / AHI_SampleFrameSize(stream->type);
        AHI_LoadSound(channel, AHIST_DYNAMICSAMPLE, &sample, actrl);

        if (stream->type == AHIST_S16S || stream->type == AHIST_M16S) {
            memcpy(stream->buffer, data, size);
        } else {
            //LOG_WARN(" using AHIST_M8S\n");
            for (unsigned long i = 0; i < size; i++) {
                stream->buffer[i] = data[i] ^ 0x80;
            }
        }

        ChannelPlaying[channel] = CHANNEL_LOOPING;
        AHI_SetFreq(channel, stream->sample_rate, actrl, AHISF_IMM);
        AHI_SetVol(channel, 0x10000/2, 0x8000, actrl, AHISF_IMM);
        AHI_SetSound(channel, channel, 0, 0, actrl, AHISF_IMM);
    } else {
        char *src = (char *)data;
        int remaining = size;
        while (remaining > 0) {
            char *dest = &stream->buffer[stream->position];
            int len = stream->length - stream->position;
            if (remaining < len) {
                len = remaining;
            }
            remaining -= len;
            if (remaining > 0) {
                // wrap around
                stream->position = 0;
            } else {
                stream->position += len;
            }

            if (stream->type == AHIST_S16S || stream->type == AHIST_M16S) {
                memcpy(dest, src, len);
            } else {
                //LOG_WARN(" using AHIST_M8S\n");
                for (int i = 0; i < len; i++) {
                    *dest++ = *src++ ^ 0x80;
                }
            }
        }
    }
    return eAB_success;
}

tAudioBackend_error_code AudioBackend_StreamClose(tAudioBackend_stream* stream_handle) {
    tMiniaudio_stream* stream = stream_handle;
    int channel = (int)stream->channel;

    LOG_TRACE(" channel %ld\n", channel);

    AHI_SetSound(channel, AHI_NOSOUND, 0, 0, actrl, AHISF_IMM);
    ChannelPlaying[channel] = CHANNEL_STOPPED;

    if (stream->buffer) {
        free(stream->buffer);
        stream->buffer = NULL;
    }
    free(stream);
    return eAB_success;
}
