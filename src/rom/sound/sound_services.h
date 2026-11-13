/* src/rom/sound/sound_services.h: the game's services to the sound library (MUSYX_DATA.md 8.1). The library calls
 * nothing of the OS itself: sound_set_services installs these hooks, the game's audio_driver_services, which the
 * library's files call through sound_services. include/game/types.h declares the same record for the game's C. */
#ifndef SOUND_SERVICES_H
#define SOUND_SERVICES_H

typedef struct SoundServices {
    void (*submit_task)(OSTask *task);                       /* run an audio task (musyx_submit_audio_task) */
    void (*wait_task)(void);                                 /* wait for the last one */
    void (*add_callback)(void (*callback)(void));            /* call callback every retrace */
    void (*remove_callback)(void (*callback)(void));         /* ... no longer */
    void (*start_dma)(void *dst, unsigned short size, void *src);   /* a sample DMA from the cartridge */
    unsigned int (*translate_address)(unsigned int address);  /* a sample's cartridge address */
    void *(*allocate)(unsigned int size, unsigned int mode);  /* memory from the game's heap */
    void (*release)(void *allocation);                       /* ... back */
} SoundServices;

extern SoundServices sound_services;

#endif
